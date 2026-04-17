"""Reconstruct C++ class hierarchy + method inventory from MSVC-mangled exports.

Lives entirely off pefile — no IDA dependency, no external undname. Uses the
documented MSVC name-mangling grammar to pull out namespace chain, class name,
method kind, and a best-effort return-arg signature sketch. Gives a quick
bird's-eye view before IDA finishes deep analysis.

Output: D:/Project/DXSense/analysis/class_inventory.md  +  JSON sidecar.
"""
from __future__ import annotations
import json, re, sys, collections
from dataclasses import dataclass, asdict, field
from pathlib import Path
import pefile

TARGET = Path(sys.argv[1] if len(sys.argv) > 1
              else r"D:\Project\DXSense\analysis\neox_engine.dll")
OUT_DIR = Path(r"D:\Project\DXSense\analysis")
OUT_DIR.mkdir(parents=True, exist_ok=True)

# MSVC mangled form is well-documented:
#   ?<name>@<class>@<outer>@...@@<calling_conv><kind><args>Z
# For our purposes we only need: method name, class chain, and call-conv marker.
MANGLED_RE = re.compile(
    r'^\?'                                           # MSVC prefix
    r'(?P<name>[^@?][^@]*|\?[0-9A-Z])'               # method or ctor(?0) / dtor(?1) / op
    r'@'
    r'(?P<chain>(?:[^@]+@)+)'                        # namespace/class chain
    r'@'                                             # chain terminator
    r'(?P<sig>.*)$'
)

SPECIAL_METHODS = {
    '?0': '<ctor>',
    '?1': '<dtor>',
    '?2': 'operator new',
    '?3': 'operator delete',
    '?4': 'operator =',
    '?5': 'operator >>',
    '?6': 'operator <<',
    '?7': 'operator !',
    '?8': 'operator ==',
    '?9': 'operator !=',
    '?A': 'operator []',
    '?B': 'operator ()',
    '?C': 'operator ->',
    '?D': 'operator *',
    '?E': 'operator ++',
    '?F': 'operator --',
    '?G': 'operator -',
    '?H': 'operator +',
    '?I': 'operator &',
    '?J': 'operator ->*',
    '?K': 'operator /',
    '?L': 'operator %',
    '?M': 'operator <',
    '?N': 'operator <=',
    '?O': 'operator >',
    '?P': 'operator >=',
    '?Q': 'operator ,',
    '?R': 'operator ()',
    '?S': 'operator ~',
    '?T': 'operator ^',
    '?U': 'operator |',
    '?V': 'operator &&',
    '?W': 'operator ||',
    '?X': 'operator *=',
    '?Y': 'operator +=',
    '?Z': 'operator -=',
}

# Rough classifier from the first signature byte.
# Full MSVC grammar is more involved but this gets us 90% coverage.
KIND_FLAGS = {
    'Q': 'public',
    'R': 'public static',
    'S': 'public virtual',
    'A': 'private',
    'B': 'private static',
    'C': 'private virtual',
    'I': 'protected',
    'J': 'protected static',
    'K': 'protected virtual',
    'U': 'public virtual',
    'V': 'protected virtual',
    'W': 'private virtual',
    'Y': 'extern "C"',
}

PROTOBUF_METHODS = {
    'ByteSize', 'MergeFrom', 'CopyFrom', 'Clear', 'IsInitialized',
    'SerializeWithCachedSizes', 'GetCachedSize', 'New', 'Swap',
    'InternalSerialize', 'MergePartialFromCodedStream',
}

@dataclass
class Method:
    name:       str
    class_fqn:  str
    kind:       str              # public/virtual/...
    mangled:    str              = ''
    rva:        int              = 0

@dataclass
class ClassInfo:
    fqn:        str
    methods:    list             = field(default_factory=list)
    categories: set              = field(default_factory=set)

    def summary(self) -> dict:
        special = sum(1 for m in self.methods if m.name.startswith('<'))
        virtual = sum(1 for m in self.methods if 'virtual' in m.kind)
        return {
            'fqn': self.fqn,
            'method_count': len(self.methods),
            'virtual_count': virtual,
            'ctor_dtor': special,
            'categories': sorted(self.categories),
        }

def demangle_method(sym: str) -> Method | None:
    m = MANGLED_RE.match(sym)
    if not m:
        return None
    name_raw = m.group('name')
    chain    = m.group('chain').rstrip('@').split('@')
    sig      = m.group('sig')
    kind     = KIND_FLAGS.get(sig[:1], '?' + sig[:1])

    if name_raw.startswith('?'):
        name = SPECIAL_METHODS.get('?' + name_raw[1:2], f'<{name_raw}>')
    else:
        name = name_raw
    # Chain is emitted innermost→outermost in MSVC mangling; reverse for FQN.
    class_fqn = '::'.join(reversed(chain))
    return Method(name=name, class_fqn=class_fqn, kind=kind, mangled=sym)

