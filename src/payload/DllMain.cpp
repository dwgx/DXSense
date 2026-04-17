#include "core/Engine.hpp"

#include <Windows.h>

namespace {

DWORD WINAPI boot_thread(LPVOID module) {
    dxs::Engine::instance().start(module);
    return 0;
}

}  // namespace

BOOL APIENTRY DllMain(HMODULE mod, DWORD reason, LPVOID) {
    switch (reason) {
        case DLL_PROCESS_ATTACH: {
            DisableThreadLibraryCalls(mod);
            // Do NOT do real work inside DllMain under the loader lock.
            // Kick off a worker thread and return immediately.
            if (HANDLE t = CreateThread(nullptr, 0, &boot_thread, mod, 0, nullptr); t) {
                CloseHandle(t);
            }
            return TRUE;
        }
        case DLL_PROCESS_DETACH:
            dxs::Engine::instance().stop();
            return TRUE;
    }
    return TRUE;
}
