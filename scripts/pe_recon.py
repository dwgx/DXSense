"""Quick PE recon — sections, entropy, imports, protector fingerprints."""
import math, os, sys, re
from pathlib import Path
import pefile

SUSPICIOUS_SECTIONS = {
    "vmp0", "vmp1", "vmp2",          # VMProtect
    ".themida", ".winlice",          # Themida
    ".enigma1", ".enigma2",          # Enigma
    ".aspack",                       # ASPack
    "upx0", "upx1", "upx2",          # UPX
    ".petite",                       # Petite
    ".mpress1", ".mpress2",          # MPRESS
    ".nsp0", ".nsp1", ".nsp2",       # NsPack
    ".boom",                         # Custom NetEase?
    ".neox", ".nx",                  # NeoX-specific
}

ANTI_DEBUG_APIS = {
    "IsDebuggerPresent", "CheckRemoteDebuggerPresent", "NtQueryInformationProcess",
    "NtSetInformationThread", "NtClose", "OutputDebugStringA", "OutputDebugStringW",
    "GetTickCount", "QueryPerformanceCounter", "ZwQueryInformationProcess",
    "NtQuerySystemInformation", "DbgUiRemoteBreakin", "DbgBreakPoint",
    "NtCreateThreadEx", "NtSetDebugFilterState",
}

def entropy(data: bytes) -> float:
    if not data: return 0.0
    freq = [0] * 256
    for b in data: freq[b] += 1
    n = len(data)
    e = 0.0
    for c in freq:
        if c:
            p = c / n
            e -= p * math.log2(p)
    return e

def analyze(path: Path):
    print(f"\n{'='*80}\n{path.name}  ({path.stat().st_size/1024/1024:.1f} MB)\n{'='*80}")
    try:
        pe = pefile.PE(str(path), fast_load=True)
    except pefile.PEFormatError as ex:
        print(f"  NOT a PE file: {ex}")
        return
    pe.parse_data_directories()

    h = pe.FILE_HEADER
    oh = pe.OPTIONAL_HEADER
    is_64 = pe.FILE_HEADER.Machine == 0x8664
    print(f"  arch:       {'x64' if is_64 else 'x86'}")
    print(f"  entry:      0x{oh.AddressOfEntryPoint:08X}   image_base=0x{oh.ImageBase:X}")
    print(f"  subsystem:  {oh.Subsystem}   DLL chars: 0x{oh.DllCharacteristics:04X}")
    print(f"  linker:     {oh.MajorLinkerVersion}.{oh.MinorLinkerVersion}  (MSVC>=14 ≈ 14.x)")
    ts = pe.FILE_HEADER.TimeDateStamp
    print(f"  timestamp:  0x{ts:08X}")

    # Sections ----
    print(f"\n  sections ({len(pe.sections)}):")
    print(f"    {'name':<12} {'vsize':>10} {'rsize':>10} {'entropy':>8}  {'flags':<8}  hint")
    suspicious = []
    for s in pe.sections:
        name = s.Name.rstrip(b'\x00').decode('latin-1', 'replace')
        raw  = s.get_data()
        e    = entropy(raw) if raw else 0.0
        flag = ''
        if s.Characteristics & 0x20000000: flag += 'X'
        if s.Characteristics & 0x40000000: flag += 'R'
        if s.Characteristics & 0x80000000: flag += 'W'
        hint = ''
        if name.lower() in SUSPICIOUS_SECTIONS: hint = '*** PROTECTOR ***'
        if e > 7.5 and raw: hint = (hint + ' HIGH-ENTROPY (packed?)').strip()
        if hint: suspicious.append((name, hint))
        print(f"    {name:<12} {s.Misc_VirtualSize:>10} {s.SizeOfRawData:>10} {e:>8.3f}  {flag:<8}  {hint}")

    # Imports ----
    imp_dlls  = []
    anti_hits = []
    if hasattr(pe, 'DIRECTORY_ENTRY_IMPORT'):
        for entry in pe.DIRECTORY_ENTRY_IMPORT:
            dll = entry.dll.decode('latin-1', 'replace')
            imp_dlls.append(dll)
            for imp in entry.imports:
                if imp.name:
                    nm = imp.name.decode('latin-1', 'replace')
                    if nm in ANTI_DEBUG_APIS:
                        anti_hits.append(f"{dll}!{nm}")
    print(f"\n  import DLLs ({len(imp_dlls)}):")
    for d in imp_dlls[:30]:
        print(f"    - {d}")
    if len(imp_dlls) > 30:
        print(f"    ... ({len(imp_dlls)-30} more)")
    if not imp_dlls:
        print("    (none — typical for packed binaries, runtime resolves)")

    if anti_hits:
        print(f"\n  anti-debug / anti-analysis APIs seen:")
        for a in sorted(set(anti_hits)): print(f"    - {a}")

    # TLS callbacks (common for protectors)
    if hasattr(pe, 'DIRECTORY_ENTRY_TLS') and pe.DIRECTORY_ENTRY_TLS:
        tls = pe.DIRECTORY_ENTRY_TLS.struct
        print(f"\n  TLS present: callbacks_rva=0x{tls.AddressOfCallBacks:X}  (anti-debug red flag)")

    # Overlay (data appended after last section)
    last_end = max(s.PointerToRawData + s.SizeOfRawData for s in pe.sections)
    real_size = path.stat().st_size
    if real_size > last_end:
        print(f"\n  overlay: {real_size - last_end} bytes past last section (0x{last_end:X}..0x{real_size:X})")

    # Export directory (for DLLs)
    if hasattr(pe, 'DIRECTORY_ENTRY_EXPORT'):
        exp_count = len(pe.DIRECTORY_ENTRY_EXPORT.symbols)
        print(f"\n  exports: {exp_count} symbols")
        for s in pe.DIRECTORY_ENTRY_EXPORT.symbols[:10]:
            if s.name:
                print(f"    - {s.name.decode('latin-1','replace')}")

    # Rich header — compiler fingerprint
    if hasattr(pe, 'RICH_HEADER') and pe.RICH_HEADER:
        print(f"\n  Rich header IDs (compiler fingerprint): {len(pe.RICH_HEADER.values)//2} entries")

    if suspicious:
        print(f"\n  *** NOTABLE SECTIONS ***")
        for name, hint in suspicious:
            print(f"    {name}: {hint}")

if __name__ == "__main__":
    targets = sys.argv[1:] or [
        r"D:\WorkSpace\FeverGames\dwrg2\dwrg.exe",
        r"D:\WorkSpace\FeverGames\dwrg2\neox_engine.dll",
    ]
    for t in targets:
        p = Path(t)
        if p.exists():
            analyze(p)
        else:
            print(f"missing: {t}")
