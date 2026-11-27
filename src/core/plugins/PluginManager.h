#pragma once

// Plugins (RES-PLUG-001..008, ADR-000215).
//
// A plugin is a directory with a manifest, plugin.json:
//
//   id, name, version (major.minor.patch), author, description, license
//   resonant {min, max}       supported Resonant versions (inclusive)
//   apiVersion                plugin API version (1)
//   kind                      "workflows" | "python" | "native"
//   entry                     workflows: a directory of workflow JSON files;
//                             python: the module file defining register(api);
//                             native: the shared library
//   dependencies [{id, min}]  other plugins (with a lowest version)
//   requires                  capabilities: "python", action ids
//   provides                  extension points: workflow-template,
//                             geometry-generator, analysis, measurement-importer
//   permissions               what it does, declared for the user (not enforced)
//
// The manager discovers plugins in its search paths, validates manifests,
// versions, API versions, duplicates and dependencies, and loads only the
// plugins the user enabled (configuration file). Workflow plugins add their
// workflows to the library; native plugins are loaded through the C ABI of
// PluginApi.h; Python plugins are loaded by the Python worker process (the
// application hands it their directories). Nothing here is a sandbox: plugins
// are code that runs with the user's permissions, and the documentation and
// user interface say so.
#include "foundation/Result.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace resonant::plugins {

inline constexpr int kPluginApiVersion = 1;

struct Version {
    int major = 0, minor = 0, patch = 0;
    static std::optional<Version> parse(const std::string& text);
    std::string str() const;
    friend auto operator<=>(const Version&, const Version&) = default;
};

struct Dependency {
    std::string id;
    std::string minVersion;
};

struct PluginManifest {
    std::string id, name, version, author, description, license;
    std::string resonantMin, resonantMax;
    int apiVersion = 0;
    std::string kind;
    std::string entry;
    std::vector<Dependency> dependencies;
    std::vector<std::string> requires_;  // capabilities ("requires" in the manifest)
    std::vector<std::string> provides;
    std::vector<std::string> permissions;
    std::filesystem::path directory;
};

Result<PluginManifest> readManifest(const std::filesystem::path& directory);
nlohmann::json toJson(const PluginManifest& manifest);

enum class PluginState {
    Disabled,           // valid, not enabled by the user
    Ready,              // enabled and valid; a Python plugin waits for the worker
    Loaded,             // its extensions are registered
    Incompatible,       // Resonant version or plugin API version not supported
    MissingDependency,  // a dependency is absent, disabled, too old or failed
    Invalid,            // the manifest or the plugin itself is broken
    Failed              // loading or registration failed
};
std::string_view toString(PluginState state);

struct PluginRecord {
    PluginManifest manifest;
    PluginState state = PluginState::Disabled;
    bool enabled = false;
    std::string message;                    // why it is in its state
    std::vector<std::string> extensions;    // registered extensions (workflow ids, ...)
};

class PluginManager {
public:
    // `configFile`: JSON {"enabled": [ids]}; `appVersion`: the running Resonant version.
    PluginManager(std::vector<std::filesystem::path> searchPaths, std::filesystem::path configFile, std::string appVersion);

    // The standard search paths: $RESONANT_PLUGIN_PATH (colon separated), the
    // user's data directory (~/.local/share/Resonant/plugins) and the bundled
    // examples; and the standard configuration file ($RESONANT_PLUGIN_CONFIG, else
    // ~/.config/Resonant/plugins.json).
    static std::vector<std::filesystem::path> standardSearchPaths(const std::filesystem::path& bundledExamples);
    static std::filesystem::path standardConfigFile();

    // Finds and validates every plugin (states Disabled / Ready / Incompatible /
    // MissingDependency / Invalid); loads nothing.
    void discover(const std::vector<std::string>& capabilities = {});
    // Loads the Ready workflow and native plugins (in dependency order).
    void loadEnabled();
    // Records the outcome of a Python plugin loaded by the worker.
    void markLoaded(const std::string& id, std::vector<std::string> extensions);
    void markFailed(const std::string& id, const std::string& message);

    Status setEnabled(const std::string& id, bool enabled);  // persists the configuration; takes effect at the next discover()
    const std::vector<PluginRecord>& plugins() const { return plugins_; }
    const PluginRecord* find(const std::string& id) const;
    std::vector<std::filesystem::path> readyPythonPlugins() const;  // directories, for the worker
    const std::vector<std::filesystem::path>& searchPaths() const { return searchPaths_; }

private:
    void loadWorkflows(PluginRecord& record);
    void loadNative(PluginRecord& record);
    std::set<std::string> enabledIds() const;
    PluginRecord* findMutable(const std::string& id);

    std::vector<std::filesystem::path> searchPaths_;
    std::filesystem::path configFile_;
    std::string appVersion_;
    std::vector<PluginRecord> plugins_;
};

}  // namespace resonant::plugins
