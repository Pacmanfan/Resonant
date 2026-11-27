#include "plugins/PluginManager.h"

#include "automation/WorkflowEngine.h"
#include "foundation/Log.h"
#include "plugins/PluginApi.h"

#include <dlfcn.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <map>
#include <sstream>

namespace resonant::plugins {

std::optional<Version> Version::parse(const std::string& text)
{
    Version v;
    char dot1 = 0, dot2 = 0;
    std::istringstream s(text);
    if (!(s >> v.major))
        return std::nullopt;
    if (s >> dot1) {
        if (dot1 != '.' || !(s >> v.minor))
            return std::nullopt;
        if (s >> dot2) {
            if (dot2 != '.' || !(s >> v.patch))
                return std::nullopt;
        }
    }
    if (v.major < 0 || v.minor < 0 || v.patch < 0)
        return std::nullopt;
    return v;
}

std::string Version::str() const { return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch); }

std::string_view toString(PluginState state)
{
    switch (state) {
    case PluginState::Disabled: return "disabled";
    case PluginState::Ready: return "ready";
    case PluginState::Loaded: return "loaded";
    case PluginState::Incompatible: return "incompatible";
    case PluginState::MissingDependency: return "missing dependency";
    case PluginState::Invalid: return "invalid";
    case PluginState::Failed: return "failed";
    }
    return "invalid";
}

Result<PluginManifest> readManifest(const std::filesystem::path& directory)
{
    using R = Result<PluginManifest>;
    const auto file = directory / "plugin.json";
    // Messages name the plugin's folder, not its full path (no private paths in reports).
    const std::string where = directory.filename().string() + "/plugin.json";
    std::ifstream in(file);
    if (!in)
        return R::failure("no plugin.json in " + directory.filename().string());
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(in);
    } catch (const std::exception& e) {
        return R::failure(where + " is not valid JSON: " + e.what());
    }
    try {
        PluginManifest m;
        m.directory = directory;
        m.id = j.at("id").get<std::string>();
        m.name = j.value("name", m.id);
        m.version = j.at("version").get<std::string>();
        m.author = j.value("author", std::string());
        m.description = j.value("description", std::string());
        m.license = j.value("license", std::string());
        const nlohmann::json supported = j.value("resonant", nlohmann::json::object());
        m.resonantMin = supported.value("min", std::string("0.0.0"));
        m.resonantMax = supported.value("max", std::string("999.0.0"));
        m.apiVersion = j.at("apiVersion").get<int>();
        m.kind = j.at("kind").get<std::string>();
        m.entry = j.at("entry").get<std::string>();
        for (const auto& d : j.value("dependencies", nlohmann::json::array()))
            m.dependencies.push_back({d.at("id").get<std::string>(), d.value("min", std::string("0.0.0"))});
        m.requires_ = j.value("requires", std::vector<std::string>());
        m.provides = j.value("provides", std::vector<std::string>());
        m.permissions = j.value("permissions", std::vector<std::string>());
        if (m.id.empty() || m.id.find('/') != std::string::npos)
            return R::failure("the plugin id must be non-empty and contain no '/'");
        if (!Version::parse(m.version))
            return R::failure("plugin '" + m.id + "': the version '" + m.version + "' is not major.minor.patch");
        if (m.kind != "workflows" && m.kind != "python" && m.kind != "native")
            return R::failure("plugin '" + m.id + "': unknown kind '" + m.kind + "'");
        if (m.entry.find("..") != std::string::npos || std::filesystem::path(m.entry).is_absolute())
            return R::failure("plugin '" + m.id + "': the entry must be a path inside the plugin directory");
        return m;
    } catch (const std::exception& e) {
        return R::failure(where + ": " + e.what());
    }
}

nlohmann::json toJson(const PluginManifest& m)
{
    nlohmann::json deps = nlohmann::json::array();
    for (const Dependency& d : m.dependencies)
        deps.push_back({{"id", d.id}, {"min", d.minVersion}});
    return {{"id", m.id},           {"name", m.name},         {"version", m.version},           {"author", m.author},
            {"description", m.description}, {"license", m.license}, {"resonant", {{"min", m.resonantMin}, {"max", m.resonantMax}}},
            {"apiVersion", m.apiVersion}, {"kind", m.kind}, {"entry", m.entry}, {"dependencies", deps}, {"requires", m.requires_},
            {"provides", m.provides}, {"permissions", m.permissions}, {"directory", m.directory.string()}};
}

PluginManager::PluginManager(std::vector<std::filesystem::path> searchPaths, std::filesystem::path configFile, std::string appVersion)
    : searchPaths_(std::move(searchPaths)), configFile_(std::move(configFile)), appVersion_(std::move(appVersion))
{
}

