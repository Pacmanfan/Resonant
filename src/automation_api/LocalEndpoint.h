// The local endpoint of the automation API (REQ-CHAT-20261126-012,
// ADR-000216): a Unix-domain socket on which the gRPC bridge (and local tools)
// hand calls to the Dispatcher. One call per connection: a request frame, then
// one response frame (unary) or several (streams), each a 4-byte little-endian
// length and a resonant.transport.v1 message.
//
// Access: the socket's directory is private to the user (0700), the socket
// 0600, the peer must run as the same user (SO_PEERCRED), and every request
// carries the endpoint's token. Not a sandbox: whoever holds the token acts
// with the permissions of the sessions it opens.
#pragma once

#include "Dispatcher.h"

#include <atomic>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace resonant::automation_api {

class LocalEndpoint {
public:
    explicit LocalEndpoint(Dispatcher& dispatcher);
    ~LocalEndpoint();
    LocalEndpoint(const LocalEndpoint&) = delete;
    LocalEndpoint& operator=(const LocalEndpoint&) = delete;

    // Listens at `socketPath` (its directory is created private). Returns an error message, or empty.
    std::string start(const std::string& socketPath, const std::string& token);
    // Stops listening, ends the connections and waits for them. `whileWaiting` runs repeatedly
    // meanwhile: a host whose executor marshals calls to its own thread (the GUI) processes them there.
    void stop(const std::function<void()>& whileWaiting = {});
    bool running() const { return listenFd_ >= 0; }
    const std::string& path() const { return path_; }
    int activeConnections() const { return active_.load(); }

    static constexpr std::size_t kMaxFrameBytes = 64u << 20;  // a request larger than this is refused
    static constexpr int kMaxConnections = 64;

    // A random token (hex, 256 bits) from the system's random source.
    static std::string newToken();
    // The default socket: $XDG_RUNTIME_DIR/resonant/automation.sock, else a per-user folder in the temp directory.
    static std::string defaultSocketPath();

private:
    void acceptLoop();
    void serve(int fd);

    Dispatcher& dispatcher_;
    std::string path_, token_;
    int listenFd_ = -1;
    std::atomic<bool> stop_{false};
    std::atomic<int> active_{0};
    std::thread acceptor_;
    std::mutex mutex_;
    struct Worker {
        std::thread thread;
        std::shared_ptr<std::atomic<bool>> done;
    };
    std::list<Worker> workers_;
    void reap();
    std::list<int> connections_;
};

}  // namespace resonant::automation_api