def classify(cls: ClassInfo) -> None:
    method_names = {m.name for m in cls.methods}
    if method_names & PROTOBUF_METHODS:
        cls.categories.add('protobuf_message')
    if 'Execute' in method_names or 'Run' in method_names:
        cls.categories.add('command')
    if any(m.name in ('Subscribe', 'Publish', 'On', 'Fire') for m in cls.methods):
        cls.categories.add('event_source')
    if cls.fqn.startswith('AK::'):
        cls.categories.add('wwise_audio')
    if cls.fqn.startswith('mobile::server::'):
        cls.categories.add('server_proto')
    if cls.fqn.startswith('mobile::client::'):
        cls.categories.add('client_proto')
    if '::Graph' in cls.fqn or cls.fqn.endswith('Scene'):
        cls.categories.add('scene_graph')

def main() -> int:
    pe = pefile.PE(str(TARGET), fast_load=True)
    pe.parse_data_directories()

    classes: dict[str, ClassInfo] = {}
    unmangled = 0
    skipped   = 0
    name_to_rva = {}
    if hasattr(pe, 'DIRECTORY_ENTRY_EXPORT'):
        for s in pe.DIRECTORY_ENTRY_EXPORT.symbols:
            if not s.name: continue
            nm = s.name.decode('latin-1', 'replace')
            name_to_rva[nm] = s.address
            if not nm.startswith('?'):
                unmangled += 1
                continue
            meth = demangle_method(nm)
            if not meth:
                skipped += 1
                continue
            meth.rva = name_to_rva.get(nm, 0)
            cls = classes.setdefault(meth.class_fqn, ClassInfo(fqn=meth.class_fqn))
            cls.methods.append(meth)

    for cls in classes.values():
        classify(cls)

    # ---- Top N by method count -------------------------------------------
    ranked = sorted(classes.values(), key=lambda c: -len(c.methods))

    cat_counter = collections.Counter()
    for c in classes.values():
        for cat in c.categories or ['(uncategorized)']:
            cat_counter[cat] += 1

    # ---- Markdown report -------------------------------------------------
    md = OUT_DIR / 'class_inventory.md'
    with md.open('w', encoding='utf-8') as f:
        f.write(f"# Class inventory — {TARGET.name}\n\n")
        f.write(f"- Exports parsed as C++ methods: **{sum(len(c.methods) for c in classes.values())}**\n")
        f.write(f"- Unique classes: **{len(classes)}**\n")
        f.write(f"- C-style exports (not covered): {unmangled}\n")
        f.write(f"- Mangled but not yet parsable by our grammar: {skipped}\n\n")

        f.write("## Category breakdown\n\n| category | class count |\n|---|---:|\n")
        for cat, n in cat_counter.most_common():
            f.write(f"| {cat} | {n} |\n")

        f.write("\n## Top 40 classes by method count\n\n")
        f.write("| class | methods | virtual | ctor/dtor | categories |\n")
        f.write("|---|---:|---:|---:|---|\n")
        for c in ranked[:40]:
            s = c.summary()
            f.write(f"| `{s['fqn']}` | {s['method_count']} | {s['virtual_count']} | "
                    f"{s['ctor_dtor']} | {', '.join(s['categories']) or '—'} |\n")

        # Protobuf messages get their own appendix — most valuable section
        proto = [c for c in ranked if 'protobuf_message' in c.categories]
        f.write(f"\n## Protobuf messages ({len(proto)})\n\n")
        f.write("One per line, sorted by method count (proxy for schema complexity):\n\n")
        for c in proto:
            f.write(f"- `{c.fqn}` — {len(c.methods)} methods\n")

    # ---- JSON sidecar ----------------------------------------------------
    js = OUT_DIR / 'class_inventory.json'
    payload = {
        'target':          str(TARGET),
        'class_count':     len(classes),
        'method_total':    sum(len(c.methods) for c in classes.values()),
        'unmangled':       unmangled,
        'categories':      dict(cat_counter),
        'top_classes':     [c.summary() for c in ranked[:200]],
        'protobuf_messages': [{'fqn': c.fqn, 'methods': len(c.methods)}
                              for c in ranked if 'protobuf_message' in c.categories],
    }
    js.write_text(json.dumps(payload, indent=2, ensure_ascii=False), encoding='utf-8')

    print(f"wrote {md}")
    print(f"wrote {js}")
    print(f"classes={len(classes)}  methods={sum(len(c.methods) for c in classes.values())}  "
          f"protobuf={sum(1 for c in classes.values() if 'protobuf_message' in c.categories)}")
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
