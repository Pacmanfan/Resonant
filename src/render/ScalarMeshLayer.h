// A surface coloured by a value per vertex: deformed structures coloured by
// displacement or stress. The vertices are given in world coordinates and
// replaced whenever the displayed data or the animation phase changes.
#pragma once

#include "RenderLayer.h"

#include <Eigen/Core>

#include <memory>
#include <vector>

namespace resonant::render {

struct ScalarMesh {
    std::vector<Eigen::Vector3f> positions, normals;
    std::vector<float> values;
    std::vector<std::uint32_t> indices;
    bool diverging = false;  // signed quantity: blue - white - red; otherwise viridis from 0
    float scale = 1.0f;      // the value at the top of the colour scale (and -scale at the bottom when diverging)
    bool empty() const { return indices.empty(); }
};

class ScalarMeshLayer final : public RenderLayer {
public:
    void setMesh(std::shared_ptr<const ScalarMesh> mesh);
    const ScalarMesh* mesh() const { return mesh_.get(); }

    bool initialize(QOpenGLFunctions_4_5_Core& gl, ShaderLibrary& shaders) override;
    void draw(const RenderContext& context) override;
    void shutdown() override;

private:
    void upload();

    QOpenGLFunctions_4_5_Core* gl_ = nullptr;
    std::shared_ptr<const ScalarMesh> mesh_;
    bool dirty_ = false;
    GLuint vertexArray_ = 0, vertexBuffer_ = 0, indexBuffer_ = 0;
    GLsizei indexCount_ = 0;
};

}  // namespace resonant::render
