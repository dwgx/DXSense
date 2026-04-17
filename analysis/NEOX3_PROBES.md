# NeoX3 Python probe catalog

Copy-pasteable Python snippets that work against the running game via
the DXSense RemoteBridge. Use with:

```bash
python scripts/dxs_remote.py -c "<paste>"
# or save to a file and:
python scripts/dxs_remote.py path/to/probe.py
```

Every probe here assumes the game is alive and DXSense is injected.

---

## 0. Sanity check

```py
import sys
print("sys.version:", sys.version.split()[0])
print("modules loaded:", len(sys.modules))
```

---

## 1. Avatar (account state)

```py
import gc
av = next(o for o in gc.get_objects() if type(o).__name__ == 'Avatar')
print("account_aid:", av.account_aid)
print("active butcher:", av.active_butcher_page_id)
print("active civilian:", av.active_civilian_page_id)
print("valid match types:", av.valid_match_types)
```

### Avatar full attribute list (filtered)

```py
import gc
av = next(o for o in gc.get_objects() if type(o).__name__ == 'Avatar')
attrs = [n for n in dir(av) if not n.startswith('_')]
print(f"{len(attrs)} public attributes")
# filter by substring to find categories
for n in sorted(attrs):
    if 'match' in n.lower():
        print(" ", n)
```

---

## 2. Camera (real matrices + world→screen)

```py
import gc
ctrl = next(o for o in gc.get_objects()
            if type(o).__name__ == 'CameraCtrl' and
               type(o).__module__.startswith('core.camera'))
cam = ctrl.cam
print("fov:", cam.fov)
print("position:", cam.position)
print("look_at:", cam.look_at)
# cam.transformation / cam.projection_matrix are math3d.matrix — use repr
print("view:", repr(cam.transformation))
print("proj:", repr(cam.projection_matrix))

# Project a world point to screen pixels — ESP-grade projection
your_pos = ctrl.unit.position   # or any math3d.vector
px, py = cam.world_to_screen(your_pos)
print(f"your char on screen: ({px}, {py})")
```

### Camera mode + helpers

```py
import gc
ctrl = next(o for o in gc.get_objects()
            if type(o).__name__ == 'CameraCtrl' and
               type(o).__module__.startswith('core.camera'))
print("mode:", ctrl.get_cam_mode())
print("pos, dir:", ctrl.get_pos_dir())
print("forward:", ctrl.get_dir3())
print("right:", ctrl.get_right())
print("up:", ctrl.get_up_dir())
print("params (r,h,p,fov):", ctrl.get_cam_param_all())
```

---

## 3. In-match units

```py
import gc
um = next((o for o in gc.get_objects()
           if type(o).__name__ == 'UnitManager' and hasattr(o, 'units_by_type')), None)
if not um:
    print("no in-match UnitManager — probably in hall")
else:
    print(f"{len(um.units)} units total")
    for t, lst in sorted(um.units_by_type.items()):
        name = um.unit_type2des_str.get(t, f'T{t}')
        print(f"  {t:>4}  {name:<14}  x{len(lst)}")
```

### Survivors + Hunter with positions

```py
import gc
um = next(o for o in gc.get_objects()
          if type(o).__name__ == 'UnitManager' and hasattr(o, 'units_by_type'))
for t in (1, 2):
    tag = 'BUTCHER' if t == 1 else 'CIVILIAN'
    for u in um.units_by_type.get(t, []):
        p = u.position
        print(f"  uid={u.uid}  {tag}  pos=({p.x:7.1f}, {p.y:5.2f}, {p.z:7.1f})  speed={u.speed_real:5.1f}")
```

### Generator decoding progress

