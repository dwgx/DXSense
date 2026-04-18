#pragma once

#include <Windows.h>

#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>

// Forward-declared CPython opaque types — we never include Python.h so the
// build stays hermetic and we're immune to the host's exact Python version.
struct _object;    using PyObject = _object;

// CPython ssize_t alias — matches Py_ssize_t on x64 Windows (signed pointer-
// width integer). Using this in the API table lets us bind to PyBytes_* /
// PyUnicode_* signatures without pulling in Python.h.
using DxsPySSizeT = std::intptr_t;

namespace dxs {

// Thin, version-agnostic bridge to the CPython interpreter that NeoX3 embeds.
// The engine DLL re-exports ~1500 Py*/_Py* symbols, so we resolve the ones we
// need at injection time via GetProcAddress and never link to python3.lib.
//
// Design points worth flagging:
//   * All calls take/release the GIL. The engine runs its own Python threads
//     on its gameplay loop; if we touch the interpreter without the GIL we
//     crash immediately.
//   * sys.stdout/sys.stderr get redirected to an in-memory ring buffer via
//     injected Python code, so `print(...)` from the REPL shows up in our
//     overlay without fighting the engine's own logging.
//   * Errors from the last execution are captured with sys.exc_info() and
//     appended to the same buffer prefixed with "!!!".
class PythonBridge {
public:
    static PythonBridge& instance();

    // Resolve CPython symbols off the host DLL. Returns false if the target
    // process isn't actually NeoX3 / doesn't export CPython.
    bool initialize(HMODULE host_module);

    // Run a single statement or expression. Output (stdout/stderr + exception
    // traceback) ends up in the shared output buffer; callers read it via
    // drain_output(). Thread-safe against the engine's Python threads.
    void exec(std::string_view source);

    // exec + immediately grab everything written during THIS call and return
    // it as a single string. Mutex-held across exec + drain so the remote
    // bridge can't interleave with the REPL panel.
    std::string exec_and_collect(std::string_view source);

    // High-performance function call — looks up `module.func` in the
    // interpreter, calls it with no args, and if the return value is a
    // `bytes` object copies its contents into `out`. No stdout detour, no
    // text parsing; under 100 µs per tick when the callee is a pure
    // `struct.pack(...)`. Returns false if anything in the dispatch path
    // fails (module missing, function missing, not bytes, etc.) — callers
    // can fall back to exec_and_collect if they care to diagnose.
    bool call_bytes(const char* module_name,
                    const char* func_name,
                    std::string& out);

    // Copy + clear the captured output. Returns an empty string when nothing
    // new has been produced since the last drain.
    std::string drain_output();

    bool ready() const noexcept { return ready_; }

private:
    PythonBridge() = default;

    // ---- CPython function pointers ---------------------------------------
    // Only the subset we need. Declared as member fields so the whole surface
    // is discoverable in one place.
    struct Api {
        int         (*Py_IsInitialized)()                               = nullptr;
        int         (*PyGILState_Ensure)()                              = nullptr;
        void        (*PyGILState_Release)(int)                          = nullptr;
        int         (*PyRun_SimpleString)(const char*)                  = nullptr;
        PyObject*   (*PyImport_AddModule)(const char*)                  = nullptr;
        PyObject*   (*PyModule_GetDict)(PyObject*)                      = nullptr;
        PyObject*   (*PyDict_GetItemString)(PyObject*, const char*)     = nullptr;
        int         (*PyDict_SetItemString)(PyObject*, const char*, PyObject*) = nullptr;
        PyObject*   (*PyObject_GetAttrString)(PyObject*, const char*)   = nullptr;
        PyObject*   (*PyObject_CallObject)(PyObject*, PyObject*)        = nullptr;
        const char* (*PyUnicode_AsUTF8)(PyObject*)                      = nullptr;
        PyObject*   (*PyUnicode_FromString)(const char*)                = nullptr;
        // Binary-fast path: PyBytes_AsStringAndSize hands us a pointer
        // into the bytes object's internal buffer. Py_DecRef drops the
        // return-value ref after we've memcpy'd out. Both are part of the
        // stable CPython ABI and are in NeoX3's re-export table.
        int         (*PyBytes_AsStringAndSize)(PyObject*, char**, DxsPySSizeT*) = nullptr;
        void        (*Py_DecRef)(PyObject*)                             = nullptr;
        void        (*PyErr_Print)()                                    = nullptr;
        PyObject*   (*PyErr_Occurred)()                                 = nullptr;
        void        (*PyErr_Clear)()                                    = nullptr;
    } api_;

    // RAII GIL guard — nested acquire/release is safe by spec.
    struct GilLock {
        PythonBridge* b;
        int           state;
        explicit GilLock(PythonBridge* b_) : b(b_), state(b_->api_.PyGILState_Ensure()) {}
        ~GilLock() { b->api_.PyGILState_Release(state); }
    };

    bool install_stdout_bridge();

    HMODULE           host_     = nullptr;
    bool              ready_    = false;
    std::mutex        buf_mtx_;
    std::string       buffer_;     // captured prints / tracebacks
    std::mutex        exec_mtx_;   // serialises exec_and_collect so remote
                                    // and REPL don't tear each other's output.
};

}  // namespace dxs
