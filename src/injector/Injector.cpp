// dxs_inject: minimal LoadLibraryW-based remote injection.
//
// Usage:
//   dxs_inject <process.exe|PID> [path\to\DXSense.dll]
//
// If the DLL path is omitted we probe the injector's own directory for
// DXSense.dll — handy during dev when both artifacts sit side-by-side in
// build\bin.

#include <Windows.h>
#include <TlHelp32.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>

namespace fs = std::filesystem;

namespace {

void perr(const wchar_t* what) {
    const DWORD e = GetLastError();
    wchar_t buf[512]{};
    FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, e, 0, buf, 512, nullptr);
    std::fwprintf(stderr, L"[!] %s: (%lu) %s\n", what, e, buf);
}

DWORD find_pid_by_name(std::wstring_view exe) {
    const HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    DWORD pid = 0;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, exe.data()) == 0) {
                pid = pe.th32ProcessID;
                break;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return pid;
}

// Find the remote HMODULE of a DLL by its exe-file name (e.g. "DXSense.dll").
// Walks the target's module list via CreateToolhelp32Snapshot — doesn't
// require any APIs injected into the target. Returns nullptr if absent.
HMODULE find_remote_module(DWORD pid, std::wstring_view dll_name) {
    const HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap == INVALID_HANDLE_VALUE) return nullptr;
    MODULEENTRY32W me{};
    me.dwSize = sizeof(me);
    HMODULE out = nullptr;
    if (Module32FirstW(snap, &me)) {
        do {
            if (_wcsicmp(me.szModule, dll_name.data()) == 0) {
                out = me.hModule;
                break;
            }
        } while (Module32NextW(snap, &me));
    }
    CloseHandle(snap);
    return out;
}

// Issue one remote FreeLibrary(hmod). Kernel32 lives at the same base across
// 64-bit processes of the same arch, so FreeLibrary's VA in our process is
// valid in theirs — same trick the LoadLibraryW injection below uses.
bool remote_free_library(HANDLE proc, HMODULE hmod) {
    const auto freelib = reinterpret_cast<LPTHREAD_START_ROUTINE>(
        GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "FreeLibrary"));
    if (!freelib) return false;
    const HANDLE th = CreateRemoteThread(proc, nullptr, 0,
                                          freelib, hmod, 0, nullptr);
    if (!th) return false;
    WaitForSingleObject(th, 5000);
    DWORD rc = 0;
    GetExitCodeThread(th, &rc);
    CloseHandle(th);
    return rc != 0;
}

// Drop every lingering reference to the named DLL in the target. Handles the
// "interrupted eject" zombie case: the payload's self-eject splash plays for
// a short budget before it calls FreeLibraryAndExitThread; if the user hits
// the injector during that window, LoadLibraryW bumps the refcount — the
// eject then decrements it back and the DLL stays loaded but dead. Further
// injects just increment the refcount on that dead module with no DllMain
// call, so Engine::start never re-runs.
//
// Strategy: if the module is present, WAIT up to 3.5 s for an in-flight
// eject_worker to complete its own FreeLibraryAndExitThread. That's the
// graceful path — we don't want to yank the DLL out from under the eject
// thread (its code pages would be unmapped mid-execution → AV). Only after
// the grace period lapses do we force-FreeLibrary: at that point the DLL
// is presumed truly zombie (eject crashed, or double-injected etc.) and
// there's no live thread inside it to crash.
void ensure_clean_slate(HANDLE proc, DWORD pid, const std::wstring& dll_name) {
    HMODULE existing = find_remote_module(pid, dll_name);
    if (!existing) return;

    std::wprintf(L"[*] %s already in target (handle 0x%p) — waiting for any "
                 L"in-flight eject to complete\n", dll_name.c_str(), existing);

    // Graceful wait — eject_worker typically finishes in ~2.6 s.
    for (int ms = 0; ms < 3500; ms += 100) {
        Sleep(100);
        existing = find_remote_module(pid, dll_name);
        if (!existing) {
            std::wprintf(L"[+] prior instance unloaded cleanly after %d ms\n", ms + 100);
            return;
        }
    }

    // Module is still present after the grace window → assume zombie and
    // forcibly decrement every ref we can see.
    std::wprintf(L"[!] %s did not unload within grace window — force-unloading zombie\n",
                 dll_name.c_str());
    for (int guard = 0; guard < 16; ++guard) {
        existing = find_remote_module(pid, dll_name);
        if (!existing) return;
        if (!remote_free_library(proc, existing)) break;
    }
    HMODULE still = find_remote_module(pid, dll_name);
    if (still) {
        std::fwprintf(stderr,
            L"[!] %s is still loaded after 16 FreeLibrary attempts — the prior "
            L"instance may be pinned. Restart the target to clean up.\n",
            dll_name.c_str());
    }
}