```py
import gc
um = next(o for o in gc.get_objects()
          if type(o).__name__ == 'UnitManager' and hasattr(o, 'units_by_type'))
for g in um.units_by_type.get(3, []):
    p = g.position
    prog = getattr(g, 'progress', None) or getattr(g, 'charging_progress', None)
    state = getattr(g, 'state', None)
    who = getattr(g, 'decoder_uids', None) or getattr(g, 'charging_uids', None)
    print(f"  gen uid={g.uid}  pos=({p.x:.0f},{p.z:.0f})  state={state}  prog={prog}  decoders={who}")
```

### Rocket chair (Hook) state

```py
import gc
um = next(o for o in gc.get_objects()
          if type(o).__name__ == 'UnitManager' and hasattr(o, 'units_by_type'))
for h in um.units_by_type.get(4, []):
    print(f"  hook uid={h.uid}  state={h.state}  pos=({h.position.x:.0f},{h.position.z:.0f})")
```

### Door state

```py
import gc
um = next(o for o in gc.get_objects()
          if type(o).__name__ == 'UnitManager' and hasattr(o, 'units_by_type'))
print(f"doors: {um.door_info}")
for uid in um.door_info:
    u = um.units.get(uid)
    if u:
        print(f"  {uid}: pos=({u.position.x:.0f},{u.position.z:.0f}) state={u.state} opened={getattr(u,'opened',None)}")
```

### Who is carrying whom (hunter carrying survivor)

```py
import gc
um = next(o for o in gc.get_objects()
          if type(o).__name__ == 'UnitManager' and hasattr(o, 'units_by_type'))
print("carrier_units:", um.carrier_units)
print("bound_info_list:", um.bound_info_list)
print("bound_list:", um.bound_list)
```

### Your self unit (survivor mode)

Not directly flagged; heuristic — match your Avatar's active_civilian_page_id:

```py
import gc
um = next(o for o in gc.get_objects()
          if type(o).__name__ == 'UnitManager' and hasattr(o, 'units_by_type'))
av = next(o for o in gc.get_objects() if type(o).__name__ == 'Avatar')
my_role = av.active_civilian_page_id
print("my role id:", my_role)
for u in um.units_by_type.get(2, []):
    print(f"  civilian uid={u.uid} role_id={getattr(u,'role_id', getattr(u,'hero_id','?'))}")
```

---

## 4. World / game time

```py
import gc
wb = next((o for o in gc.get_objects() if type(o).__name__ == 'WorldBattle'), None)
if wb:
    print("WorldBattle:", hex(id(wb)))
    for n in ('game_time','game_frame','match_type','game_mode','map_id'):
        v = getattr(wb, n, None)
        if v is not None: print(f"  {n} = {v}")

# combat-core UM gives game_time reliably
for o in gc.get_objects():
    if type(o).__name__ == 'UnitManager' and hasattr(o, 'game_time'):
        print(f"game_time: {o.game_time:.1f}s  game_frame: {o.game_frame}")
        break
```

---

## 5. Manager inventory

### Count every manager class

```py
import gc, collections
cnt = collections.Counter()
for o in gc.get_objects():
    try: cn = type(o).__name__
    except: continue
    if cn.endswith('Mgr') or cn.endswith('Manager') or cn.endswith('Factory'):
        cnt[cn] += 1
for cn, n in sorted(cnt.items(), key=lambda x: (-x[1], x[0])):
    print(f"  {cn:<36} x{n}")
```

### Dive into a specific manager

```py
import gc
# change the name to inspect any manager from the list
target = 'CheatValidateMgr'
for o in gc.get_objects():
    if type(o).__name__ == target:
        print(f"{target} @ {id(o):#x}")
        for n in sorted(dir(o)):
            if n.startswith('_'): continue
            try:
                v = getattr(o, n)
                if callable(v): print(f"  {n:<30} <method>")
                else: print(f"  {n:<30} = {repr(v)[:80]}")
            except Exception as e: print(f"  {n:<30} !{e}")
        break
```

---

## 6. Network / RPC

### Gate server addresses currently in use

