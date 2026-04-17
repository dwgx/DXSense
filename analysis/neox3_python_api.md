# NeoX3 Python runtime — discovered live API

All entries verified live against the running `dwrg.exe` on 2026-04-17 via
the DXSense `RemoteBridge` (`scripts/dxs_remote.py`). Addresses / attribute
names in this doc are stable enough to bake into panel probes.

---

## Camera (the one we spent three passes hunting)

**Singleton**: `core.camera.CameraCtrl` — exactly one live instance.

```py
import gc
ctrl = next(o for o in gc.get_objects()
            if type(o).__name__ == 'CameraCtrl' and
               type(o).__module__.startswith('core.camera'))
```

**Native camera object**: `ctrl.cam`, a `world.camera` C extension. The
important ones (all verified live):

| attribute | what it is |
|-----------|------------|
| `fov`                  | `float`, degrees (65.0 default) |
| `position` / `world_position` | `math3d.vector` — eye in world space |
| `look_at`              | `math3d.vector` — target |
| `transformation`       | `math3d.matrix` — view matrix (4x4) |
| `projection_matrix`    | `math3d.matrix` — projection (4x4) |
| `world_matrix` / `world_transformation` | redundant views of the same |
| `rotation_matrix`      | 3x3-ish view rotation |
| `z_range`              | `(near, far)` tuple — `(1.0, 5000.0)` |
| `world_to_screen(v)`   | **takes a `math3d.vector`, returns `(x_px, y_px)`** |
| `screen_to_world(v)`   | inverse projection |
| `world_to_camera(v)`   | world → camera-space |

`math3d.matrix` objects are not iterable — read them via `repr()` parsing
or index them as `m[row][col]`.

**NOTE:** `cam.transform_matrix` raises `"use camera.transformation instead"`.
Always use `transformation` / `world_transformation`.

**CameraCtrl ergonomic helpers**:

| call | returns |
|------|---------|
| `ctrl.get_dir()`          | forward vector (horizontal plane) |
| `ctrl.get_dir3()`         | 3D forward |
| `ctrl.get_right()`        | right vector |
| `ctrl.get_up_dir()`       | up vector |
| `ctrl.get_pos_dir()`      | `(position, forward)` tuple |
| `ctrl.get_cam_tran()`     | `(position, (pitch, yaw, roll))` |
| `ctrl.get_cam_param_all()`| `(radius, pivot_height, pitch, fov)` |
| `ctrl.cam_fov`            | 65.0 |

---

## Units & game state

**`UnitManager`** — two instances. The client-side one has `units_by_type`;
the combat-core one owns `ref_bind_player` (often `None` — unreliable).

```py
um = next(o for o in gc.get_objects()
          if type(o).__name__ == 'UnitManager' and hasattr(o, 'units_by_type'))
```

**Type map** (verified):
| id  | name       | notes |
|-----|------------|-------|
| 1   | BUTCHER    | 监管者 |
| 2   | CIVILIAN   | 求生者 |
| 3   | GENERATOR  | 密码机 (state=1 未解) |
| 4   | HOOK       | 火箭椅 |
| 5   | BOX        | |
| 6   | DOOR       | 大门 |
| 7   | WOOD       | 木板 |
| 8   | PANEL      | 窗 |
| 9   | CUPBOARD   | 柜子 |
| 10  | CAVE       | |
| 11  | CROW       | 乌鸦 |
| 12  | SWITCH     | |
| 510 | SPIRIT     | |

Per-unit fields that actually populate (on `CivilianUnit` / `ButcherUnit`):

| field | meaning |
|-------|---------|
| `position`        | current world pos |
| `target_pos`      | where it's moving to |
| `next_pos`        | immediate next step |
| `last_server_pos` | authoritative server |
| `speed`, `speed_real` | floats |
| `speed_dir`       | unit vector in XZ plane |
| `n_seconds_poses` | `deque` of recent positions (trail) |
| `state_machine`   | `common_cs.states.StateMachine.StateMachine` |
| `uid`             | numeric uid, 1000001-based per match |
| `player_name`     | bytes — decode with `latin-1` then `gbk` |
| role-specific:    | `phantom_possess_*`, `monkey_uid`, `spy_target_uid`, `parkour_end_pos`, ... |

---

## Avatar (the player's account object)

Lives in `gc.get_objects()` as the unique `Avatar` instance. NOT the
in-match character — it's the whole account:

```py
av = next(o for o in gc.get_objects() if type(o).__name__ == 'Avatar')
av.account_aid                # NetEase user id (int)
av.active_butcher_page_id     # '1' — current hunter role
av.active_civilian_page_id    # '10014' — current survivor role
av.achievement_*              # per-match-type achievement dicts
av.activity_*                 # event state
av.start_match / av.wait_match / av.show_match_ui   # matchmaking methods
```

---

## World / Match

**`WorldBattle`** — one per active match. Contains unit references,
match clock, gate server info.

```py
wb = next(o for o in gc.get_objects() if type(o).__name__ == 'WorldBattle')
```

**Combat core `UnitManager`**:
- `game_time` — seconds since match started
- `game_frame` — integer frame count
- `ref_gamemode_data` — `GameModeDataNormal` / similar

---

## Networking

Framework is `asiocore_py3 v2.0.26` (build path
`D:\conan\data\asiocore_py3\2.0.26\H55\stable\source\...`) running
KCP redundantly across 5 parallel gate IPs. Relevant globals:

- `EchoGateClient` logger channel — logs every connect / disconnect /
  redundancy decision
- `common_cs.combat_core.CombatCore.CombatCore`
- async logger funnels through `neox_engine.dll +0x30DB100`
  (`sub_1830DB100`) — our RPC tracer hook target

---

## Cheat / validation

User-authored modules discovered at runtime:

- `core.AntiMemModifyMgr` (module)
- `CheatValidateMgr` (class, one live instance)
- `ExceptionRecordMgr`
- `Avatar.security_lock_key = {19: 'chat_lobby_lock', 20: 'chat_friend_lock'}`

These ARE the "验证" the user wrote. They live in-process alongside
everything else — the game trusts its own Python layer.
