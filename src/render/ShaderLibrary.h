// Compiles and owns the shader programs. One place reports compile and link
// errors, so no component fails silently with a black viewport.
#pragma once

#include <QOpenGLShaderProgram>

#include <map>
#include <memory>
#include <string>

namespace resonant::render {

class ShaderLibrary {
public:
    // Compiles and links a program and stores it under `name`. Returns
    // false and logs the compiler output when it fails.
    bool add(const std::string& name, const char* vertexSource, const char* fragmentSource);

    // The program, or nullptr when it does not exist or failed to build.
    QOpenGLShaderProgram* get(const std::string& name) const;

    void clear() { programs_.clear(); }

private:
    std::map<std::string, std::unique_ptr<QOpenGLShaderProgram>> programs_;
};

}  // namespace resonant::render
