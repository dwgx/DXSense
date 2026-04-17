"""Deeper inspection: TLS callbacks, Detours tables, export class taxonomy."""
import sys, struct, re, collections
from pathlib import Path
import pefile

PATH = Path(sys.argv[1] if len(sys.argv) > 1
            else r"D:\WorkSpace\FeverGames\dwrg2\neox_engine.dll")

pe = pefile.PE(str(PATH))
base = pe.OPTIONAL_HEADER.ImageBase

# ---- TLS callbacks ---------------------------------------------------------
print("=== TLS callbacks ===")
if hasattr(pe, 'DIRECTORY_ENTRY_TLS') and pe.DIRECTORY_ENTRY_TLS:
    tls = pe.DIRECTORY_ENTRY_TLS.struct
    cb_addr = tls.AddressOfCallBacks
    print(f"  AddressOfCallBacks (VA): 0x{cb_addr:X}")
    print(f"  StartAddressOfRawData:   0x{tls.StartAddressOfRawData:X}")
    print(f"  EndAddressOfRawData:     0x{tls.EndAddressOfRawData:X}")
    cb_rva = cb_addr - base
    try:
        data = pe.get_data(cb_rva, 128)
        for i in range(0, len(data), 8):
            va = struct.unpack_from('<Q', data, i)[0]
            if va == 0: break
            print(f"    callback[{i//8}] = 0x{va:X}  (RVA 0x{va-base:X})")
    except Exception as e:
        print(f"  (could not read callback list: {e})")
else:
    print("  none")

# ---- Detour sections -------------------------------------------------------
print("\n=== Detours-style sections (.detourc/.detourd) ===")
for s in pe.sections:
    n = s.Name.rstrip(b'\x00').decode('latin-1','replace').lower()
    if n in ('.detourc', '.detourd'):
        print(f"  {n}: VA=0x{base+s.VirtualAddress:X}  vsize={s.Misc_VirtualSize}  rsize={s.SizeOfRawData}")
        raw = s.get_data()[:256]
        print(f"    first 128 bytes:")
        for i in range(0, min(128, len(raw)), 16):
            hexs = ' '.join(f'{b:02x}' for b in raw[i:i+16])
            asci = ''.join(chr(b) if 32<=b<127 else '.' for b in raw[i:i+16])
            print(f"      {i:04x}  {hexs:<48}  {asci}")

# ---- Export taxonomy -------------------------------------------------------
print("\n=== Export taxonomy (top-level namespaces) ===")
if hasattr(pe, 'DIRECTORY_ENTRY_EXPORT'):
    buckets = collections.Counter()
    samples = collections.defaultdict(list)
    pat_ns    = re.compile(r'^\?[^\?]*@([A-Za-z_][A-Za-z0-9_]*)@')
    pat_class = re.compile(r'^\?[^\?]*@([A-Za-z_][A-Za-z0-9_]*)@([A-Za-z_][A-Za-z0-9_]*)@')
    c_like = 0
    for s in pe.DIRECTORY_ENTRY_EXPORT.symbols:
        if not s.name: continue
        name = s.name.decode('latin-1','replace')
        m = pat_class.match(name) or pat_ns.match(name)
        if m:
            # Inner namespace shows up as the LAST group — walk chain.
            chain = re.findall(r'@([A-Za-z_][A-Za-z0-9_]*)', name)
            # last group before @@ is outermost namespace
            if chain:
                key = '::'.join(reversed(chain[:3]))
                buckets[key] += 1
                if len(samples[key]) < 3:
                    samples[key].append(name[:80])
        else:
            c_like += 1
    print(f"  C-style (non-mangled) exports: {c_like}")
    print(f"  namespace buckets (top 25):")
    for k, v in buckets.most_common(25):
        print(f"    {v:>5}  {k}")
        for nm in samples[k][:1]:
            print(f"           e.g. {nm}")

# ---- Version info / product name ------------------------------------------
print("\n=== Version info ===")
if hasattr(pe, 'FileInfo'):
    for fi in pe.FileInfo or []:
        for e in fi:
            if e.Key == b'StringFileInfo':
                for st in e.StringTable:
                    for k, v in st.entries.items():
                        print(f"  {k.decode('latin-1','replace'):20} = {v.decode('latin-1','replace')}")
