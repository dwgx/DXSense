"""Extract .proto schemas by disassembling protobuf_AddDesc_* bootstrap funcs.

NeoX3 uses the legacy protobuf 2.x/3.0 registration scheme: one exported
`protobuf_AddDesc_<filename>_2eproto()` per generated .proto file. The body
loads a pointer to a serialized FileDescriptorProto blob and its length into
RCX/EDX, then calls `InternalAddGeneratedFile`. We find that
lea+mov pair with capstone, slice .rdata, and hand it to protobuf to parse.

Works without IDA, without debugging the game, and without pattern-matching
over the whole .rdata blob.
"""
from __future__ import annotations
import json, os, re, sys
from pathlib import Path
from dataclasses import dataclass
from typing import Optional

import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_64, CS_OP_IMM, CS_OP_MEM, CS_OP_REG, x86_const
from google.protobuf import descriptor_pb2
from google.protobuf.descriptor import FieldDescriptor

TARGET = Path(sys.argv[1] if len(sys.argv) > 1
              else r"D:\Project\DXSense\analysis\neox_engine.dll")
OUT_DIR = Path(r"D:\Project\DXSense\analysis\protos")
OUT_DIR.mkdir(parents=True, exist_ok=True)

# Regex to pick out the protobuf bootstrap export name + decode the mangled .proto filename.
ADDDESC_RE = re.compile(r'protobuf_AddDesc_(?P<name>[A-Za-z0-9_]+)_2eproto')

def decode_proto_name(mangled: str) -> str:
    """`client_5fgate_2eproto` -> `client_gate.proto` (MSVC escapes hex bytes as _XX)."""
    def rep(m): return chr(int(m.group(1), 16))
    return re.sub(r'_([0-9a-fA-F]{2})', rep, mangled) + '.proto'

# ----------------------------------------------------------------------------
# Disassembly: find the lea + mov pair that sets up (blob_ptr, blob_size) before
# InternalAddGeneratedFile. Capstone is at 5.0.7 which has the OP types we need.
# ----------------------------------------------------------------------------
@dataclass
class BlobRef:
    blob_va:    int           # virtual address of the serialized FileDescriptorProto
    size:       int           # number of bytes at blob_va
    detected_at: int          # RVA of the instruction where we resolved the lea

def collect_blob_candidates(pe: pefile.PE, func_rva: int, window: int = 0x200) -> list[BlobRef]:
    """Collect every plausible (ptr, size) setup that precedes a CALL, up to ret.

    MSVC may emit `mov edx, SIZE; lea rcx, PTR` OR `lea rcx, PTR; mov edx, SIZE`
    (or interleave with unrelated insns). We track the last observed value of
    each register and snapshot the pair on every CALL.
    """
    base = pe.OPTIONAL_HEADER.ImageBase
    code = pe.get_memory_mapped_image()[func_rva:func_rva + window]
    md = Cs(CS_ARCH_X86, CS_MODE_64)
    md.detail = True

    candidates: list[BlobRef] = []
    seen: set[tuple[int, int]] = set()
    last_rcx_ptr: Optional[int] = None
    last_edx_imm: Optional[int] = None

    for insn in md.disasm(code, base + func_rva):
        if insn.mnemonic == 'lea' and len(insn.operands) == 2:
            dst, src = insn.operands
            if (dst.type == CS_OP_REG and src.type == CS_OP_MEM
                    and src.mem.base == x86_const.X86_REG_RIP
                    and dst.reg == x86_const.X86_REG_RCX):
                last_rcx_ptr = insn.address + insn.size + src.mem.disp
        elif insn.mnemonic == 'mov' and len(insn.operands) == 2:
            dst, src = insn.operands
            if dst.type == CS_OP_REG and src.type == CS_OP_IMM:
                if dst.reg == x86_const.X86_REG_EDX:
                    last_edx_imm = src.imm & 0xFFFFFFFF
                elif dst.reg == x86_const.X86_REG_RCX:
                    last_rcx_ptr = src.imm   # immediate-to-rcx, unusual but safe
        elif insn.mnemonic == 'call':
            if last_rcx_ptr is not None and last_edx_imm is not None:
                size = last_edx_imm
                if 16 <= size <= 8 * 1024 * 1024:
                    key = (last_rcx_ptr, size)
                    if key not in seen:
                        seen.add(key)
                        candidates.append(BlobRef(blob_va=last_rcx_ptr, size=size,
                                                  detected_at=insn.address))
        elif insn.mnemonic == 'ret':
            break
    return candidates