std::vector<std::filesystem::path> PluginManager::standardSearchPaths(const std::filesystem::path& bundledExamples)
{
    std::vector<std::filesystem::path> out;
    if (const char* env = std::getenv("RESONANT_PLUGIN_PATH")) {
        std::string list = env;
        size_t start = 0;
        while (start <= list.size()) {
            const size_t colon = list.find(':', start);
            const std::string part = list.substr(start, colon == std::string::npos ? std::string::npos : colon - start);
            if (!part.empty())
                out.emplace_back(part);
            if (colon == std::string::npos)
                break;
            start = colon + 1;
        }
    }
    if (const char* home = std::getenv("HOME")) {
        const char* data = std::getenv("XDG_DATA_HOME");
        out.push_back((data && *data ? std::filesystem::path(data) : std::filesystem::path(home) / ".local" / "share") / "Resonant" / "plugins");
    }
    if (!bundledExamples.empty())
        out.push_back(bundledExamples);
    return out;
}

std::filesystem::path PluginManager::standardConfigFile()
{
    if (const char* file = std::getenv("RESONANT_PLUGIN_CONFIG"); file && *file)
        return file;  // an explicit configuration (screenshots, tests)
    const char* config = std::getenv("XDG_CONFIG_HOME");
    const char* home = std::getenv("HOME");
    const std::filesystem::path base = config && *config ? std::filesystem::path(config) : std::filesystem::path(home ? home : ".") / ".config";
    return base / "Resonant" / "plugins.json";
}

std::set<std::string> PluginManager::enabledIds() const
{
    std::set<std::string> out;
    std::ifstream in(configFile_);
    if (!in)
        return out;
    try {
        const auto j = nlohmann::json::parse(in);
        for (const auto& id : j.value("enabled", nlohmann::json::array()))
            if (id.is_string())
                out.insert(id.get<std::string>());
    } catch (const std::exception& e) {
        logError("plugins", "the plugin configuration " + configFile_.filename().string() + " could not be read: " + e.what());
    }
    return out;
}

Status PluginManager::setEnabled(const std::string& id, bool enabled)
{
    std::set<std::string> ids = enabledIds();
    if (enabled)
        ids.insert(id);
    else
        ids.erase(id);
    std::error_code error;
    std::filesystem::create_directories(configFile_.parent_path(), error);
    std::ofstream out(configFile_);
    if (!out)
        return Status::failure("cannot write the plugin configuration " + configFile_.string());
    out << nlohmann::json{{"enabled", std::vector<std::string>(ids.begin(), ids.end())}}.dump(2) << "\n";
    if (PluginRecord* r = findMutable(id))
        r->enabled = enabled;
    return Status::success();
}

const PluginRecord* PluginManager::find(const std::string& id) const
{
    for (const PluginRecord& r : plugins_)
        if (r.manifest.id == id)
            return &r;
    return nullptr;
}

PluginRecord* PluginManager::findMutable(const std::string& id)
{
    for (PluginRecord& r : plugins_)
        if (r.manifest.id == id)
            return &r;
    return nullptr;
}

