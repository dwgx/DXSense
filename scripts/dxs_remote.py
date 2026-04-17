"""Remote-drive the in-game Python REPL via DXSense's RemoteBridge.

Usage:
    python scripts/dxs_remote.py                      # heredoc stdin
    python scripts/dxs_remote.py -c "import sys; print(sys.path)"
    python scripts/dxs_remote.py --probe              # run the canonical probe
    python scripts/dxs_remote.py path/to/snippet.py   # file
"""
from __future__ import annotations
import argparse, json, pathlib, sys, textwrap, urllib.error, urllib.request

HOST = "http://127.0.0.1:9099/"
TIMEOUT = 30.0

CANONICAL_PROBE = textwrap.dedent(r"""
    import sys, gc, __main__
    print("\n========== DXSense LIVE PROBE ==========")
    mgr_like = [n for n in dir(__main__) if any(x in n.lower()
                for x in ('mgr','manager','world','scene','avatar','player','game'))]
    print(f"\n[1] __main__ manager-like globals ({len(mgr_like)}):")
    for n in mgr_like[:40]:
        try: print(f"  {n:<34} {type(getattr(__main__,n)).__name__}")
        except Exception: pass

    print("\n[2] Self-like entities (Avatar/Soul/PlayerAvatar/LocalPlayer/Survivor):")
    for o in gc.get_objects():
        try: cn = type(o).__name__
        except Exception: continue
        if cn in ('Avatar','Soul','PlayerAvatar','LocalPlayer','Survivor'):
            try:
                eid  = getattr(o,'entity_id',None) or getattr(o,'id',None)
                pos  = getattr(o,'position',None) or getattr(o,'pos',None)
                hp   = getattr(o,'hp',None)
                name = getattr(o,'name','')
                print(f"  {cn:<18} id={id(o):#x} eid={eid} name={name!r} hp={hp} pos={pos}")
            except Exception as e: print(f"  err {e}")

    print("\n[3] EntityFactory instances:")
    for o in gc.get_objects():
        try: cn = type(o).__name__
        except Exception: continue
        if cn == 'EntityFactory':
            try:
                ents = list(o.get_all_entities()) if hasattr(o,'get_all_entities') else []
                kinds = {}
                for e in ents: kinds[type(e).__name__] = kinds.get(type(e).__name__,0)+1
                print(f"  EntityFactory @ {id(o):#x}  {len(ents)} entities")
                for k,v in sorted(kinds.items(), key=lambda x:-x[1]):
                    print(f"    {k:<22} x {v}")
            except Exception as e: print(f"  err {e}")

    print("\n[4] Camera-like objects:")
    seen = 0
    for o in gc.get_objects():
        try: cn = type(o).__name__
        except Exception: continue
        if 'Camera' in cn and 'Mode' not in cn and 'Cut' not in cn:
            print(f"  {cn:<30} id={id(o):#x}")
            seen += 1
            if seen >= 10: break

    print("\n[5] Loaded NeoX/game modules (first 30):")
    mods = [m for m in sys.modules
            if any(m.startswith(p) for p in ('neox.','common.','mobile.','core.','game.','world2'))]
    for m in sorted(mods)[:30]: print(f"  {m}")
    print(f"  ...  {len(mods)} total")
    print("\n========== END PROBE ==========\n")
""").strip()


def call(source: str, isolate: bool = True) -> str:
    """POST Python to the in-game bridge. If isolate, wraps the snippet so
    stdout from OUR code is captured to a local buffer and delivered between
    sentinels — this filters out the game's concurrent logging spam that
    otherwise shares the same sys.stdout sink.
    """
    if isolate:
        source = textwrap.dedent("""
            import io as _dxs_io, sys as _dxs_sys, traceback as _dxs_tb
            _dxs_buf = _dxs_io.StringIO()
            _dxs_prev = _dxs_sys.stdout
            _dxs_sys.stdout = _dxs_buf
            try:
        """) + textwrap.indent(source, "    ") + textwrap.dedent("""
            except Exception:
                _dxs_tb.print_exc(file=_dxs_buf)
            finally:
                _dxs_sys.stdout = _dxs_prev
                print("==DXS_BEGIN==")
                print(_dxs_buf.getvalue())
                print("==DXS_END==")
        """)
    data = source.encode("utf-8")
    req = urllib.request.Request(HOST, data=data, method="POST",
                                 headers={"Content-Type": "text/plain"})
    try:
        with urllib.request.urlopen(req, timeout=TIMEOUT) as r:
            raw = r.read().decode("utf-8", errors="replace")
    except urllib.error.URLError as e:
        return f"!! remote bridge unreachable: {e}\n" \
               f"   is DXSense injected and the bridge enabled (Config remote.enabled)?\n"
    if isolate:
        a = raw.find("==DXS_BEGIN==")
        b = raw.find("==DXS_END==", a + 1)
        if a >= 0 and b > a:
            return raw[a + len("==DXS_BEGIN==") : b].strip("\n") + "\n"
    return raw


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("-c", "--code",  help="inline Python snippet")
    ap.add_argument("--probe",       action="store_true",
                    help="run the canonical DXSense live-probe")
    ap.add_argument("file",          nargs="?",
                    help="path to a .py file to run")
    args = ap.parse_args()

    if args.probe:        src = CANONICAL_PROBE
    elif args.code:       src = args.code
    elif args.file:       src = pathlib.Path(args.file).read_text(encoding="utf-8")
    else:                 src = sys.stdin.read()

    print(call(src), end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
