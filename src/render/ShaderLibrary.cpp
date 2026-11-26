#include "ShaderLibrary.h"

#include "foundation/Log.h"

namespace resonant::render {

bool ShaderLibrary::add(const std::string& name, const char* vertexSource, const char* fragmentSource)
{
    auto program = std::make_unique<QOpenGLShaderProgram>();
    const bool ok = program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexSource)
                    && program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentSource) && program->link();
    if (!ok) {
        logError("render", "shader program '" + name + "' failed to build: " + program->log().toStdString());
        return false;
    }
    programs_[name] = std::move(program);
    return true;
}

QOpenGLShaderProgram* ShaderLibrary::get(const std::string& name) const
{
    const auto it = programs_.find(name);
    return it == programs_.end() ? nullptr : it->second.get();
}

}  // namespace resonant::render
