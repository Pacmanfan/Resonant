#include "LocalEndpoint.h"

#include "resonant/transport/v1/local.pb.h"
#include "resonant/v1/common/errors.pb.h"

#include <cerrno>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

namespace resonant::automation_api {

namespace v1 = ::resonant::v1;

namespace {

bool readAll(int fd, char* data, std::size_t size)
{
    while (size > 0) {
        const ssize_t n = ::recv(fd, data, size, 0);
        if (n == 0)
            return false;
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return false;
        }
        data += n;
        size -= static_cast<std::size_t>(n);
    }
    return true;
}

bool writeAll(int fd, const char* data, std::size_t size)
{
    while (size > 0) {
        const ssize_t n = ::send(fd, data, size, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return false;
        }
        data += n;
        size -= static_cast<std::size_t>(n);
    }
    return true;
}

bool readFrame(int fd, std::string& out, std::size_t limit)
{
    unsigned char header[4];
    if (!readAll(fd, reinterpret_cast<char*>(header), 4))
        return false;
    const std::size_t size = header[0] | (header[1] << 8) | (header[2] << 16) | (static_cast<std::size_t>(header[3]) << 24);
    if (size > limit)
        return false;
    out.resize(size);
    return size == 0 || readAll(fd, out.data(), size);
}

bool writeFrame(int fd, const std::string& message)
{
    const std::size_t size = message.size();
    const unsigned char header[4] = {static_cast<unsigned char>(size), static_cast<unsigned char>(size >> 8), static_cast<unsigned char>(size >> 16),
                                     static_cast<unsigned char>(size >> 24)};
    return writeAll(fd, reinterpret_cast<const char*>(header), 4) && writeAll(fd, message.data(), size);
}

bool connected(int fd)
{
    pollfd p{fd, POLLRDHUP, 0};
    if (::poll(&p, 1, 0) < 0)
        return true;
    return (p.revents & (POLLRDHUP | POLLHUP | POLLERR | POLLNVAL)) == 0;
}

bool sameToken(const std::string& a, const std::string& b)
{
    // Constant time in the length of the expected token.
    unsigned char diff = static_cast<unsigned char>(a.size() != b.size());
    for (std::size_t k = 0; k < b.size(); ++k)
        diff |= static_cast<unsigned char>(b[k] ^ (k < a.size() ? a[k] : 0));
    return diff == 0;
}

transport::v1::LocalResponse responseOf(const Reply& r)
{
    transport::v1::LocalResponse out;
    out.set_status(r.status);
    out.set_payload(r.payload);
    out.set_error(r.error);
    return out;
}

Reply refusal(int status, const std::string& message)
{
    v1::Error e;
    e.set_code(status == 16 ? v1::ERROR_CODE_PERMISSION_DENIED : v1::ERROR_CODE_INVALID_ARGUMENT);
    e.set_message(message);
    Reply r;
    r.status = status;
    e.SerializeToString(&r.error);
    return r;
}

}  // namespace

LocalEndpoint::LocalEndpoint(Dispatcher& dispatcher) : dispatcher_(dispatcher) {}

LocalEndpoint::~LocalEndpoint()
{
    stop();
}

std::string LocalEndpoint::newToken()
{
    std::ifstream random("/dev/urandom", std::ios::binary);
    unsigned char bytes[32] = {};
    if (!random.read(reinterpret_cast<char*>(bytes), sizeof bytes))
        return {};
    static const char* hex = "0123456789abcdef";
    std::string out;
    for (unsigned char b : bytes) {
        out += hex[b >> 4];
        out += hex[b & 15];
    }
    return out;
}

std::string LocalEndpoint::defaultSocketPath()
{
    namespace fs = std::filesystem;
    if (const char* runtime = std::getenv("XDG_RUNTIME_DIR"); runtime && *runtime)
        return (fs::path(runtime) / "resonant" / "automation.sock").string();
    return (fs::temp_directory_path() / ("resonant-" + std::to_string(::getuid())) / "automation.sock").string();
}

