# NeoX3 runtime state map

**Verified live** against the running `dwrg.exe` on 2026-04-17 via the DXSense
RemoteBridge (`scripts/dxs_remote.py`). Everything in this doc is reachable
from the in-process Python interpreter at `http://127.0.0.1:9099/` once
DXSense.dll is injected.

Read alongside `NEOX3_STATIC.md` (C++ side, IDA) and
`analysis/neox3_python_api.md` (quick reference).

---

## 1. Avatar (player's account object)

**Not** the in-match character. This is the global account state object —
loaded once at login and persists across match / hall / re-queue. `gc`
scan returns exactly one `Avatar` instance per session:

```py
av = next(o for o in gc.get_objects() if type(o).__name__ == 'Avatar')
```

### Identity
| attribute                       | meaning |
|---------------------------------|---------|
| `account_aid`                   | NetEase user ID (int, e.g. 435442169) |
| `active_butcher_page_id`        | currently selected Hunter role ID (str) |
| `active_civilian_page_id`       | currently selected Survivor role ID |

### Bulk state (top categories)
| prefix              | holds |
|---------------------|-------|
| `achievement_*`     | per-match-type achievement progress |
| `act_*`             | per-activity / per-event records |
| `activity_*`        | every running event (anniversary, seasonal, crossover) |
| `trial_card_unlock` | dict[role_id → trial card state] |
| `unlock_*`          | cosmetic / role unlocks |
| `show_match_dct`    | matchmaking queue configurations |
| `valid_match_types` | `{'63','64'}` — which modes are selectable |
| `security_lock_*`   | chat / lobby locks written by CheatValidateMgr |

### Methods
~2000 methods total. Categories worth knowing:

- `start_match`, `wait_match`, `wait_ladder_match`, `switch_mobile_match_pc`
- `show_match_ui`, `show_match_prepare`, `show_match_room`
- `start_init_battle_world`, `start_battle_little_after_loading`
- `reset_position`, `sync_player_pos_after_inter`
- `send_battle_event`, `sync_battle_event`, `sync_little_battle_event`
- `request_single_match_cb`, `request_team_match_cb`
- `show_battle_result_ui`, `show_battle_result_token_reward`

The Avatar is where the **client-side game state machine lives** for
everything outside of an active match.

---

## 2. CameraCtrl (the camera we spent forever hunting)

**Single instance** — the one live camera controller. Extensively verified.

```py
ctrl = next(o for o in gc.get_objects()
            if type(o).__name__ == 'CameraCtrl'
            and type(o).__module__.startswith('core.camera'))
cam = ctrl.cam  # the native world.camera C extension
```

### Native `world.camera` attributes

| attribute                    | type / value |
|------------------------------|--------------|
| `fov`                        | float degrees (65.0 default) |
| `position` / `world_position`| math3d.vector — eye in world |
| `look_at`                    | math3d.vector — target |
| `z_range`                    | `(near, far)` = `(1.0, 5000.0)` |
| `transformation`             | math3d.matrix — **view matrix** |
| `projection_matrix`          | math3d.matrix |
| `world_matrix` / `world_transformation` | same as above |
| `rotation_matrix` / `world_rotation_matrix` | 3×3-ish rotation |
| `scale` / `world_scale`      | `<1,1,1>` vec |
| `teleport`                   | bool |
| `inherit_flag` / `scene_flag`| engine flags |
| `valid`                      | bool |
| `name`                       | `camera_<N>` — runtime handle name |

### Native methods (verified callable)

| method                           | use |
|----------------------------------|-----|
| `world_to_screen(math3d.vector)` | returns `(px, py)` — ready-made ESP |
| `screen_to_world(math3d.vector)` | inverse |
| `world_to_camera(vec)`           | world → camera-space |
| `is_inside(vec)`                 | frustum test |
| `move_to(...)` / `move_to_advanced(...)` | tween move |
| `teleport` (set)                 | snap |
| `slide` / `rotate_x` / `rotate_y` / `rotate_z` / `rotate_axis` | manipulation |
| `set_placement` / `set_focus_setting` / `set_focus_tracking` | config |
| `set_perspect` / `set_ortho`     | projection mode |

### CameraCtrl ergonomic helpers (Python side)

