// The built-in layers: ground grid with coordinate axes, lit scene meshes,
// and free-form marker lines.
#pragma once

#include "GpuMesh.h"
#include "RenderLayer.h"

#include <map>
#include <memory>
#include <vector>

namespace resonant::render {

// A mesh instance to draw this frame. The renderer knows nothing about
// scene objects: the application turns its scene into DrawItems.
struct DrawItem {
    std::shared_ptr<const TriangleMesh> mesh;  // identity is the cache key
    Eigen::Matrix4d model = Eigen::Matrix4d::Identity();
    Eigen::Vector4f color = Eigen::Vector4f(0.7f, 0.72f, 0.75f, 1.0f);
    bool selected = false;
};

struct LineItem {
    Vec3 from = Vec3::Zero();
    Vec3 to = Vec3::Zero();
    Eigen::Vector4f color = Eigen::Vector4f(1.0f, 1.0f, 1.0f, 1.0f);
};

// Ground grid in the XY plane (Z = 0) and the coordinate axes: X red,
// Y green, Z blue. The grid spacing follows the camera distance in decades
// (1 mm, 1 cm, 10 cm, 1 m, ...), so it stays useful at every scale.
class GridLayer final : public RenderLayer {
public:
    bool showGrid = true;
    bool showAxes = true;

    // Grid line spacing in metres for a camera distance.
    static double spacingFor(double cameraDistance);

    bool initialize(QOpenGLFunctions_4_5_Core& gl, ShaderLibrary& shaders) override;
    void draw(const RenderContext& context) override;
    void shutdown() override;

private:
    std::unique_ptr<LineBuffer> lines_;
};

class MeshLayer final : public RenderLayer {
public:
    void setItems(std::vector<DrawItem> items) { items_ = std::move(items); }
    size_t cachedMeshCount() const { return cache_.size(); }

    bool initialize(QOpenGLFunctions_4_5_Core& gl, ShaderLibrary& shaders) override;
    void draw(const RenderContext& context) override;
    void shutdown() override;

private:
    QOpenGLFunctions_4_5_Core* gl_ = nullptr;
    std::vector<DrawItem> items_;
    // GPU meshes by source mesh. An entry is dropped when its mesh is no
    // longer drawn, so edited geometry does not accumulate.
    std::map<const TriangleMesh*, std::pair<std::shared_ptr<const TriangleMesh>, std::unique_ptr<GpuMesh>>> cache_;
};

class LineLayer final : public RenderLayer {
public:
    void setLines(std::vector<LineItem> lines) { lines_ = std::move(lines); }

    bool initialize(QOpenGLFunctions_4_5_Core& gl, ShaderLibrary& shaders) override;
    void draw(const RenderContext& context) override;
    void shutdown() override;

private:
    std::vector<LineItem> lines_;
    std::unique_ptr<LineBuffer> buffer_;
};

}  // namespace resonant::render
