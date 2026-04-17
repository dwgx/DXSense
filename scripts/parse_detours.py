"""Parse Microsoft Detours .detourc / .detourd metadata in a PE.

Format observations from neox_engine.dll (8704-byte .detourc, 512-byte .detourd):

  0x000..0x0FF  — header/reserved area. Small-ID bytes (0x00, 0x2411, 0x01, 0x02,
                  0x04, 0x05) repeating in 4- or 8-byte patterns, then zeroes.
                  We treat this as opaque and skip past it to the record array.
  0x100..end    — 16-byte trampoline records:
                    +0  uint64_t  id_or_flags   (small integer, per-hook id)
                    +8  uint64_t  target_va     (absolute VA of the instrumented
                                                 function in the loaded module)
                  Zero-filled records represent unused slots in a reserved table.

.detourd appears to be a tiny footer (0 / -1 / 0 qwords), almost certainly
just the module-level "end of trampoline list" sentinel. We dump it for
completeness but it has no per-function payload.

Output:
  - analysis/detours_table.csv  columns: idx, id, target_va, target_rva,
                                         nearest_export, offset, section
  - stdout summary
"""
from __future__ import annotations
import bisect, csv, struct, sys
from pathlib import Path
import pefile

TARGET = Path(sys.argv[1] if len(sys.argv) > 1
              else r"D:\Project\DXSense\analysis\neox_engine.dll")
OUT    = Path(r"D:\Project\DXSense\analysis\detours_table.csv")

RECORD_AREA_START = 0x100
RECORD_STRIDE     = 16

def section_by_name(pe: pefile.PE, name: str):
    for s in pe.sections:
        if s.Name.rstrip(b'\x00').decode('latin-1','replace') == name:
            return s
    return None

def section_for_rva(pe: pefile.PE, rva: int) -> str:
    for s in pe.sections:
        if s.VirtualAddress <= rva < s.VirtualAddress + max(s.Misc_VirtualSize, s.SizeOfRawData):
            return s.Name.rstrip(b'\x00').decode('latin-1','replace')
    return '(none)'

def main() -> int:
    pe   = pefile.PE(str(TARGET))
    base = pe.OPTIONAL_HEADER.ImageBase

    dc = section_by_name(pe, '.detourc')
    dd = section_by_name(pe, '.detourd')
    if not dc:
        print("no .detourc section")
        return 1

    dc_bytes = dc.get_data()[:dc.Misc_VirtualSize]
    dd_bytes = dd.get_data()[:dd.Misc_VirtualSize] if dd else b''
    print(f".detourc: {len(dc_bytes)} bytes   VA 0x{base+dc.VirtualAddress:X}")
    print(f".detourd: {len(dd_bytes)} bytes" + (f"   VA 0x{base+dd.VirtualAddress:X}" if dd else ""))

    # Build an address-ordered export list for nearest-symbol resolution.
    exports: list[tuple[int, str]] = []   # sorted by RVA
    if hasattr(pe, 'DIRECTORY_ENTRY_EXPORT'):
        for s in pe.DIRECTORY_ENTRY_EXPORT.symbols:
            if s.name:
                exports.append((s.address, s.name.decode('latin-1','replace')))
        exports.sort()
    rva_list = [e[0] for e in exports]
    print(f"exports indexed: {len(exports)}")

    # --- parse records ---
    records = []
    for off in range(RECORD_AREA_START, len(dc_bytes) - RECORD_STRIDE + 1, RECORD_STRIDE):
        rec = dc_bytes[off:off + RECORD_STRIDE]
        id_or_flags, target_va = struct.unpack('<QQ', rec)
        if id_or_flags == 0 and target_va == 0:
            continue
        records.append({
            'offset':   off,
            'id':       id_or_flags,
            'target_va': target_va,
        })

    print(f"non-zero records: {len(records)}")

    # --- resolve to exports ---
    resolved_exact = 0
    resolved_near  = 0
    rows = []
    for i, r in enumerate(records):
        va = r['target_va']
        rva = va - base if va >= base else va   # guard against non-VA values
        sect = section_for_rva(pe, rva)
        name = ''
        off_into = 0
        if exports:
            idx = bisect.bisect_right(rva_list, rva) - 1
            if 0 <= idx < len(rva_list):
                exp_rva, exp_name = exports[idx]
                delta = rva - exp_rva
                # Only accept if reasonably close (<16 KB). Otherwise leave blank.
                if 0 <= delta < 16 * 1024:
                    name = exp_name
                    off_into = delta
                    if delta == 0:
                        resolved_exact += 1
                    else:
                        resolved_near += 1
        rows.append({
            'idx':             i,
            'offset':          f'0x{r["offset"]:04X}',
            'id':              f'0x{r["id"]:X}',
            'target_va':       f'0x{va:X}',
            'target_rva':      f'0x{rva:X}',
            'section':         sect,
            'nearest_export':  name,
            'offset_into':     off_into,
        })

    OUT.parent.mkdir(parents=True, exist_ok=True)
    with OUT.open('w', newline='', encoding='utf-8') as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()) if rows else
                           ['idx','offset','id','target_va','target_rva','section','nearest_export','offset_into'])
        w.writeheader()
        w.writerows(rows)
    print(f"wrote {OUT}  ({len(rows)} rows)")

    # --- summary ---
    total_exports = len(exports) or 1
    print(f"\n=== Summary ===")
    print(f"records:               {len(records)}")
    print(f"resolved to export:    {resolved_exact} exact + {resolved_near} near = "
          f"{resolved_exact+resolved_near}  ({100.0*(resolved_exact+resolved_near)/len(records):.1f}%)")
    print(f"ratio of exports instrumented: "
          f"{100.0*(resolved_exact+resolved_near)/total_exports:.2f}%")

    # What sections do the targets live in?
    from collections import Counter
    sect_counter = Counter(r['section'] for r in rows)
    print("\ntarget sections:")
    for s, n in sect_counter.most_common():
        print(f"  {s:<10} {n}")

    # ID distribution (the first qword)
    id_counter = Counter(r['id'] for r in rows[:100])
    print("\ntop 'id' values (first 100 records):")
    for i, n in id_counter.most_common(10):
        print(f"  {i:<20} {n}")

    # First 20 resolved targets — the real money shot
    print("\nfirst 20 resolved targets:")
    shown = 0
    for r in rows:
        if r['nearest_export']:
            nm = r['nearest_export']
            print(f"  [{r['idx']:>4}] {r['target_va']}  {nm[:100]}"
                  + (f"+{r['offset_into']}" if r['offset_into'] else ''))
            shown += 1
            if shown >= 20:
                break

    return 0

if __name__ == '__main__':
    raise SystemExit(main())