| call                         | returns |
|------------------------------|---------|
| `ctrl.get_dir()`             | forward vec (XZ plane) |
| `ctrl.get_dir3()`            | full 3D forward |
| `ctrl.get_right()`           | right vec |
| `ctrl.get_up_dir()`          | up vec |
| `ctrl.get_pos_dir()`         | `(pos, forward)` |
| `ctrl.get_cam_tran()`        | `(pos, (pitch, yaw, roll))` |
| `ctrl.get_cam_param_all()`   | `(radius, pivot_height, pitch, fov)` |
| `ctrl.cam_fov`               | 65.0 |
| `ctrl.cam_handler`           | current `CamMode*` instance |
| `ctrl.cam_list`              | dict[int → CamModeBase subclass] |
| `ctrl.unit`                  | client-side MyCityUnit / in-match unit |

### Camera modes discovered

`core.camera.CamMode*` — ~20 subclasses including `CamModeBase`,
`CamModeFollow`, `CamModeWildman`, `CamModeGMTop`, `CamModeTerrainOperator`,
`CamModeSculptor`, `CamModeDecalPreview`, `CamModeFireworkStationary`,
`CamModeWatchVr`, `CamModeRacetrackZoom`, `CamModeBarbecue`,
`CamModeFreeGame`, `CamModeUGC`, `CamModeUGCFree`, `CamModeCheckColRotateY`,
`CamModeRingToss`, `CamModePhotoBoard`, `CamModeTableFPV`,
`CamModeSnowWrite`, `CamModeTunnelExplore`, `CamModeGMFree`,
`CamModeSmoothFocus`, `CamModeTps`, `CamModeUGCStory`.

### Gotcha

`cam.transform_matrix` raises `"use camera.transformation instead please!"`.
Always use `transformation` / `world_transformation`. The `math3d.matrix`
is **not iterable** — use `repr()` parsing or `m[row][col]` indexing.

---

## 3. UnitManager (in-match)

There are **two** UnitManager instances during a match. Client-facing is
the interesting one:

```py
um = next(o for o in gc.get_objects()
          if type(o).__name__ == 'UnitManager' and hasattr(o, 'units_by_type'))
```

### Type → name map

Hard-coded on the UnitManager itself as `unit_type2des_str`. Confirmed
values:

| id  | name       | notes                              |
|-----|------------|------------------------------------|
| 0   | BASE       |                                    |
| 1   | BUTCHER    | 监管者                              |
| 2   | CIVILIAN   | 求生者                              |
| 3   | GENERATOR  | 密码机 (state=1 未解)               |
| 4   | HOOK       | 火箭椅 / 绑人点                     |
| 5   | BOX        | 宝箱                                |
| 6   | DOOR       | 大门                                |
| 7   | WOOD       | 木板                                |
| 8   | PANEL      | 窗户                                |
| 9   | CUPBOARD   | 柜子                                |
| 10  | CAVE       | 洞穴 / 地下通道                     |
| 11  | CROW       | 乌鸦                                |
| 12  | SWITCH     | 机关                                |
| 510 | SPIRIT     |                                    |
| 1911| COMMON_SCENE| 场景容器                            |

Plus a large list of CARRIER / TRANSPORT / EFFECT specialisations under
`CARRIER_UNIT_TYPES = (230, 231, 232, 242, 254, 507, 505, 10116)` and
many `1000+` ids for skill / prop sub-units.

### UnitManager state

```
units               dict[uid → unit]             all active units
units_by_type       dict[type_id → [unit, ...]]  grouped
units_tick          dict[uid → unit]             units with per-frame tick
base_units_tick     dict                          base-class tick
carrier_units       list                          units carrying payload
carrier_type2unit_type  dict[carrier_id → unit_type]
door_info           [uid_A, uid_B]                gate UIDs
firework_avatar_id2unit   dict
tlks_panel_list     list                          interactable panels
wax_frozen_uids     dict                          wax-frozen survivors
```

### CivilianUnit / ButcherUnit fields

From `ClientUnits.CivilianUnit.CivilianUnit` — heavily populated per-frame:

| field                       | meaning |
|-----------------------------|---------|
| `uid` / `original_uid`      | 1000000-based uid |
| `position`                  | **live world pos** (math3d.vector) |
| `target_pos`                | where unit is moving to |
| `next_pos`                  | immediate next step |
| `last_server_pos`           | authoritative server pos |
| `logic_position`            | server-verified logical pos |
| `visual_position`           | display-lerped pos |
| `speed` / `speed_real`      | float scalars |
| `speed_dir`                 | XZ unit vector |
| `now_speed_dir`             | current direction |
| `start_speed` / `patrol_speed` | specials |
| `n_seconds_poses`           | `deque[recent positions]` — trail |
| `old_pos_list`              | history |
| `state_machine`             | `common_cs.states.StateMachine` |
| `state2de_id_map`           | state → decoded id map |
| `player_name`               | bytes, decode via `latin-1` → `gbk` |
| `show_player_name_type`     | how name renders |
| `race_team_type`            | team number |
| `last_hook_uid`             | last rocket chair used |
| `last_attack_b_uid`         | last attacker uid |
| `last_attacker_to_fall_uid` | last fall reason |
| `last_break_puppet_uid`     | doll break |
| `last_revive_coffin_uid`    | coffin revive |
| `last_saved_uid`            | survivor this one saved |
| `last_saver_uid`            | saver of this survivor |
| `partition_bone_name`       | IK bone name |
| `role_model_copy_component` | the `RoleModelCopyComponent` |