void PluginManager::discover(const std::vector<std::string>& capabilities)
{
    plugins_.clear();
    const std::set<std::string> enabled = enabledIds();
    std::set<std::string> seen;
    for (const auto& root : searchPaths_) {
        std::error_code error;
        if (!std::filesystem::is_directory(root, error))
            continue;
        std::vector<std::filesystem::path> dirs;
        for (const auto& entry : std::filesystem::directory_iterator(root, error))
            if (entry.is_directory() && std::filesystem::exists(entry.path() / "plugin.json"))
                dirs.push_back(entry.path());
        std::sort(dirs.begin(), dirs.end());
        for (const auto& dir : dirs) {
            PluginRecord r;
            auto m = readManifest(dir);
            if (!m) {
                r.manifest.id = dir.filename().string();
                r.manifest.name = r.manifest.id;
                r.manifest.directory = dir;
                r.state = PluginState::Invalid;
                r.message = m.message();
                plugins_.push_back(std::move(r));
                continue;
            }
            r.manifest = m.take();
            r.enabled = enabled.contains(r.manifest.id);
            if (!seen.insert(r.manifest.id).second) {
                r.state = PluginState::Invalid;
                r.message = "another plugin with the id '" + r.manifest.id + "' was found first (earlier search paths win)";
                plugins_.push_back(std::move(r));
                continue;
            }
            const auto app = Version::parse(appVersion_), lo = Version::parse(r.manifest.resonantMin), hi = Version::parse(r.manifest.resonantMax);
            if (r.manifest.apiVersion != kPluginApiVersion) {
                r.state = PluginState::Incompatible;
                r.message = "plugin API " + std::to_string(r.manifest.apiVersion) + " is not supported (this is " + std::to_string(kPluginApiVersion) + ")";
            } else if (!app || !lo || !hi || *app < *lo || *app > *hi) {
                r.state = PluginState::Incompatible;
                r.message = "supports Resonant " + r.manifest.resonantMin + " to " + r.manifest.resonantMax + ", this is " + appVersion_;
            } else if (!std::filesystem::exists(dir / r.manifest.entry)) {
                r.state = PluginState::Invalid;
                r.message = "the entry '" + r.manifest.entry + "' does not exist";
            } else {
                const auto has = [&](const std::string& c) { return std::find(capabilities.begin(), capabilities.end(), c) != capabilities.end(); };
                std::string missing;
                for (const std::string& need : r.manifest.requires_)
                    if (!has(need) && !automation::ActionRegistry::instance().knows(need))
                        missing = need;
                if (r.manifest.kind == "python" && !has("python"))
                    missing = "python";
                if (!missing.empty()) {
                    r.state = PluginState::Incompatible;
                    r.message = "requires '" + missing + "', which is not available";
                } else {
                    r.state = r.enabled ? PluginState::Ready : PluginState::Disabled;
                    r.message = r.enabled ? "enabled" : "not enabled (plugins load only when you enable them)";
                }
            }
            plugins_.push_back(std::move(r));
        }
    }
    // Dependencies (repeated until stable: a failure propagates to dependants).
    for (bool changed = true; changed;) {
        changed = false;
        for (PluginRecord& r : plugins_) {
            if (r.state != PluginState::Ready && r.state != PluginState::Disabled)
                continue;
            for (const Dependency& d : r.manifest.dependencies) {
                const PluginRecord* dep = find(d.id);
                std::string why;
                if (!dep)
                    why = "needs the plugin '" + d.id + "', which is not installed";
                else if (dep->state == PluginState::Incompatible || dep->state == PluginState::Invalid || dep->state == PluginState::MissingDependency ||
                         dep->state == PluginState::Failed)
                    why = "needs '" + d.id + "', which is " + std::string(toString(dep->state));
                else if (r.enabled && !dep->enabled)
                    why = "needs '" + d.id + "', which is not enabled";
                else if (Version::parse(dep->manifest.version) < Version::parse(d.minVersion))
                    why = "needs '" + d.id + "' " + d.minVersion + " or newer (installed: " + dep->manifest.version + ")";
                if (!why.empty()) {
                    r.state = PluginState::MissingDependency;
                    r.message = why;
                    changed = true;
                    break;
                }
            }
        }
    }
}

void PluginManager::loadEnabled()
{
    // Dependency order: a plugin after the plugins it needs.
    std::vector<PluginRecord*> order;
    std::set<std::string> placed;
    std::function<void(PluginRecord&)> visit = [&](PluginRecord& r) {
        if (placed.contains(r.manifest.id))
            return;
        placed.insert(r.manifest.id);
        for (const Dependency& d : r.manifest.dependencies)
            if (PluginRecord* dep = findMutable(d.id))
                visit(*dep);
        order.push_back(&r);
    };
    for (PluginRecord& r : plugins_)
        visit(r);
    for (PluginRecord* r : order) {
        if (r->state != PluginState::Ready)
            continue;
        bool dependencyFailed = false;
        for (const Dependency& d : r->manifest.dependencies)
            if (const PluginRecord* dep = find(d.id); dep && dep->state == PluginState::Failed)
                dependencyFailed = true;
        if (dependencyFailed) {
            r->state = PluginState::MissingDependency;
            r->message = "a plugin it needs failed to load";
            continue;
        }
        if (r->manifest.kind == "workflows")
            loadWorkflows(*r);
        else if (r->manifest.kind == "native")
            loadNative(*r);
        // Python plugins stay Ready until the worker reports.
    }
}

