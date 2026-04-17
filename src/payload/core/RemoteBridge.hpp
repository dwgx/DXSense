#pragma once

#include <atomic>
#include <thread>

namespace dxs {

// Minimal single-threaded HTTP POST server bound to 127.0.0.1. Exposes
// one endpoint: POST / — body is treated as Python source, executed in
// the host's embedded interpreter via PythonBridge::exec_and_collect,
// and the captured stdout/stderr is returned as the response body.
//
// This is strictly a LOCAL dev tool. Binding to loopback only means the
// port is unreachable from outside the machine. No auth, no TLS — if the
// user doesn't want it, they turn it off in Settings.
class RemoteBridge {
public:
    static RemoteBridge& instance();

    bool start(int port = 9099);   // returns false if the port is busy
    void stop();                   // joined by Engine::stop

    bool    running() const noexcept { return running_.load(); }
    int     port()    const noexcept { return port_; }

private:
    RemoteBridge() = default;
    void server_loop();

    std::atomic_bool running_{false};
    std::thread      thread_;
    int              port_     = 0;
    uintptr_t        listen_   = 0;   // opaque SOCKET, avoid pulling winsock into header
};

}  // namespace dxs