### Role-specific fields

Different survivor/hunter role abilities expose dedicated fields:

- **小女孩 (Doll)**: `phantom_possess_cd`, `phantom_ghost_possess_data`, `phantom_possesser_uid`, `possess_unit`, `possessed_girl_unit`
- **矿工 (Miner)**: `monkey_uid`
- **杰克 (Jack) — fog**: `spy_target_uid`, `spy_energy_speed`
- **前锋 (Forward)**: `parkour_start_pos`, `parkour_end_pos`
- **祭司 (Priestess)**: `minder_sender_uid`, `minder_receiver_uid`
- **摄影 (Photographer)**: `out_photo_uid`, `opposite_face_models`
- **囚徒 (Prisoner)**: `switch_uid`
- **舞女 (Dancer)**: `meditation_speed`
- **白手白祭 (Whitedective)**: `watched_by_whitedective_uid`
- **杰克剑士 (Swordman)**: `swordman_speed_up_ratio`
- **蜡像师 (WaxArtist)**: `wax_frozen_uids`
- **木偶 (Puppet)**: `puppet_uid`
- **跛脚机械师 (Mechanic)**: `tramcar_uid`, `tramcar_uid_store`

Every role stamps state through the `CivilianUnit` even when it's a
different survivor role — makes passive observation of others trivial.

### Generator / machine fields

- `state` (int): `1` = idle/not decoded, `3` = being decoded, other values for overcharge / failure states
- `progress` / `charging_progress` (float): decode %
- `decoder_uids` / `charging_uids`: list of civilian uids working on it

### Hook (rocket) states

16 hooks per default map. `state`:
- `1` = standby / empty
- `3` = someone is strapped
- other values for in-countdown / depleted

### Door state

`door_info = [uid_A, uid_B]` on UnitManager — the two gate UIDs.

---

## 4. WorldBattle / CombatCore

**`WorldBattle`** — exactly one per active match.
```py
wb = next(o for o in gc.get_objects() if type(o).__name__ == 'WorldBattle')
```
References every UnitManager + the combat core + gamemode data.

**Combat-core UnitManager** (second UnitManager instance, interface-different):

| field                  | meaning |
|------------------------|---------|
| `game_time`            | seconds since match start (float) |
| `game_frame`           | frame counter |
| `instance_id`          | 6 observed |
| `instance_type`        | `'UnitManager'` |
| `params`               | `{instance_group: ManagerType, ...}` |
| `ref_bind_player`      | **often `None`** — unreliable for localplayer |
| `ref_gamemode_data`    | `GameModeDataNormal` or similar |
| `ref_room`             | `WorldBattle` back-reference |
| `ref_GK`               | `<module 'game_kernel' (script:/game_kernel.nxs)>` |
| `instances` / `instances_by_type` | combat-core unit registry |
| `instances_for_tick`   | tick-enabled subset |
| `player_unit_types`    | `(2, 1)` — civilian + butcher |
| `destroyed`            | flipped at match end |
| `inited` / `late_initd`| init lifecycle |

`game_kernel.nxs` is the native Python extension (script:/game_kernel.nxs)
that provides low-level combat primitives.

---

## 5. Manager singletons (complete inventory)

**157 manager types, 405 live instances** discovered via gc scan for
names ending in `Mgr` / `Manager` / `Factory`. Highest-value ones:

### World / game-state
- `WorldManager`            — world root
- `WorldCacheMgr`           — world cache
- `UnitManager` (×2)        — in-match units
- `ComponentMgr`            — ECS components
- `SpaceManager`            — spatial partitioning
- `NavManager`              — pathfinding graph
- `CollisionManager`        — collision
- `SceneMusicMgr`           — scene audio
- `RegionManager`           — map regions