std::string LocalEndpoint::start(const std::string& socketPath, const std::string& token)
{
    namespace fs = std::filesystem;
    if (running())
        return "the endpoint is already running";
    if (token.size() < 16)
        return "the token is too short (at least 16 characters)";
    sockaddr_un address{};
    if (socketPath.size() >= sizeof(address.sun_path))
        return "the socket path is too long";
    const fs::path dir = fs::path(socketPath).parent_path();
    std::error_code ec;
    fs::create_directories(dir, ec);
    if (ec)
        return "the socket directory could not be created: " + ec.message();
    // The directory must be ours and private.
    struct stat st{};
    if (::stat(dir.c_str(), &st) != 0 || st.st_uid != ::getuid())
        return "the socket directory does not belong to this user";
    ::chmod(dir.c_str(), 0700);
    if (fs::exists(fs::symlink_status(socketPath))) {
        // A socket left by a crash is replaced; anything else is not touched.
        if (!fs::is_socket(fs::symlink_status(socketPath)))
            return "the socket path exists and is not a socket";
        fs::remove(socketPath, ec);
    }
    const int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0)
        return std::string("socket: ") + std::strerror(errno);
    address.sun_family = AF_UNIX;
    std::strncpy(address.sun_path, socketPath.c_str(), sizeof(address.sun_path) - 1);
    const mode_t previous = ::umask(0177);
    const int bound = ::bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof address);
    ::umask(previous);
    if (bound != 0 || ::listen(fd, 16) != 0) {
        const std::string error = std::strerror(errno);
        ::close(fd);
        return "the socket could not listen: " + error;
    }
    ::chmod(socketPath.c_str(), 0600);
    path_ = socketPath;
    token_ = token;
    stop_ = false;
    listenFd_ = fd;
    acceptor_ = std::thread([this] { acceptLoop(); });
    return {};
}

void LocalEndpoint::stop(const std::function<void()>& whileWaiting)
{
    if (listenFd_ < 0)
        return;
    stop_ = true;
    if (acceptor_.joinable())
        acceptor_.join();
    ::close(listenFd_);
    listenFd_ = -1;
    std::list<Worker> workers;
    {
        std::lock_guard lock(mutex_);
        for (int fd : connections_)
            ::shutdown(fd, SHUT_RDWR);  // unblocks reads and writes
    }
    // A call in progress may be waiting for the host's thread: let it run there until every connection ended.
    while (active_.load() > 0) {
        if (whileWaiting)
            whileWaiting();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    {
        std::lock_guard lock(mutex_);
        workers.swap(workers_);
    }
    for (Worker& w : workers)
        if (w.thread.joinable())
            w.thread.join();
    std::error_code ec;
    std::filesystem::remove(path_, ec);
}

void LocalEndpoint::acceptLoop()
{
    while (!stop_) {
        reap();
        pollfd p{listenFd_, POLLIN, 0};
        if (::poll(&p, 1, 200) <= 0)
            continue;
        const int fd = ::accept4(listenFd_, nullptr, nullptr, SOCK_CLOEXEC);
        if (fd < 0)
            continue;
        ucred peer{};
        socklen_t length = sizeof peer;
        if (::getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &peer, &length) != 0 || peer.uid != ::getuid() || active_.load() >= kMaxConnections) {
            ::close(fd);
            continue;
        }
        std::lock_guard lock(mutex_);
        connections_.push_back(fd);
        ++active_;
        auto done = std::make_shared<std::atomic<bool>>(false);
        workers_.push_back({std::thread([this, fd, done] {
                                serve(fd);
                                {
                                    std::lock_guard l(mutex_);
                                    connections_.remove(fd);
                                }
                                ::close(fd);
                                --active_;
                                *done = true;
                            }),
                            done});
    }
}

void LocalEndpoint::reap()
{
    // Joins the connection threads that ended (called by the acceptor, which owns the list's growth).
    std::list<Worker> finished;
    {
        std::lock_guard lock(mutex_);
        for (auto it = workers_.begin(); it != workers_.end();) {
            auto next = std::next(it);
            if (it->done->load())
                finished.splice(finished.end(), workers_, it);
            it = next;
        }
    }
    for (Worker& w : finished)
        w.thread.join();
}

void LocalEndpoint::serve(int fd)
{
    std::string frame;
    if (!readFrame(fd, frame, kMaxFrameBytes))
        return;
    transport::v1::LocalRequest request;
    if (!request.ParseFromString(frame)) {
        writeFrame(fd, responseOf(refusal(3, "the frame is not a LocalRequest")).SerializeAsString());
        return;
    }
    const auto token = request.metadata().find("token");
    if (token == request.metadata().end() || !sameToken(token->second, token_)) {
        writeFrame(fd, responseOf(refusal(16, "a valid token is needed")).SerializeAsString());
        return;
    }
    const services::Interface source = services::Interface::Grpc;
    const MethodInfo* info = dispatcher_.method(request.method());
    if (info && info->streaming) {
        const Reply end = dispatcher_.stream(
            request.method(), request.payload(), source,
            [fd](const std::string& message) {
                if (message.empty())
                    return connected(fd);
                transport::v1::LocalResponse item;
                item.set_payload(message);
                item.set_stream_item(true);
                return writeFrame(fd, item.SerializeAsString());
            },
            &stop_);
        transport::v1::LocalResponse last = responseOf(end);
        last.set_end_of_stream(true);
        writeFrame(fd, last.SerializeAsString());
        return;
    }
    writeFrame(fd, responseOf(dispatcher_.call(request.method(), request.payload(), source)).SerializeAsString());
}

}  // namespace resonant::automation_api