void PluginManager::loadWorkflows(PluginRecord& record)
{
    const auto dir = record.manifest.directory / record.manifest.entry;
    std::vector<automation::Workflow> loaded;
    std::error_code error;
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(dir, error))
        if (entry.path().extension() == ".json")
            files.push_back(entry.path());
    std::sort(files.begin(), files.end());
    for (const auto& file : files) {
        try {
            auto w = automation::workflowFromJson(nlohmann::json::parse(std::ifstream(file)));
            if (!w) {
                record.state = PluginState::Failed;
                record.message = file.filename().string() + ": " + w.message();
                return;
            }
            if (Status s = automation::checkRequirements(w.value(), {"python"}); !s) {
                record.state = PluginState::Failed;
                record.message = file.filename().string() + ": " + s.message();
                return;
            }
            loaded.push_back(w.take());
        } catch (const std::exception& e) {
            record.state = PluginState::Failed;
            record.message = file.filename().string() + ": " + e.what();
            return;
        }
    }
    if (loaded.empty()) {
        record.state = PluginState::Failed;
        record.message = "the plugin provides no workflow";
        return;
    }
    for (automation::Workflow& w : loaded) {
        record.extensions.push_back(w.id);
        automation::WorkflowLibrary::instance().add(std::move(w), "plugin " + record.manifest.id);
    }
    record.state = PluginState::Loaded;
    record.message = std::to_string(record.extensions.size()) + " workflow(s)";
}

namespace {
struct NativeContext {
    PluginRecord* record;
};
int hostAddWorkflow(void* context, const char* text, char* error, size_t size)
{
    auto* c = static_cast<NativeContext*>(context);
    const auto fail = [&](const std::string& message) {
        if (error && size)
            std::snprintf(error, size, "%s", message.c_str());
        return 1;
    };
    try {
        auto w = automation::workflowFromJson(nlohmann::json::parse(text ? text : ""));
        if (!w)
            return fail(w.message());
        c->record->extensions.push_back(w.value().id);
        automation::WorkflowLibrary::instance().add(w.take(), "plugin " + c->record->manifest.id);
        return 0;
    } catch (const std::exception& e) {
        return fail(e.what());
    }
}
void hostLog(void* context, int level, const char* message)
{
    auto* c = static_cast<NativeContext*>(context);
    const std::string text = "[" + c->record->manifest.id + "] " + (message ? message : "");
    if (level >= 2)
        logError("plugins", text);
    else if (level == 1)
        logWarning("plugins", text);
    else
        logInfo("plugins", text);
}
}  // namespace

void PluginManager::loadNative(PluginRecord& record)
{
    const auto path = record.manifest.directory / record.manifest.entry;
    void* handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        record.state = PluginState::Failed;
        const char* e = dlerror();
        record.message = std::string("the library could not be loaded: ") + (e ? e : "unknown error");
        return;
    }
    auto version = reinterpret_cast<ResonantPluginApiVersionFunction>(dlsym(handle, "resonant_plugin_api_version"));
    auto reg = reinterpret_cast<ResonantPluginRegisterFunction>(dlsym(handle, "resonant_plugin_register"));
    if (!version || !reg) {
        record.state = PluginState::Failed;
        record.message = "the library does not export resonant_plugin_api_version and resonant_plugin_register";
        dlclose(handle);
        return;
    }
    if (version() != RESONANT_PLUGIN_API_VERSION) {
        record.state = PluginState::Incompatible;
        record.message = "the library implements plugin API " + std::to_string(version());
        dlclose(handle);
        return;
    }
    // Kept for the lifetime of the process: native plugins are never unloaded.
    static std::vector<std::unique_ptr<NativeContext>> contexts;
    contexts.push_back(std::make_unique<NativeContext>(NativeContext{&record}));
    const ResonantPluginHost host{sizeof(ResonantPluginHost), RESONANT_PLUGIN_API_VERSION, contexts.back().get(), hostAddWorkflow, hostLog};
    const int status = reg(&host);
    record.state = status == 0 ? PluginState::Loaded : PluginState::Failed;
    record.message = status == 0 ? std::to_string(record.extensions.size()) + " extension(s)" : "registration returned " + std::to_string(status);
}

void PluginManager::markLoaded(const std::string& id, std::vector<std::string> extensions)
{
    if (PluginRecord* r = findMutable(id)) {
        r->state = PluginState::Loaded;
        r->extensions = std::move(extensions);
        r->message = std::to_string(r->extensions.size()) + " extension(s) in the Python worker";
    }
}

void PluginManager::markFailed(const std::string& id, const std::string& message)
{
    if (PluginRecord* r = findMutable(id)) {
        r->state = PluginState::Failed;
        r->message = message;
    }
}

std::vector<std::filesystem::path> PluginManager::readyPythonPlugins() const
{
    std::vector<std::filesystem::path> out;
    for (const PluginRecord& r : plugins_)
        if (r.manifest.kind == "python" && (r.state == PluginState::Ready || r.state == PluginState::Loaded))
            out.push_back(r.manifest.directory);
    return out;
}

}  // namespace resonant::plugins