```py
import sys
# asiocore_py3 logger spews disconnect/connect events with IPs — enable
# logging capture by reading the existing EchoGateClient instances
import gc
for o in gc.get_objects():
    if type(o).__name__ == 'EchoGateClient':
        print(f"gate client @ {id(o):#x}")
        for n in ('ip','port','remote_ip','remote_port','connected'):
            v = getattr(o, n, None)
            if v is not None: print(f"  {n} = {v}")
```

### Peek into RPC stubs

```py
import gc
for o in gc.get_objects():
    if type(o).__name__ in ('GenClientServerRpcStub', 'GenClientSoulRpcStub'):
        print(f"{type(o).__name__} @ {id(o):#x}")
        methods = [n for n in dir(o) if not n.startswith('_') and callable(getattr(o, n, None))]
        print(f"  {len(methods)} callable methods; first 20:")
        for n in methods[:20]: print(f"    {n}")
```

---

## 7. Security layer (user-authored)

```py
import gc
for name in ('CheatValidateMgr', 'ExceptionRecordMgr', 'MultiOpenMgr',
             'TracebackManager', 'RawTraceRouteMgr'):
    for o in gc.get_objects():
        if type(o).__name__ == name:
            print(f"{name} @ {id(o):#x}")
            for n in sorted(dir(o))[:25]:
                if n.startswith('_'): continue
                try:
                    v = getattr(o, n)
                    mark = '<m>' if callable(v) else '='
                    s = '' if callable(v) else repr(v)[:50]
                    print(f"  {n:<28} {mark} {s}")
                except: pass
            break
    print()

import core.AntiMemModifyMgr as m
print("core.AntiMemModifyMgr module:")
for n in dir(m):
    if not n.startswith('_'):
        print(f"  {n}")
```

---

## 8. Continuous monitors (run-and-loop)

### Hunter distance watcher

Run in a terminal, prints your distance to the hunter every second:

```bash
while true; do
  python scripts/dxs_remote.py -c "
import gc, math
um = next(o for o in gc.get_objects() if type(o).__name__=='UnitManager' and hasattr(o,'units_by_type'))
if 2 in um.units_by_type and 1 in um.units_by_type:
    me = um.units_by_type[2][0]
    h  = um.units_by_type[1][0]
    d = math.hypot(h.position.x - me.position.x, h.position.z - me.position.z)
    print(f'hunter dist: {d:6.1f}m')
else:
    print('not in match')
"
  sleep 1
done
```

### Live position trail

```py
import gc, time
um = next(o for o in gc.get_objects() if type(o).__name__=='UnitManager' and hasattr(o,'units_by_type'))
if 2 not in um.units_by_type:
    print("not in match")
else:
    me = um.units_by_type[2][0]
    print("recent poses:")
    for p in list(me.n_seconds_poses)[-10:]:
        print(f"  ({p.x:7.1f}, {p.y:5.2f}, {p.z:7.1f})")
```

---

## 9. Screen-space projection (ESP math)

Use `cam.world_to_screen` to get pixel coords for ANY world position:

```py
import gc
ctrl = next(o for o in gc.get_objects()
            if type(o).__name__ == 'CameraCtrl' and
               type(o).__module__.startswith('core.camera'))
cam = ctrl.cam
um = next((o for o in gc.get_objects()
           if type(o).__name__ == 'UnitManager' and hasattr(o, 'units_by_type')), None)

if not um:
    print("not in match")
else:
    for u in um.units_by_type.get(1, []) + um.units_by_type.get(2, []):
        try:
            sp = cam.world_to_screen(u.position)
            kind = 'BUTCHER' if u in um.units_by_type[1] else 'CIVILIAN'
            print(f"  {kind} uid={u.uid} screen={sp}")
        except Exception as e:
            print(f"  err {e}")
```

`cam.world_to_screen` returns `None` or throws when the target is
behind the camera or outside the frustum — use a try/except.
