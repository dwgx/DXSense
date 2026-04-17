#include "RemoteBridge.hpp"

#include "Logger.hpp"
#include "scripting/PythonBridge.hpp"

#include <WinSock2.h>
#include <WS2tcpip.h>

#include <cstring>
#include <string>

namespace dxs {

namespace {

// WSAStartup / WSACleanup are process-global — we do them on-demand from
// start() and defer cleanup until stop().
bool g_wsa_up = false;
void ensure_wsa() {
    if (g_wsa_up) return;
    WSADATA d{};
    if (WSAStartup(MAKEWORD(2, 2), &d) == 0) g_wsa_up = true;
}
}  // namespace

RemoteBridge& RemoteBridge::instance() {
    static RemoteBridge b;
    return b;
}

bool RemoteBridge::start(int port) {
    if (running_.load()) return true;
    ensure_wsa();

    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return false;

    BOOL yes = TRUE;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&yes), sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(static_cast<u_short>(port));
    InetPtonA(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        DXS_WARN("RemoteBridge: bind({}) failed, port busy", port);
        closesocket(s);
        return false;
    }
    if (listen(s, 4) != 0) {
        closesocket(s);
        return false;
    }

    listen_  = static_cast<uintptr_t>(s);
    port_    = port;
    running_.store(true);
    thread_  = std::thread(&RemoteBridge::server_loop, this);
    DXS_INFO("RemoteBridge listening on 127.0.0.1:{}", port);
    return true;
}

void RemoteBridge::stop() {
    if (!running_.load()) return;
    running_.store(false);
    if (listen_) {
        closesocket(static_cast<SOCKET>(listen_));
        listen_ = 0;
    }
    if (thread_.joinable()) thread_.join();
}

void RemoteBridge::server_loop() {
    const SOCKET listen_sock = static_cast<SOCKET>(listen_);

    while (running_.load()) {
        sockaddr_in peer{};
        int         plen  = sizeof(peer);
        SOCKET      client = accept(listen_sock,
                                    reinterpret_cast<sockaddr*>(&peer), &plen);
        if (client == INVALID_SOCKET) { if (!running_.load()) break; continue; }

        // Read as much as we can — HTTP POST bodies for our use case are
        // well under 64 KB. Keep reading until we've seen Content-Length
        // bytes of body or the socket closes.
        std::string req;
        req.reserve(8192);
        char buf[4096];
        for (;;) {
            const int n = recv(client, buf, sizeof(buf), 0);
            if (n <= 0) break;
            req.append(buf, buf + n);

            // Fast-path: if we can see the header terminator and Content-Length
            // bytes of body have arrived, stop.
            const auto hdr_end = req.find("\r\n\r\n");
            if (hdr_end == std::string::npos) continue;
            size_t content_len = 0;
            const auto cl = req.find("Content-Length:");
            if (cl != std::string::npos && cl < hdr_end) {
                content_len = std::strtoul(req.c_str() + cl + 15, nullptr, 10);
            }
            if (req.size() >= hdr_end + 4 + content_len) break;
        }

        // Extract body.
        std::string body;
        const auto hdr_end = req.find("\r\n\r\n");
        if (hdr_end != std::string::npos) body = req.substr(hdr_end + 4);

        std::string out;
        if (body.empty()) {
            out = "DXSense remote bridge ready — POST Python source as body.\n";
        } else {
            out = PythonBridge::instance().exec_and_collect(body);
            if (out.empty()) out = "(no output)\n";
        }

        char hdr[256];
        const int hdr_len = std::snprintf(hdr, sizeof(hdr),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain; charset=utf-8\r\n"
            "Content-Length: %zu\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Connection: close\r\n\r\n",
            out.size());
        send(client, hdr, hdr_len,             0);
        send(client, out.data(), int(out.size()), 0);
        closesocket(client);
    }
}

}  // namespace dxs
