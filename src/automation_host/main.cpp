// resonant-automation: the headless automation host (REQ-CHAT-20261126-012,
// ADR-000216). One project with its undo history and jobs, the application
// services and the local endpoint; no window. The gRPC bridge
// (python3 -m resonant_api.bridge) or a local client connects to its socket.
//
//   resonant-automation [--socket PATH] [--token-file PATH] [--project FILE]
//                       [--project-dir DIR] [--allow-files] [--admin] [--workers N]
//
// On start it prints one JSON line: {"socket": ..., "tokenFile": ..., "pid": ...}.
// The token itself is only written to the token file (0600), never printed.
#include "Dispatcher.h"
#include "LocalEndpoint.h"
#include "foundation/Version.h"
#include "project/Project.h"
#include "project/ProjectSerializer.h"
#include "services/Services.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <sys/stat.h>
#include <thread>

namespace {

std::atomic<bool> g_stop{false};

void onSignal(int)
{
    g_stop = true;
}

void usage()
{
    std::cout << "resonant-automation " << resonant::kApplicationVersion
              << ": the headless Resonant automation host\n\n"
                 "  --socket PATH      the local endpoint (default: $XDG_RUNTIME_DIR/resonant/automation.sock)\n"
                 "  --token-file PATH  where the access token is written, mode 0600 (default: beside the socket)\n"
                 "                     RESONANT_AUTOMATION_TOKEN sets the token instead of a random one\n"
                 "  --project FILE     open this project (default: a new, empty project)\n"
                 "  --project-dir DIR  the directory remote clients may open and save projects in\n"
                 "  --allow-files      grant remote sessions the files permission (needs --project-dir)\n"
                 "  --admin            grant remote sessions the admin permission (all sessions' audit, all sessions)\n"
                 "  --workers N        simulation worker threads (default 2)\n";
}

}  // namespace

int main(int argc, char** argv)
{
    using namespace resonant;
    namespace fs = std::filesystem;
    std::string socketPath = automation_api::LocalEndpoint::defaultSocketPath();
    std::string tokenFile, projectFile, projectDir;
    bool allowFiles = false, admin = false;
    unsigned workers = 2;
    for (int k = 1; k < argc; ++k) {
        const std::string a = argv[k];
        const auto next = [&]() -> std::string {
            if (k + 1 >= argc) {
                std::cerr << a << " needs a value\n";
                std::exit(2);
            }
            return argv[++k];
        };
        if (a == "--socket")
            socketPath = next();
        else if (a == "--token-file")
            tokenFile = next();
        else if (a == "--project")
            projectFile = next();
        else if (a == "--project-dir")
            projectDir = next();
        else if (a == "--allow-files")
            allowFiles = true;
        else if (a == "--admin")
            admin = true;
        else if (a == "--workers")
            workers = static_cast<unsigned>(std::max(1, std::atoi(next().c_str())));
        else if (a == "--help" || a == "-h") {
            usage();
            return 0;
        } else {
            std::cerr << "unknown option " << a << " (see --help)\n";
            return 2;
        }
    }
    if (allowFiles && projectDir.empty()) {
        std::cerr << "--allow-files needs --project-dir (remote file access stays within one directory)\n";
        return 2;
    }

    std::unique_ptr<Project> project;
    if (!projectFile.empty()) {
        auto loaded = loadProjectFile(projectFile);
        if (!loaded) {
            std::cerr << "the project could not be opened: " << loaded.message() << "\n";
            return 1;
        }
        project = std::move(loaded.value().project);
    } else {
        project = Project::createNew("Automation");
    }
    services::OwnedProjectHost host(std::move(project), workers);
    services::Session session(host, "headless");
    host.setSession(&session);
    if (!projectFile.empty())
        host.setFilePath(fs::absolute(projectFile).string());
    if (!projectDir.empty())
        session.services().project.setProjectDirectory(fs::absolute(projectDir).string());

    std::uint32_t policy = services::kDefaultRemotePermissions;
    if (allowFiles)
        policy |= static_cast<std::uint32_t>(services::Permission::Files);
    if (admin)
        policy |= static_cast<std::uint32_t>(services::Permission::Admin);
    std::mutex owner;  // the session is used by one thread at a time
    automation_api::Dispatcher dispatcher(
        session,
        [&owner](const std::function<void()>& f) {
            std::lock_guard lock(owner);
            f();
        },
        policy);

    std::string token;
    if (const char* fromEnv = std::getenv("RESONANT_AUTOMATION_TOKEN"); fromEnv && *fromEnv)
        token = fromEnv;
    else
        token = automation_api::LocalEndpoint::newToken();
    if (token.empty()) {
        std::cerr << "no random source for the token\n";
        return 1;
    }
    automation_api::LocalEndpoint endpoint(dispatcher);
    if (const std::string error = endpoint.start(socketPath, token); !error.empty()) {
        std::cerr << error << "\n";
        return 1;
    }
    if (tokenFile.empty())
        tokenFile = (fs::path(socketPath).parent_path() / "automation.token").string();
    {
        const mode_t previous = ::umask(0177);
        std::ofstream out(tokenFile, std::ios::trunc);
        out << token << "\n";
        ::umask(previous);
        ::chmod(tokenFile.c_str(), 0600);
        if (!out) {
            std::cerr << "the token file could not be written\n";
            return 1;
        }
    }
    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);
    std::cout << nlohmann::json{{"socket", endpoint.path()}, {"tokenFile", tokenFile}, {"pid", ::getpid()}}.dump() << std::endl;

    // The owner loop: finished jobs are collected here (results, applied workflow changes).
    while (!g_stop) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        std::lock_guard lock(owner);
        session.collectFinishedJobs();
    }
    endpoint.stop();
    std::error_code ec;
    fs::remove(tokenFile, ec);
    // Jobs still running are cancelled with the host.
    for (const JobInfo& info : host.jobs().jobs())
        if (!isFinished(info.state))
            host.jobs().cancel(info.id);
    return 0;
}
