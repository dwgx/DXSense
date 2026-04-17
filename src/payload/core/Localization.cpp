#include "Localization.hpp"

#include "Config.hpp"

#include <unordered_map>

namespace dxs {

namespace {

// Compile-time translation table. Adding a new language = a new column.
// Keeping English as key-identity: the fallback already works for the en
// case, so we only store non-English entries.
struct Entry {
    std::string_view key;
    std::string_view en;       // also the default if a language is missing
    std::string_view zh_CN;
};

constexpr Entry kStrings[] = {
    {"sidebar.runtime",                   "Runtime",                     "运行时"},
    {"sidebar.analysis",                  "Analysis",                    "分析"},
    {"sidebar.scripting",                 "Scripting",                   "脚本"},
    {"sidebar.settings",                  "Settings",                    "设置"},

    {"panel.overview.title",              "Overview",                    "概览"},
    {"panel.hooks.title",                 "Hooks",                       "Hooks"},
    {"panel.entities.title",              "Entities",                    "实体"},
    {"panel.matrix.title",                "Matrix",                      "矩阵"},
    {"panel.raycast.title",               "Raycast",                     "射线"},
    {"panel.rpc_tracer.title",            "RPC Tracer",                  "RPC Tracer"},
    {"panel.memory.title",                "Memory",                      "内存"},
    {"panel.python_repl.title",           "Python REPL",                 "Python REPL"},
    {"panel.quick_actions.title",         "Quick Actions",               "快动"},
    {"panel.settings.title",              "Settings",                    "设置"},

    {"overview.framerate",                "FRAMERATE",                   "帧率"},
    {"overview.frame_time",               "FRAME TIME",                  "帧时间"},
    {"overview.frames",                   "FRAMES",                      "帧"},
    {"overview.subsystems",               "SUBSYSTEMS",                  "子系统"},
    {"overview.intro",
     "DXSense is a debug runtime, not a cheat. Panels on the left expose the "
     "engine through live data.",
     "DXSense 是调试运行时，而非作弊工具。左侧面板通过实时数据展现引擎内部。"},

    {"common.address",                    "ADDRESS",                     "地址"},
    {"common.run",                        "Run",                         "运行"},
    {"common.clear",                      "Clear",                       "清除"},
    {"common.refresh",                    "Refresh",                     "刷新"},
    {"common.pause",                      "Pause",                       "暂停"},
    {"common.arm_hook",                   "Arm hook",                    "部署 hook"},
    {"common.autoscroll",                 "Autoscroll",                  "自动滚动"},
    {"common.filter",                     "Filter",                      "筛选"},
    {"common.not_yet_wired",
     "Not yet wired — awaiting subsystem survey.",
     "尚未连接 — 等待子系统调查。"},
    {"common.attached",                   "attached",                    "已连接"},
    {"common.detached",                   "detached",                    "已分离"},

    {"settings.language",                 "Language",                    "语言"},
    {"settings.reset",                    "Reset all settings",          "重置所有设置"},
    {"settings.reset_confirm",
     "Reset all DXSense settings to defaults?",
     "确定将所有 DXSense 设置重置为默认值吗？"},

    {"rpc_tracer.intro",
     "Hooks the engine's global logger. Every async:: RPC stub funnels its "
     "fully-qualified name through this call, so enabling the hook gives you a "
     "live feed of everything the network layer is about to do — no packet "
     "capture needed.",
     "挂钩引擎的全局日志器。每个 async:: RPC stub 调用都会通过此机制传递其完整限定名，"
     "启用该 hook 可实时查看网络层的所有操作 — 无需抓包。"},
    {"rpc_tracer.hook_armed",             "hook armed",                  "hook 已部署"},
    {"rpc_tracer.hook_idle",              "hook idle",                   "hook 空闲"},

    {"memory.intro",
     "Read-only hex viewer. Use VirtualQuery for page-safe reads so unmapped "
     "memory renders as ?? instead of crashing.",
     "只读十六进制查看器。使用 VirtualQuery 进行页安全读取，未映射内存将显示为 ?? "
     "而非导致崩溃。"},

    {"entities.intro",
     "Live list of every game entity — Survivors / Hunter / props / repair "
     "machines / gates — with HP, position, and RPC mailbox handle.",
     "实时列出所有游戏实体 (求生者 / 监管者 / 道具 / 密码机 / 大门) 及其 HP、位置和 "
     "RPC 邮箱句柄。"},

    {"python_repl.interpreter_attached",  "interpreter attached",        "解释器已连接"},
    {"python_repl.interpreter_idle",      "interpreter unreachable (retrying)",
                                           "解释器不可达 (正在重试)"},
    {"python_repl.help",
     "Up/Down history · Ctrl+L clear",
     "上/下翻阅历史 · Ctrl+L 清空"},
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
    // Language choice persists across sessions. Default remains English —
    // that way a fresh install is dev-legible globally.
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
        if      (lang_ == "zh-CN" && !e->zh_CN.empty()) return e->zh_CN;
        return e->en;
    }
    return key;   // graceful fallback: render the raw key so missing strings stand out.
}

}  // namespace dxs
