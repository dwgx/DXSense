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
    {"sidebar.runtime",                   "Runtime",                     "运行时"},
    {"sidebar.analysis",                  "Analysis",                    "分析"},
    {"sidebar.scripting",                 "Scripting",                   "脚本"},
    {"sidebar.settings",                  "Settings",                    "设置"},
    {"sidebar.overlay",                   "Overlay",                     "HUD"},

    {"panel.overview.title",              "Overview",                    "概览"},
    {"panel.hooks.title",                 "Hooks",                       "Hooks"},
    {"panel.entities.title",              "Entities",                    "实体"},
    {"panel.matrix.title",                "Matrix",                      "矩阵"},
    {"panel.raycast.title",               "Raycast",                     "射线"},
    {"panel.rpc_tracer.title",            "RPC Tracer",                  "RPC 追踪"},
    {"panel.memory.title",                "Memory",                      "内存"},
    {"panel.python_repl.title",           "Python REPL",                 "Python REPL"},
    {"panel.quick_actions.title",         "Quick Actions",               "快捷动作"},
    {"panel.settings.title",              "Settings",                    "设置"},
    {"panel.hud.title",                   "HUD Editor",                  "HUD 编辑器"},

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
    {"hud.widget_radar",                  "Radar",                       "雷达"},
    {"hud.widget_crosshair",              "Crosshair",                   "准星"},
    {"hud.widget_stats",                  "Stats",                       "状态条"},
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
