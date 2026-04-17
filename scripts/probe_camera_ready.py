"""Snapshot the NeoX3 camera the moment it's alive.

Run via the RemoteBridge any time — this probe returns quickly with
"not ready yet" if the camera isn't bound, so you can retry cheaply.

When the camera IS live it prints a structured block that answers:
  1. How to extract floats from math3d.matrix
  2. The actual 4x4 view + projection matrix values
  3. world_to_screen input/output format + a sample call
  4. How to locate the local player's world-space position

That's everything the C++ CameraSampler needs to be written against.
"""

import gc
import math
import sys
import traceback


def find_camera_ctrl():
    for o in gc.get_objects():
        t = type(o)
        if t.__name__ == "CameraCtrl" and t.__module__.startswith("core.camera"):
            return o
    return None


def find_native_camera():
    for o in gc.get_objects():
        mod = getattr(type(o), "__module__", "") or ""
        if mod == "world.camera":
            return o
    return None


def find_world_battle():
    for o in gc.get_objects():
        if type(o).__name__ == "WorldBattle":
            return o
    return None


def find_unit_manager():
    best = None
    for o in gc.get_objects():
        t = type(o)
        if t.__name__ == "UnitManager" and hasattr(o, "units_by_type"):
            return o
    return best


def dump_matrix(label, m):
    print(f"--- {label} ({type(m).__module__}.{type(m).__name__}) ---")
    print(f"  repr: {m!r}")
    # Try every plausible extraction API — we learn which one works.
    extractors = []
    for fn in ("to_list", "as_list", "tolist", "to_tuple", "to_array"):
        if hasattr(m, fn):
            try:
                extractors.append((fn + "()", getattr(m, fn)()))
            except Exception as e:
                extractors.append((fn + "()", f"<raised {type(e).__name__}: {e}>"))
    # Indexable?
    try:
        row0 = m[0]
        extractors.append(("m[0]", row0))
        try:
            extractors.append(("m[0][0]", m[0][0]))
        except Exception as e:
            extractors.append(("m[0][0]", f"<raised {type(e).__name__}: {e}>"))
    except Exception as e:
        extractors.append(("m[0]", f"<raised {type(e).__name__}: {e}>"))

    # math3d.matrix often has get_row / get_column
    for fn in ("get_row", "get_column", "row", "col"):
        if hasattr(m, fn):
            try:
                extractors.append((f"{fn}(0)", getattr(m, fn)(0)))
            except Exception as e:
                extractors.append((f"{fn}(0)", f"<raised {type(e).__name__}: {e}>"))

    for name, val in extractors:
        print(f"  {name} -> {val!r}")


def dump_cam():
    cc = find_camera_ctrl()
    if cc is None:
        print("STATE: no CameraCtrl in memory")
        return False

    print(f"CameraCtrl @ {hex(id(cc))}  module={type(cc).__module__}")
    cam = cc.cam
    if cam is None:
        native = find_native_camera()
        if native is None:
            print("STATE: cam not bound (cc.cam is None), no world.camera native alive")
            print("  → you are probably in the default preload room; enter a scene.")
            return False
        cam = native
        print(f"cc.cam is None — using bare native camera @ {hex(id(cam))}")
    else:
        print(f"cc.cam @ {hex(id(cam))}  module={type(cam).__module__}  repr={cam!r}")

    try:
        dump_matrix("view  (cam.transformation)", cam.transformation)
    except Exception as e:
        print(f"view err: {e}")
    try:
        dump_matrix("proj  (cam.projection_matrix)", cam.projection_matrix)
    except Exception as e:
        print(f"proj err: {e}")

    # world_to_screen probe — try several input types
    print("--- world_to_screen call signatures ---")
    print(f"  has world_to_screen: {hasattr(cam, 'world_to_screen')}")
    pos_probe = None
    try:
        pos_probe = cam.get_pos()
        print(f"  cam.get_pos() -> {pos_probe!r}")
    except Exception as e:
        print(f"  get_pos err: {e}")

    # Try tuple, list, math3d.vector
    import math3d  # proven to exist in the engine namespace

    math3d_names = [n for n in dir(math3d) if not n.startswith("_")]
    print(f"  math3d.*: {math3d_names[:25]}")
    vec_ctor = None
    for name in ("vector", "vec3", "Vec3", "Vector3"):
        if hasattr(math3d, name):
            vec_ctor = getattr(math3d, name)
            print(f"  using math3d.{name}")
            break

    sample_points = [
        ("tuple",  (0.0, 0.0, 0.0)),
        ("list",   [10.0, 1.0, 10.0]),
    ]
    if vec_ctor is not None:
        try:
            sample_points.append(("math3d.vector", vec_ctor(10.0, 1.0, 10.0)))
        except Exception as e:
            print(f"  vec_ctor err: {e}")

    for label, p in sample_points:
        try:
            r = cam.world_to_screen(p)
            print(f"  world_to_screen({label} {p!r}) -> {r!r}  type={type(r).__name__}")
        except Exception as e:
            print(f"  world_to_screen({label} {p!r}) err: {type(e).__name__}: {e}")

    # Local player position — try several access paths
    print("--- local player position probes ---")
    wb = find_world_battle()
    if wb is not None:
        print(f"  WorldBattle @ {hex(id(wb))}")
        for attr in ("local_uid", "local_player_uid", "my_uid", "self_uid"):
            if hasattr(wb, attr):
                try:
                    print(f"    wb.{attr} = {getattr(wb, attr)!r}")
                except Exception as e:
                    print(f"    wb.{attr} err: {e}")
    else:
        print("  no WorldBattle yet")

    um = find_unit_manager()
    if um is not None:
        try:
            byt = um.units_by_type
            print(f"  UnitManager.units_by_type counts: "
                  f"{ {k: len(v) for k, v in byt.items()} }")
            for civ in list(byt.get(2, []))[:3]:
                try:
                    print(f"    civ uid={getattr(civ,'uid',None)!r} "
                          f"pos={getattr(civ,'position',None)!r} "
                          f"is_local={getattr(civ,'is_local',None)!r}")
                except Exception as e:
                    print(f"    civ dump err: {e}")
        except Exception as e:
            print(f"  UnitManager dump err: {e}")
    else:
        print("  no UnitManager")

    return True


try:
    ok = dump_cam()
    print(f"\nRESULT: {'camera-ready' if ok else 'not-ready'}")
except Exception:
    traceback.print_exc(file=sys.stdout)
