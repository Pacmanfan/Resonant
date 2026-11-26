// The renderer draws a frame as a sequence of layers. A layer is a reusable
// rendering component with its own GPU resources: the grid, the scene
// meshes, marker lines, and later field slices, volumes and isosurfaces.
// New kinds of visualization are added as new layers, not by growing the
// viewport widget.
#pragma once

#include "Camera.h"
#include "ShaderLibrary.h"

#include <QOpenGLFunctions_4_5_Core>

namespace resonant::render {

struct RenderContext {
    QOpenGLFunctions_4_5_Core* gl = nullptr;
    ShaderLibrary* shaders = nullptr;
    const Camera* camera = nullptr;
    Eigen::Matrix4d view = Eigen::Matrix4d::Identity();
    Eigen::Matrix4d projection = Eigen::Matrix4d::Identity();
    int width = 1;
    int height = 1;
};

class RenderLayer {
public:
    virtual ~RenderLayer() = default;

    // Called once with the context current, before the first draw.
    virtual bool initialize(QOpenGLFunctions_4_5_Core& gl, ShaderLibrary& shaders) = 0;
    // Called every frame with the context current.
    virtual void draw(const RenderContext& context) = 0;
    // Called with the context current; releases every GPU resource.
    virtual void shutdown() = 0;
};

QMatrix4x4 toQt(const Eigen::Matrix4d& matrix);

}  // namespace resonant::render