### Combat
- `CombatUnitManager`       — combat core units
- `GameplayEffectManager`   — gameplay effects
- `GameplayTagMgr`          — gameplay tags
- `SkillManager`            — skill registry
- `SkillEffectManager` (×27)— per-live-effect
- `SkillModifyManager` (×5) — mod stack
- `BuffMgr` (×4)            — buff stack
- `BattleSkillEnhanceManager` (×5) — enhancement stack
- `PropsMgr`                — pickups / props
- `MoveSpeedMgr` (×5)       — speed modifier stack
- `OutlinePrioMgr` (×154)   — outline priority per outlined object

### Role-specific
- `HypnosisPhantomMgr`      — 催眠师 phantom
- `MinerShadowMgr`          — 矿工 shadow
- `FlashlightMgr`           — flashlight
- `GardenerShieldMgr`       — 园丁 shield
- `MagnetPoleMgr`           — magnet role
- `FlySpringBoardMgr` (×4)  — springboard
- `ThrowTraceMgr` (×3)      — projectile trajectory
- `TrajectoryMgr` (×3)      — path trails
- `HeartBeatMgr` (×0?)
- `JingleBellMgr`           — bell item
- `MermaidSFXManager`       — mermaid role
- `MoonManager`             — lunar cycle / moon role

### Effects / rendering
- `UIManager` / `UIPlistManager` / `UIWidgetManager` / `UISeqMgr`
- `PostProcessManager`      — post-FX
- `WeatherManager` / `WeatherParticleManager`
- `SunFlareMgr`             — lens flare
- `FogManager`              — fog
- `ClothDecorationEffectManager` (×5) / `ClothInteractionManager` (×5)
- `SFXManager`              — sound effects
- `ShaderCacheWarmUpMgr`
- `DissolveManager` / `DissolveParamMgr`
- `ChangeDiffuseManager`    — diffuse swap
- `FilterExpandManager` / `GrayExpandManager` / `OutlineManager` / `PostProcessManager`
- `OfferingEffectManager` (×5) — buff / offering effects
- `MagicFieldManager` (×5)
- `EmojiDropMgr`            — emoji drop visuals
- `ShowAnimationManager` (×5) — cinematic animation playback

### Input / UI
- `KeyBoardManager` / `KeyHandlerMgr` / `KeyCachedMgr` (×6)
- `GlobalTouchManager`
- `MouseManager`
- `PCHotkeyManager`
- `HintManager` / `CheckHintMgr`
- `ControllerSensitivityMgr` (×6)
- `InputHistoryManager`

### Data / resources
- `DataManager` / `DataCacheManager` / `SettingDataManager` / `ItemMgr`
- `CharDayTemplateMgr`
- `CouplesManager` / `CouplePetManager`
- `CustomAlbumMgr` / `CustomSchemeMgr` / `CustomVolumeMgr`
- `ModelEffectDataMgr` / `ModelEffectMgr`
- `LinePortraitMgr`
- `JobDescMgr` / `JobNumMgr` / `CloneCreateMgr`
- `HotfixMgr`               — live patch applier
- `PatchLogMgr` / `PatchSDKMgr`
- `NewPlayerResRecordMgr`   — tutorial / FTUE resources
- `ResObjMgr` / `ResAsyncLoadMgr` / `ResBlackListMgr` / `ResWhiteListMgr` / `ResFileIDRecordMgr`
- `ResPreloadManager`

### Networking / integration
- `HttpMgr` (not explicitly in current listing, check via `sys.modules`)
- `ExceptionRecordMgr`
- `UnusualTIDRecordMgr` / `TIDRecordMgr`
- `TracebackManager` (×2)
- `SDKManager`              — NetEase UniSDK facade
- `JankTraceMgr` / `LagMgr` — perf tracing

### Anti-cheat / security (user-authored)
- **`CheatValidateMgr`**    — **the user's anti-cheat validation entry point**
- `ExceptionRecordMgr`
- `RawTraceRouteMgr`
- `MultiOpenMgr`            — multi-client detection
- `core.AntiMemModifyMgr` (module; scan for instances)

### Camera
- `CameraAnimMgr` (×6)     — one per active animation
- `CameraAnimFileMgr` (×6)

### Tick / logic
- `MontTickManager`         — monotonic tick
- `MontEventManager`        — monotonic events
- `EventMgr`                — central event bus
- `EventDispatcher`
- `StorylineManager`        — narrative / cutscenes
- `UniCineManager`          — cinematics
- `TalkMgr`                 — dialogue
- `TimerManager` (×2) / `TimerMgr`

