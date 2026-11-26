// Process-level setup shared by the executable and the application tests.
#pragma once

class QApplication;

namespace resonant_app {

// Must be called BEFORE the QApplication is created: sets the default
// OpenGL surface format (4.5 Core) and the settings identity.
void configureBeforeApplication();

// Called after the QApplication exists: logs the startup information.
void logStartup(const QApplication& application);

}  // namespace resonant_app
