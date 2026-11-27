/* The native plugin ABI of Resonant (plugin API version 1, ADR-000215).
 *
 * A native plugin is a shared library exporting two C functions:
 *
 *     int  resonant_plugin_api_version(void);                      // must return RESONANT_PLUGIN_API_VERSION
 *     int  resonant_plugin_register(const ResonantPluginHost* host);  // 0 = success
 *
 * The host hands the plugin a table of C functions - the only things a
 * plugin can do: add a workflow (JSON text, "resonant.workflow" format) and
 * write to the log. Only C types cross the boundary, so a plugin does not
 * depend on the compiler, the C++ standard library or Resonant's classes;
 * new host functions are appended at the end of the table (the table's size
 * tells the plugin which exist).
 *
 * A native plugin runs inside the application process with the user's
 * permissions: it is NOT sandboxed, and a crash in it is a crash of the
 * application. It is loaded only when the user enabled it, and never
 * unloaded while the application runs.
 */
#ifndef RESONANT_PLUGIN_API_H
#define RESONANT_PLUGIN_API_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RESONANT_PLUGIN_API_VERSION 1

typedef struct ResonantPluginHost {
    size_t size;                   /* sizeof(ResonantPluginHost) of the host */
    int apiVersion;                /* RESONANT_PLUGIN_API_VERSION of the host */
    void* context;                 /* pass back unchanged */
    /* Adds a workflow template; returns 0 on success, otherwise writes the reason into `error` (size bytes). */
    int (*addWorkflow)(void* context, const char* json, char* error, size_t size);
    /* Writes a line to the application log (level 0 info, 1 warning, 2 error). */
    void (*log)(void* context, int level, const char* message);
} ResonantPluginHost;

typedef int (*ResonantPluginApiVersionFunction)(void);
typedef int (*ResonantPluginRegisterFunction)(const ResonantPluginHost* host);

#ifdef __cplusplus
}
#endif

#endif
