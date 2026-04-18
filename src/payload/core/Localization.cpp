#include "Localization.hpp"

#include "Config.hpp"

#include <unordered_map>

namespace dxs {

namespace {

struct Entry {
    std::string_view key;
    std::string_view en;
    std::string_view zh_CN;
};

constexpr Entry kStrings[] = {
    // v3 category taxonomy. See Overlay::register_default_panels for the
    // panel → category mapping.
    {"sidebar.core",                      "Core",                        "核心"},
    {"sidebar.inspection",                "Inspection",                  "查看"},
    {"sidebar.scripting",                 "Scripting",                   "脚本"},
    {"sidebar.functional",                "Functional",                  "功能"},
    {"sidebar.lab",                       "Lab",                         "实验室"},
    {"sidebar.settings",                  "Settings",                    "设置"},
    // Legacy keys still referenced by a few panels until they're migrated.
    {"sidebar.runtime",                   "Core",                        "核心"},
    {"sidebar.analysis",                  "Inspection",                  "查看"},
    {"sidebar.modules",                   "Functional",                  "功能"},
    {"sidebar.hud",                       "Functional",                  "功能"},
    {"sidebar.overlay",                   "Functional",                  "功能"},

    {"panel.overview.title",              "Overview",                    "概览"},
    {"panel.hooks.title",                 "Hooks",                       "Hooks"},
    {"panel.entities.title",              "Entities",                    "实体"},
    {"panel.matrix.title",                "Matrix",                      "矩阵"},
    {"panel.raycast.title",               "Raycast",                     "射线"},
    {"panel.rpc_tracer.title",            "RPC Tracer",                  "RPC 追踪"},
    {"panel.ac_observatory.title",        "AC Observatory",              "AC 观测台"},
    {"panel.memory.title",                "Memory",                      "内存"},
    {"panel.python_repl.title",           "Python REPL",                 "Python REPL"},
    {"panel.quick_actions.title",         "Quick Actions",               "快捷动作"},
    {"panel.settings.title",              "Settings",                    "设置"},
    {"panel.hud.title",                   "HUD Widgets",                 "HUD 组件"},
    {"panel.modules.title",               "Modules",                     "模块"},
    {"panel.velocity_lab.title",          "Velocity Lab",                "速度实验室"},
    {"panel.vuln_lab.title",              "Vuln Lab",                    "漏洞实验室"},
    {"panel.interaction_father.title",    "Interaction Father",          "交互总线"},
    {"vuln_lab.section.scene",            "SCENE",                       "场景"},
    {"vuln_lab.section.gmspeed",          "V16 GM_SPEED OVERRIDE",       "V16 GM_SPEED 覆盖"},
    {"vuln_lab.section.anti",             "V9 ANTICHEAT SUPPRESSION",    "V9 反作弊抑制"},
    {"vuln_lab.section.super_atk",        "V17 SUPER_SPEED_N_ATTACK",    "V17 攻击后加速"},
    {"vuln_lab.section.nohit",            "V18 NO-HIT RAISE SPEED",      "V18 无伤加速"},
    {"vuln_lab.section.cfg",              "V13 CONFIG POISONING",        "V13 配置投毒"},
    {"vuln_lab.section.add_skill",        "V11 DYNAMIC PASSIVE INJECTION","V11 动态被动注入"},
    {"vuln_lab.apply",                    "Apply",                       "应用"},
    {"vuln_lab.off",                      "Off (-1)",                    "关闭 (-1)"},
    {"vuln_lab.auto_clear",               "Auto-clear every 500ms",      "每 500ms 自动清空"},
    {"vuln_lab.restore_defaults",         "Restore defaults",            "恢复默认"},
    {"vuln_lab.panic",                    "PANIC: off all",              "紧急关闭全部"},
    {"vuln_lab.read_failed",              "read failed",                 "读取失败"},
    {"vuln_lab.helper_ready",             "helper ready",                "helper 已就绪"},
    {"vuln_lab.helper_retry",             "helper retrying",             "helper 重试中"},
    {"interaction_father.status_armed",   "hooks armed",                 "已装"},
    {"interaction_father.status_idle",    "hooks idle",                  "未装"},
    {"modules.intro",
     "Everything with a visual is a module. Toggle to enable, gear to "
     "configure, turn on edit-positions to drag widgets around on the viewport.",
     "所有在游戏画面上显示的组件都是模块。开关控制启停，齿轮打开配置，开启 "
     "编辑位置 后可以在画面上拖动调整位置。"},
    {"modules.global",                    "All modules",                 "全部模块"},
    {"modules.edit_positions",            "Edit positions",              "编辑位置"},
    {"modules.reset_position",            "Reset position",              "重置位置"},
    {"modules.reset_size",                "Reset size",                  "重置大小"},
    {"modules.empty_hint",
     "No functional modules registered yet. HUD widgets have moved to the HUD tab.",
     "暂无功能模块。HUD 组件已迁移到 HUD 标签页。"},
    {"velocity_lab.threshold",            "Speed threshold (m/s)",       "速度阈值 (m/s)"},
    {"velocity_lab.spike_dpos",           "Spike Δpos (m)",              "瞬移判定 Δpos (m)"},
    {"velocity_lab.log_anomalies",        "Log anomalies",               "记录异常"},
    {"velocity_lab.empty",                "No units sampled yet.",       "尚未采集到单位数据"},
    {"velocity_lab.select_hint",          "Select a uid from the list.", "从列表选择一个 uid"},

    {"esp.targets",                       "TARGETS",                     "目标"},
    {"esp.overlay",                       "OVERLAY",                     "叠加"},
    {"esp.tuning",                        "TUNING",                      "调节"},
    {"esp.hunters",                       "Hunter",                      "监管者"},
    {"esp.survivors",                     "Survivors",                   "求生者"},
    {"esp.props",                         "Props",                       "道具"},
    {"esp.silhouette",                    "Silhouette",                  "剪影"},
    {"esp.box",                           "Box",                         "外框"},
    {"esp.distance",                      "Distance",                    "距离"},
    {"esp.tracer",                        "Tracer",                      "追踪线"},
    {"esp.dot",                           "Dot",                         "圆点"},
    {"esp.hunter_proximity",              "Hunter proximity glow",       "监管者靠近高亮"},
    {"esp.silhouette_alpha",              "Silhouette alpha",            "剪影不透明度"},
    {"esp.max_dist",                      "Max distance (m)",            "最大距离 (m)"},
    {"esp.tracer_origin",                 "Tracer origin",               "追踪线起点"},
    {"esp.tracer_bottom",                 "Bottom",                      "底部"},
    {"esp.tracer_top",                    "Top",                         "顶部"},
    {"esp.tracer_centre",                 "Centre",                      "中心"},
    {"esp.tracer_crosshair",              "Crosshair",                   "准星"},

    // Unit kind labels — every value seen in the event-log inventory,
    // named per NeoX3's in-game terminology. Unknown IDs fall through to
    // `esp.kind_unknown` which renders as "?" so it's visually obvious.
    {"esp.kind_hunter",                   "HUNTER",                      "监管者"},
    {"esp.kind_survivor",                 "SURVIVOR",                    "求生者"},
    {"esp.kind_generator",                "GEN",                         "密码机"},
    {"esp.kind_hook",                     "HOOK",                        "狂欢之椅"},
    {"esp.kind_box",                      "BOX",                         "箱子"},
    {"esp.kind_door",                     "DOOR",                        "大门"},
    {"esp.kind_pallet",                   "PALLET",                      "板子"},
    {"esp.kind_panel",                    "PANEL",                       "电机板"},
    {"esp.kind_cupboard",                 "LOCKER",                      "储物柜"},
    {"esp.kind_basement",                 "BASEMENT",                    "地窖"},
    {"esp.kind_crow",                     "CROW",                        "乌鸦"},
    {"esp.kind_switch",                   "SWITCH",                      "开关"},
    {"esp.kind_hatch",                    "HATCH",                       "地窖出口"},
    {"esp.kind_lobby",                    "NPC",                         "大厅角色"},
    {"esp.kind_item",                     "ITEM",                        "道具"},
    {"esp.kind_spirit",                   "SPIRIT",                      "亡灵"},
    {"esp.kind_npc",                      "NPC",                         "NPC"},
    {"esp.kind_marker",                   "MARK",                        "标记"},
    {"esp.kind_unknown",                  "?",                           "?"},

    {"overview.framerate",                "FRAMERATE",                   "帧率"},
    {"overview.frame_time",               "FRAME TIME",                  "每帧耗时"},
    {"overview.frames",                   "FRAMES",                      "总帧数"},
    {"overview.subsystems",               "SUBSYSTEMS",                  "子系统"},
    {"overview.intro",
     "DXSense is a debug runtime, not a cheat. Panels on the left expose the "
     "engine through live data.",
     "DXSense 是调试运行时，不是外挂。左侧面板通过实时数据查看引擎内部。"},

    {"common.address",                    "ADDRESS",                     "地址"},
    {"common.run",                        "Run",                         "运行"},
    {"common.clear",                      "Clear",                       "清空"},
    {"common.refresh",                    "Refresh",                     "刷新"},
    {"common.pause",                      "Pause",                       "暂停"},
    {"common.arm_hook",                   "Arm hook",                    "启用 hook"},
    {"common.autoscroll",                 "Autoscroll",                  "自动滚动"},
    {"common.filter",                     "Filter",                      "筛选"},
    {"common.not_yet_wired",
     "Not yet wired — awaiting subsystem survey.",
     "尚未接入 — 等子系统定位结果。"},
    {"common.attached",                   "attached",                    "已接入"},
    {"common.detached",                   "detached",                    "未接入"},
    {"common.active",                     "active",                      "生效"},
    {"common.idle",                       "idle",                        "空闲"},
    {"common.cancel",                     "Cancel",                      "取消"},
    {"common.reset",                      "Reset",                       "重置"},

    {"settings.language",                 "LANGUAGE",                    "语言"},
    {"settings.config_file",              "CONFIG FILE",                 "配置文件位置"},
    {"settings.reset",                    "Reset all settings",          "重置所有设置"},
    {"settings.reset_confirm",
     "Reset all DXSense settings to defaults?",
     "确定要把 DXSense 所有设置恢复为默认吗？"},

    {"hooks.intro",
     "Active detour trampolines managed by HookManager (MinHook). Render-path "
     "hooks install at attach time; analysis hooks install on demand from their "
     "respective panels.",
     "HookManager (MinHook) 管理的活跃 detour trampoline。渲染相关 hook 在注入时"
     "自动装好；分析类 hook 由各自的面板按需启用。"},
    {"hooks.col_target",                  "Target",                      "目标"},
    {"hooks.col_category",                "Category",                    "类别"},
    {"hooks.col_installed",               "Installed",                   "状态"},
    {"hooks.col_notes",                   "Notes",                       "备注"},
    {"hooks.cat_render",                  "render",                      "渲染"},
    {"hooks.cat_input",                   "input",                       "输入"},

    {"rpc_tracer.intro",
     "Hooks the engine's global logger. Every async:: RPC stub funnels its "
     "fully-qualified name through this call, so enabling the hook gives you a "
     "live feed of everything the network layer is about to do — no packet "
     "capture needed.",
     "挂钩引擎的全局日志函数。每个 async:: RPC 桩都把自己的全限定名走这个调用，"
     "启用 hook 就能实时看到网络层所有动作 — 不需要抓包。"},
    {"rpc_tracer.hook_armed",             "hook armed",                  "hook 已启用"},
    {"rpc_tracer.hook_idle",              "hook idle",                   "hook 未启用"},

    {"memory.intro",
     "Read-only hex viewer. Uses VirtualQuery for page-safe reads so unmapped "
     "memory renders as ?? instead of crashing.",
     "只读的十六进制查看器。用 VirtualQuery 做页安全读，未映射的内存显示 ?? 而不"
     "会崩。"},

    {"entities.intro",
     "Live list of every in-world object — Survivors / Hunter / props / cipher "
     "machines / gates / chests / items — with HP, position, team and state. "
     "Auto-refreshes every 2 s while visible.",
     "实时列出地图上一切对象 — 求生者 / 监管者 / 道具 / 密码机 / 大门 / 箱子 / "
     "物品 — 含血量、坐标、阵营和状态。面板可见时每 2 秒自动刷新。"},
    {"entities.auto_refresh",             "Auto",                        "自动刷新"},
    {"entities.categories",               "CATEGORIES",                  "类别筛选"},

    {"matrix.intro",
     "View / projection matrix inspector. Probes a handful of NeoX3 camera "
     "accessors through the in-process Python bridge.",
     "视图 / 投影矩阵查看器。通过进程内 Python bridge 探测 NeoX3 的相机访问接口。"},
    {"matrix.no_source",
     "No matrix source reached. The probe tried core.camera.CameraCtrl, "
     "common_cs.mgr.PhantomCamMgr, and a generic gc scan — none responded with "
     "a valid 4x4. A direct C++ camera singleton read will follow from the "
     "subsystem survey.",
     "没有拿到矩阵。探针已经试过 core.camera.CameraCtrl、common_cs.mgr.PhantomCamMgr"
     "、以及通用 gc 扫描 — 都没返回有效的 4x4 矩阵。等子系统定位出相机单例地址后，"
     "会走 C++ 直读。"},

    {"raycast.intro",
     "Camera-forward raycast for line-of-sight and occlusion analysis. Uses "
     "either the engine's own raycast primitive (once located) or a Python "
     "bridge probe into common NeoX3 physics APIs.",
     "沿相机前向发射射线做视线 / 遮挡分析。优先走引擎自身的 raycast 接口（等定位后），"
     "否则通过 Python bridge 探测 NeoX3 常见物理 API。"},

    {"python_repl.interpreter_attached",  "interpreter attached",        "解释器已接入"},
    {"python_repl.interpreter_idle",      "interpreter unreachable (retrying)",
                                           "解释器未接入 (重试中)"},
    {"python_repl.help",
     "Up/Down for history | Ctrl+L to clear",
     "上下键翻历史 | Ctrl+L 清屏"},

    {"quick.intro",
     "Each button runs the shown snippet inside the game's Python interpreter. "
     "Output streams to the REPL.",
     "每个按钮都把相应片段发到游戏的 Python 解释器里执行，输出流进 REPL 面板。"},
    {"quick.bridge_offline",              "Python bridge not attached — buttons disabled.",
                                          "Python bridge 未接入 — 按钮不可用。"},

    {"hud.intro",
     "In-world overlays rendered on the game viewport rather than inside the "
     "ClickGui window. Toggle widgets, enter edit mode to drag and resize, "
     "positions are persisted to config.",
     "直接画在游戏画面上的 HUD 组件，不在 ClickGui 窗口里。可以开关各组件，进入"
     "编辑模式后拖动和缩放，位置会自动存到配置。"},
    {"hud.edit_mode",                     "Edit mode",                   "编辑模式"},
    {"hud.enter_edit",                    "Enter edit mode",             "进入编辑模式"},
    {"hud.done",                          "Done",                        "完成"},
    {"hud.reset_position",                "Reset position",              "重置位置"},
    {"hud.reset_size",                    "Reset size",                  "重置大小"},
    {"hud.hide",                          "Hide widget",                 "隐藏组件"},
    {"hud.widget_radar",                  "Radar",                       "雷达"},
    {"hud.widget_crosshair",              "Crosshair",                   "准星"},
    {"hud.widget_stats",                  "Stats",                       "状态条"},
    {"hud.widget_esp",                    "ESP",                         "透视"},
    {"hud.widget_entity_esp",             "Entity ESP",                  "实体标注"},
};

std::unordered_map<std::string_view, const Entry*> build_index() {
    std::unordered_map<std::string_view, const Entry*> m;
    m.reserve(std::size(kStrings));
    for (const auto& e : kStrings) m[e.key] = &e;
    return m;
}
const auto& index() {
    static auto m = build_index();
    return m;
}

}  // namespace

Localization::Localization() {
    lang_ = Config::instance().get("i18n.language", "en");
    // Pull back to the ship-default on erase_all so "Restore defaults"
    // actually swaps the UI language instead of leaving zh-CN cached.
    Config::instance().on_reset([this] { lang_ = "en"; });
}

Localization& Localization::instance() {
    static Localization l;
    return l;
}

void Localization::set_language(std::string_view code) {
    lang_ = std::string(code);
    Config::instance().set("i18n.language", lang_);
}

std::string_view Localization::language() const { return lang_; }

std::string_view Localization::t(std::string_view key) const {
    if (auto it = index().find(key); it != index().end()) {
        const Entry* e = it->second;
        if (lang_ == "zh-CN" && !e->zh_CN.empty()) return e->zh_CN;
        return e->en;
    }
    return key;
}

}  // namespace dxs
