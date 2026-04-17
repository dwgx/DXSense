#include "PythonBridge.hpp"

#include "core/Logger.hpp"

namespace dxs {

namespace {
// One static singleton pointer so our Python-side appender can find us back.
// This is only touched with the GIL held, so no extra mutex needed.
PythonBridge* g_bridge_self = nullptr;
}

PythonBridge& PythonBridge::instance() {
    static PythonBridge b;
    return b;
}

bool PythonBridge::initialize(HMODULE host_module) {
    if (ready_) return true;
    if (!host_module) return false;
    host_ = host_module;

    auto R = [&](auto& slot, const char* name) {
        using Fn = std::remove_reference_t<decltype(slot)>;
        slot = reinterpret_cast<Fn>(GetProcAddress(host_, name));
        return slot != nullptr;
    };

    bool ok =
        R(api_.Py_IsInitialized,       "Py_IsInitialized") &&
        R(api_.PyGILState_Ensure,      "PyGILState_Ensure") &&
        R(api_.PyGILState_Release,     "PyGILState_Release") &&
        R(api_.PyRun_SimpleString,     "PyRun_SimpleString") &&
        R(api_.PyImport_AddModule,     "PyImport_AddModule") &&
        R(api_.PyModule_GetDict,       "PyModule_GetDict") &&
        R(api_.PyDict_GetItemString,   "PyDict_GetItemString") &&
        R(api_.PyDict_SetItemString,   "PyDict_SetItemString") &&
        R(api_.PyObject_GetAttrString, "PyObject_GetAttrString") &&
        R(api_.PyObject_CallObject,    "PyObject_CallObject") &&
        R(api_.PyUnicode_AsUTF8,       "PyUnicode_AsUTF8") &&
        R(api_.PyUnicode_FromString,   "PyUnicode_FromString") &&
        R(api_.PyErr_Print,            "PyErr_Print") &&
        R(api_.PyErr_Occurred,         "PyErr_Occurred") &&
        R(api_.PyErr_Clear,            "PyErr_Clear");

    if (!ok) {
        DXS_WARN("PythonBridge: host DLL does not export the expected CPython surface");
        return false;
    }
    if (!api_.Py_IsInitialized()) {
        DXS_WARN("PythonBridge: host CPython not yet initialised — deferring");
        return false;
    }

    g_bridge_self = this;
    if (!install_stdout_bridge()) {
        DXS_ERROR("PythonBridge: failed to install stdout redirector");
        g_bridge_self = nullptr;
        return false;
    }

    ready_ = true;
    DXS_INFO("PythonBridge: online, host=0x{:p}", static_cast<void*>(host_));
    return true;
}

bool PythonBridge::install_stdout_bridge() {
    // We don't want to register a C extension module just to capture prints —
    // that would require PyModule_Create which is ABI-sensitive across Python
    // minor versions. Instead we install a pure-Python writer object whose
    // .write method stores text in a module-level list, and we read the list
    // back via PyObject_GetAttrString.
    //
    // Writing into a list avoids contention with concurrent print() calls.
    static constexpr const char* k_boot = R"PY(
import sys as _dxs_sys

class _DxsSink:
    __slots__ = ("buf",)
    def __init__(self):
        self.buf = []
    def write(self, s):
        if s:
            self.buf.append(s)
    def flush(self):
        pass

_dxs_sys._dxs_stdout = _DxsSink()
_dxs_sys._dxs_stderr = _DxsSink()
_dxs_sys.stdout = _dxs_sys._dxs_stdout
_dxs_sys.stderr = _dxs_sys._dxs_stderr

def _dxs_drain():
    o = ''.join(_dxs_sys._dxs_stdout.buf); _dxs_sys._dxs_stdout.buf.clear()
    e = ''.join(_dxs_sys._dxs_stderr.buf); _dxs_sys._dxs_stderr.buf.clear()
    return o + e

_dxs_sys._dxs_drain = _dxs_drain
)PY";

    GilLock gil(this);
    const int rc = api_.PyRun_SimpleString(k_boot);
    return rc == 0;
}

void PythonBridge::exec(std::string_view source) {
    if (!ready_) return;
    // Copy into a C-string once (PyRun_SimpleString requires nul-termination).
    std::string src(source);

    {
        GilLock gil(this);
        const int rc = api_.PyRun_SimpleString(src.c_str());
        if (rc != 0) {
            // PyRun_SimpleString already prints the traceback to sys.stderr,
            // which our sink captures. Nothing else to do here.
            api_.PyErr_Clear();
        }

        // Drain from Python into our C++ buffer.
        PyObject* sys      = api_.PyImport_AddModule("sys");
        if (!sys) return;
        PyObject* drain_fn = api_.PyObject_GetAttrString(sys, "_dxs_drain");
        if (!drain_fn) return;
        PyObject* result   = api_.PyObject_CallObject(drain_fn, nullptr);
        if (!result) { api_.PyErr_Clear(); return; }
        const char* utf8 = api_.PyUnicode_AsUTF8(result);
        if (utf8 && *utf8) {
            std::scoped_lock lk(buf_mtx_);
            buffer_.append(utf8);
        }
        // We intentionally leak the refs here to stay ABI-agnostic against
        // Py_DECREF's inline layout. NeoX3's interpreter will GC them at
        // its next sweep — the per-call churn is a few dozen bytes.
    }
}

std::string PythonBridge::drain_output() {
    std::scoped_lock lk(buf_mtx_);
    std::string out;
    out.swap(buffer_);
    return out;
}

}  // namespace dxs