def read_va(pe: pefile.PE, va: int, size: int) -> bytes:
    rva = va - pe.OPTIONAL_HEADER.ImageBase
    return pe.get_memory_mapped_image()[rva:rva + size]

# ----------------------------------------------------------------------------
# .proto serializer — minimal but correct for proto2 (which NeoX uses).
# ----------------------------------------------------------------------------
TYPE_NAMES = {
    FieldDescriptor.TYPE_DOUBLE:  'double', FieldDescriptor.TYPE_FLOAT:   'float',
    FieldDescriptor.TYPE_INT64:   'int64',  FieldDescriptor.TYPE_UINT64:  'uint64',
    FieldDescriptor.TYPE_INT32:   'int32',  FieldDescriptor.TYPE_UINT32:  'uint32',
    FieldDescriptor.TYPE_FIXED64: 'fixed64',FieldDescriptor.TYPE_FIXED32: 'fixed32',
    FieldDescriptor.TYPE_BOOL:    'bool',   FieldDescriptor.TYPE_STRING:  'string',
    FieldDescriptor.TYPE_BYTES:   'bytes',
    FieldDescriptor.TYPE_SFIXED32:'sfixed32',FieldDescriptor.TYPE_SFIXED64:'sfixed64',
    FieldDescriptor.TYPE_SINT32:  'sint32', FieldDescriptor.TYPE_SINT64:  'sint64',
}
LABEL_NAMES = {
    FieldDescriptor.LABEL_OPTIONAL: 'optional',
    FieldDescriptor.LABEL_REQUIRED: 'required',
    FieldDescriptor.LABEL_REPEATED: 'repeated',
}

def render_field(f, indent: str) -> str:
    ty = f.type_name.lstrip('.') if f.type_name else TYPE_NAMES.get(f.type, f'type_{f.type}')
    label = LABEL_NAMES.get(f.label, '')
    opts = []
    if f.default_value:
        opts.append(f'default = {f.default_value!r}' if f.type == FieldDescriptor.TYPE_STRING
                    else f'default = {f.default_value}')
    if f.options.packed:     opts.append('packed = true')
    if f.options.deprecated: opts.append('deprecated = true')
    opt_str = f' [{", ".join(opts)}]' if opts else ''
    return f"{indent}{label} {ty} {f.name} = {f.number}{opt_str};"

def render_enum(e, indent: str) -> str:
    out = [f'{indent}enum {e.name} {{']
    for v in e.value: out.append(f'{indent}  {v.name} = {v.number};')
    out.append(f'{indent}}}')
    return '\n'.join(out)

def render_message(m, indent: str = '') -> str:
    out = [f'{indent}message {m.name} {{']
    inner = indent + '  '
    for e in m.enum_type:    out.append(render_enum(e, inner))
    for nm in m.nested_type: out.append(render_message(nm, inner))
    for f in m.field:        out.append(render_field(f, inner))
    for r in m.oneof_decl:
        out.append(f'{inner}oneof {r.name} {{')
        for f in m.field:
            if f.HasField('oneof_index') and f.oneof_index == list(m.oneof_decl).index(r):
                out.append(render_field(f, inner + '  '))
        out.append(f'{inner}}}')
    out.append(f'{indent}}}')
    return '\n'.join(out)

def render_service(s, indent: str = '') -> str:
    out = [f'{indent}service {s.name} {{']
    for meth in s.method:
        out.append(f'{indent}  rpc {meth.name} ({meth.input_type.lstrip(".")}) '
                   f'returns ({meth.output_type.lstrip(".")});')
    out.append(f'{indent}}}')
    return '\n'.join(out)