bool inject(DWORD pid, const fs::path& dll) {
    const HANDLE proc = OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION |
        PROCESS_VM_WRITE     | PROCESS_VM_READ      | PROCESS_QUERY_INFORMATION,
        FALSE, pid);
    if (!proc) { perr(L"OpenProcess"); return false; }

    ensure_clean_slate(proc, pid, dll.filename().wstring());

    const std::wstring full = fs::absolute(dll).wstring();
    const SIZE_T bytes = (full.size() + 1) * sizeof(wchar_t);

    LPVOID remote = VirtualAllocEx(proc, nullptr, bytes,
                                   MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote) { perr(L"VirtualAllocEx"); CloseHandle(proc); return false; }

    if (!WriteProcessMemory(proc, remote, full.c_str(), bytes, nullptr)) {
        perr(L"WriteProcessMemory");
        VirtualFreeEx(proc, remote, 0, MEM_RELEASE);
        CloseHandle(proc);
        return false;
    }

    // kernel32 is mapped at the same base in 64-bit processes of the same
    // architecture, so LoadLibraryW's address in our process is valid in theirs.
    const auto load = reinterpret_cast<LPTHREAD_START_ROUTINE>(
        GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW"));
    if (!load) { perr(L"GetProcAddress(LoadLibraryW)"); CloseHandle(proc); return false; }

    const HANDLE th = CreateRemoteThread(proc, nullptr, 0, load, remote, 0, nullptr);
    if (!th) { perr(L"CreateRemoteThread"); CloseHandle(proc); return false; }

    WaitForSingleObject(th, 10000);

    DWORD exit_code = 0;
    GetExitCodeThread(th, &exit_code);   // == HMODULE low 32 bits on success.
    CloseHandle(th);
    VirtualFreeEx(proc, remote, 0, MEM_RELEASE);
    CloseHandle(proc);

    if (!exit_code) {
        std::fwprintf(stderr, L"[!] LoadLibraryW returned NULL inside the target.\n"
                              L"    Check DLL bitness matches target, and that "
                              L"all transitive deps (MSVC runtime etc.) resolve.\n");
        return false;
    }
    std::wprintf(L"[+] injected OK — module handle in target: 0x%08X\n", exit_code);
    return true;
}

bool grant_debug_privilege() {
    HANDLE token{};
    if (!OpenProcessToken(GetCurrentProcess(),
                          TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token)) return false;
    LUID luid{};
    if (!LookupPrivilegeValueW(nullptr, SE_DEBUG_NAME, &luid)) {
        CloseHandle(token); return false;
    }
    TOKEN_PRIVILEGES tp{};
    tp.PrivilegeCount           = 1;
    tp.Privileges[0].Luid       = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    const BOOL ok = AdjustTokenPrivileges(token, FALSE, &tp, sizeof(tp), nullptr, nullptr);
    CloseHandle(token);
    return ok && GetLastError() == ERROR_SUCCESS;
}

fs::path default_dll_path() {
    wchar_t buf[MAX_PATH]{};
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return fs::path(buf).parent_path() / L"DXSense.dll";
}

int usage() {
    std::fwprintf(stderr,
        L"Usage: dxs_inject <process.exe|PID> [DXSense.dll]\n"
        L"Example: dxs_inject dwrg.exe\n");
    return 2;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc < 2) return usage();

    grant_debug_privilege();  // best-effort; OK to fail.

    const std::wstring target = argv[1];
    DWORD pid = 0;
    if (std::all_of(target.begin(), target.end(), iswdigit)) {
        pid = static_cast<DWORD>(std::wcstoul(target.c_str(), nullptr, 10));
    } else {
        pid = find_pid_by_name(target);
    }
    if (!pid) {
        std::fwprintf(stderr, L"[!] target not found: %s\n", target.c_str());
        return 1;
    }

    const fs::path dll = (argc >= 3) ? fs::path(argv[2]) : default_dll_path();
    if (!fs::exists(dll)) {
        std::fwprintf(stderr, L"[!] dll not found: %s\n", dll.c_str());
        return 1;
    }

    std::wprintf(L"[*] target PID: %lu\n[*] DLL: %s\n", pid, dll.c_str());
    return inject(pid, dll) ? 0 : 1;
}