### Debug / dev
- `AutotestMgr`             — automated playback
- `RecordMgr` / `RecordMgrMgr` / `VideoRecordMgr`
- `JankTraceMgr` / `LagMgr`
- `DcToolMgr`               — developer console tool
- `DevSTMgr`
- `DebugDrawInfo` (inferred from UnitManager.debug_*)

---

## 6. Module layout

~235 NeoX / game modules loaded at startup. Top-level namespaces:

| namespace          | what's there |
|--------------------|--------------|
| `core.*`           | engine-facing managers (AntiMemModifyMgr, AsyncManager, Scene, Camera, Particle, UI, …) |
| `common.*`         | shared types (entities, stats, protobuf stubs, formulas) |
| `common_cs.*`      | combat-core — combat entities, states, datas, enum_types, mgr |
| `common_tools.*`   | dev tooling — CameraAnim, Swallow, debug handlers |
| `mobile.*`         | protobuf-compiled message types from our schema recovery |
| `neox.*`           | top-level engine bindings — nxcore, nxgui, nxdependency, xml, ccwidgets, xwidgets |
| `world.*`          | native renderer module — camera, scene, lights |
| `world2.*`         | engine next-gen scene graph — LegacyCameraCut, etc. |
| `game_kernel`      | native Python extension at `script:/game_kernel.nxs` — combat primitives |
| `ClientUnits.*`    | unit-type classes (CivilianUnit, ButcherUnit, CityUnits.MyCityUnit, PetUnit, DoorUnit, PanelUnit, ...) |
| `worlds.*`         | world-mode classes (WorldBattle, WorldHall, ...) |
| `entities.logic.*` | story NPCs / hall residents (e.g. FriendLogic.NPCConanDoyle) |
| `ui.*`             | every ImGui-free UI widget in the game |
| `PatchManager.*`   | live-patch application system |
| `mbengine.*`       | mobile engine — genrpc, common |
| `common_sockets.*` | socket/network helpers |

---

## 7. Networking

**Framework**: `asiocore_py3 v2.0.26`
**Build path** (leaked from error strings):
```
D:\conan\data\asiocore_py3\2.0.26\H55\stable\source\asiocore\...
```

**Transport**: KCP (reliable-UDP) running redundantly across **5 parallel
gate IPs**. Every few seconds `EchoGateClient` reconnects/rotates:

Observed IP pool (rolling):
```
101.71.17.92:10030      113.57.240.174:20117
117.135.223.179:10034   171.43.242.227:20107
220.197.36.107:11008    223.76.113.130:20162
45.253.224.143:10013    47.109.202.47:20007
47.112.163.184:20267    39.96.208.6:20167
39.175.129.153:12018    119.96.138.74:20252
1.95.233.58:10031
```

**Client ID** format: `69e1ba23886cea12d4e31681` (MongoDB-ObjectId-like).

**Logging funnel**: every RPC stub routes its fully qualified name through
`neox_engine.dll +0x30DB100` (`sub_1830DB100`) — that's the global logger
hook point and the DXSense RPC tracer hooks exactly this.

**Client/server RPC stubs**: `GenClientServerRpcStub` and
`GenClientSoulRpcStub` (from `mbengine.common.genrpc`) handle request /
reply serialisation using the recovered protobuf messages (see
`analysis/protos/mobile/server/*.proto`).

---

## 8. User-authored security layer

Confirmed runtime presence of the user's own AC validation:

| thing                      | location |
|----------------------------|----------|
| `CheatValidateMgr`         | one live instance (`gc` scan) |
| `core.AntiMemModifyMgr`    | module (scan instances to find runtime object) |
| `ExceptionRecordMgr`       | exception capture |
| `MultiOpenMgr`             | multi-open detection |
| `Avatar.security_lock_key` | `{19:'chat_lobby_lock', 20:'chat_friend_lock'}` |

Because DXSense loads via a whitelisted injector and operates in-process,
it doesn't trip these — the AC validates against specific external
interference patterns, not against additive in-process Python/C++ from a
trusted source.

---

## 9. How to query any of this

`scripts/dxs_remote.py` — the HTTP client to DXSense's RemoteBridge.

```bash
python scripts/dxs_remote.py --probe              # canonical kitchen-sink dump
python scripts/dxs_remote.py -c "..."             # inline snippet
python scripts/dxs_remote.py path/to/file.py     # snippet from file
python scripts/dxs_remote.py < /dev/stdin         # heredoc
```

Isolated output — the bridge wraps every probe in a per-call stdout
redirect so the game's own `logging` module noise (EchoGate chatter,
various INFO logs) doesn't pollute results.

See `analysis/NEOX3_PROBES.md` for the reusable snippet catalog.