def render_file(fd: descriptor_pb2.FileDescriptorProto) -> str:
    lines = []
    lines.append(f'syntax = "{fd.syntax or "proto2"}";')
    if fd.package: lines.append(f'package {fd.package};')
    lines.append('')
    for dep in fd.dependency: lines.append(f'import "{dep}";')
    if fd.dependency: lines.append('')
    for e in fd.enum_type:    lines.append(render_enum(e, ''))
    for m in fd.message_type: lines.append(render_message(m))
    for s in fd.service:      lines.append(render_service(s))
    return '\n'.join(lines) + '\n'

# ----------------------------------------------------------------------------
def main() -> int:
    pe = pefile.PE(str(TARGET))
    exports = pe.DIRECTORY_ENTRY_EXPORT.symbols

    targets = []
    for s in exports:
        if not s.name: continue
        nm = s.name.decode('latin-1', 'replace')
        m = ADDDESC_RE.search(nm)
        if m:
            proto_filename = decode_proto_name(m.group('name'))
            targets.append((proto_filename, s.address, nm))

    print(f"Found {len(targets)} protobuf_AddDesc_* bootstrap functions:")
    for pn, rva, mangled in targets:
        print(f"  {pn:<40}  RVA 0x{rva:08X}   ({mangled[:70]}...)")

    if not targets:
        print("No AddDesc functions — schema may use newer descriptor_table scheme.")
        return 1

    summary = []
    for proto_filename, rva, _ in targets:
        print(f"\n--- {proto_filename} (func RVA 0x{rva:X}) ---")
        candidates = collect_blob_candidates(pe, rva)
        if not candidates:
            print("  FAIL: no lea rcx/mov edx pair within first 0x200 bytes.")
            continue

        # Try each candidate; the first one that parses cleanly is the real
        # InternalAddGeneratedFile call. The earlier ones are typically
        # descriptor-pool preallocations or reserve() calls.
        picked: Optional[BlobRef] = None
        fd: Optional[descriptor_pb2.FileDescriptorProto] = None
        for cand in candidates:
            blob = read_va(pe, cand.blob_va, cand.size)
            f = descriptor_pb2.FileDescriptorProto()
            try:
                f.ParseFromString(blob)
                if f.name and f.name.endswith('.proto'):
                    picked, fd = cand, f
                    break
            except Exception:
                continue

        if not picked or not fd:
            print(f"  no candidate parsed ({len(candidates)} tried): "
                  + "; ".join(f"VA=0x{c.blob_va:X}/sz={c.size}" for c in candidates[:4]))
            continue
        ref = picked
        print(f"  blob VA=0x{ref.blob_va:X}  size={ref.size}  "
              f"(detected at 0x{ref.detected_at:X}, candidate {candidates.index(ref)+1}/{len(candidates)})")

        print(f"  parsed OK: package={fd.package!r} messages={len(fd.message_type)} "
              f"enums={len(fd.enum_type)} services={len(fd.service)}")

        # Write to disk, preserving original package structure.
        pkg_dir = OUT_DIR / (fd.package.replace('.', '/') if fd.package else '_nopkg')
        pkg_dir.mkdir(parents=True, exist_ok=True)
        out_path = pkg_dir / (Path(fd.name or proto_filename).name or proto_filename)
        out_path.write_text(render_file(fd), encoding='utf-8')
        print(f"  wrote {out_path}")

        # Count fields.
        total_fields = sum(len(m.field) for m in fd.message_type)
        summary.append({
            'proto':    fd.name or proto_filename,
            'package':  fd.package,
            'messages': len(fd.message_type),
            'enums':    len(fd.enum_type),
            'services': len(fd.service),
            'fields':   total_fields,
            'path':     str(out_path),
            'bytes':    ref.size,
        })

    # Wrap-up
    print("\n=== Summary ===")
    print(f"recovered: {len(summary)} protos, "
          f"{sum(s['messages'] for s in summary)} messages, "
          f"{sum(s['fields'] for s in summary)} fields, "
          f"{sum(s['services'] for s in summary)} services")
    for s in summary:
        print(f"  - {s['proto']:<40} pkg={s['package']:<25} "
              f"msg={s['messages']:>3} fld={s['fields']:>4} bytes={s['bytes']}")

    (OUT_DIR.parent / 'protos_index.json').write_text(
        json.dumps(summary, indent=2, ensure_ascii=False), encoding='utf-8')
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
