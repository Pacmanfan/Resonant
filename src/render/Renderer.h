// The rendering subsystem: owns the shader programs and the layers, and
// draws a frame. It has an explicit lifecycle (initialize with a current
// OpenGL 4.5 Core context, render, shutdown with the same context current)
// and knows nothing about scene objects or widgets.
#pragma once

#include "FieldLayer.h"
#include "Layers.h"

#include <QOpenGLContext>

#include <string>
#include <vector>

namespace resonant::render {

class Renderer {
public:
    Renderer();
    ~Renderer();

    // Requires a current context providing OpenGL 4.5 Core. Returns false
    // and sets lastError() otherwise or when a shader fails to build.
    bool initialize(QOpenGLContext* context);
    bool isInitialized() const { return gl_ != nullptr; }
    void shutdown();

    const std::string& lastError() const { return lastError_; }
    // "OpenGL 4.6 (Core Profile) Mesa ... / renderer / vendor".
    const std::string& contextDescription() const { return contextDescription_; }

    void resize(int width, int height);
    void render(const Camera& camera);

    Eigen::Vector3f background = Eigen::Vector3f(0.13f, 0.14f, 0.16f);

    GridLayer& grid() { return *grid_; }
    const GridLayer& grid() const { return *grid_; }
    MeshLayer& meshes() { return *meshes_; }
    LineLayer& lines() { return *lines_; }
    FieldLayer& field() { return *field_; }

    // Adds a layer drawn after the built-in ones. If the renderer is
    // already initialized the layer is initialized at once (the context
    // must be current).
    RenderLayer* addLayer(std::unique_ptr<RenderLayer> layer);

private:
    QOpenGLFunctions_4_5_Core* gl_ = nullptr;
    ShaderLibrary shaders_;
    GridLayer* grid_ = nullptr;
    MeshLayer* meshes_ = nullptr;
    LineLayer* lines_ = nullptr;
    FieldLayer* field_ = nullptr;
    std::vector<std::unique_ptr<RenderLayer>> layers_;
    int width_ = 1;
    int height_ = 1;
    std::string lastError_;
    std::string contextDescription_;
};

// The surface format the application must request before creating any
// window: OpenGL 4.5 Core with a depth buffer and multisampling.
QSurfaceFormat requiredSurfaceFormat();

}  // namespace resonant::render
