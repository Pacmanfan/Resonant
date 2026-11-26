#include "Layers.h"

#include <QMatrix3x3>
#include <QMatrix4x4>

#include <cmath>
#include <set>

namespace resonant::render {

QMatrix4x4 toQt(const Eigen::Matrix4d& matrix)
{
    QMatrix4x4 result;
    for (int row = 0; row < 4; ++row)
        for (int column = 0; column < 4; ++column)
            result(row, column) = static_cast<float>(matrix(row, column));
    return result;
}

namespace {

LineBuffer::Vertex vertex(const Vec3& p, const Eigen::Vector4f& c)
{
    return {{static_cast<float>(p.x()), static_cast<float>(p.y()), static_cast<float>(p.z())},
            {c.x(), c.y(), c.z(), c.w()}};
}

void drawLines(const RenderContext& context, const LineBuffer& buffer)
{
    QOpenGLShaderProgram* program = context.shaders->get("line");
    if (!program)
        return;
    program->bind();
    program->setUniformValue("uViewProjection", toQt(context.projection * context.view));
    buffer.draw();
    program->release();
}

}  // namespace

double GridLayer::spacingFor(double cameraDistance)
{
    // About five to fifty lines across the view.
    return std::pow(10.0, std::floor(std::log10(std::max(cameraDistance, 1e-9) / 5.0)));
}

bool GridLayer::initialize(QOpenGLFunctions_4_5_Core& gl, ShaderLibrary&)
{
    lines_ = std::make_unique<LineBuffer>(gl);
    return true;
}

void GridLayer::draw(const RenderContext& context)
{
    if (!lines_ || (!showGrid && !showAxes))
        return;
    const double spacing = spacingFor(context.camera->distance);
    const int halfCount = 50;
    const double extent = spacing * halfCount;
    // Snap the grid to the target so it never runs out under the camera.
    const double cx = std::round(context.camera->target.x() / (spacing * 10.0)) * spacing * 10.0;
    const double cy = std::round(context.camera->target.y() / (spacing * 10.0)) * spacing * 10.0;

    std::vector<LineBuffer::Vertex> vertices;
    if (showGrid) {
        const Eigen::Vector4f minor(0.34f, 0.36f, 0.40f, 1.0f);
        const Eigen::Vector4f major(0.48f, 0.50f, 0.55f, 1.0f);
        for (int i = -halfCount; i <= halfCount; ++i) {
            const double offset = i * spacing;
            const auto& color = (i % 10 == 0) ? major : minor;
            // Lines along the axes are drawn by the axes below.
            if (!(showAxes && std::abs(cy + offset) < 1e-12)) {
                vertices.push_back(vertex(Vec3(cx - extent, cy + offset, 0.0), color));
                vertices.push_back(vertex(Vec3(cx + extent, cy + offset, 0.0), color));
            }
            if (!(showAxes && std::abs(cx + offset) < 1e-12)) {
                vertices.push_back(vertex(Vec3(cx + offset, cy - extent, 0.0), color));
                vertices.push_back(vertex(Vec3(cx + offset, cy + extent, 0.0), color));
            }
        }
    }
    if (showAxes) {
        const double length = extent + std::max(std::abs(cx), std::abs(cy));
        vertices.push_back(vertex(Vec3(-length, 0.0, 0.0), Eigen::Vector4f(0.55f, 0.20f, 0.20f, 1.0f)));
        vertices.push_back(vertex(Vec3(0.0, 0.0, 0.0), Eigen::Vector4f(0.55f, 0.20f, 0.20f, 1.0f)));
        vertices.push_back(vertex(Vec3(0.0, 0.0, 0.0), Eigen::Vector4f(0.95f, 0.30f, 0.30f, 1.0f)));
        vertices.push_back(vertex(Vec3(length, 0.0, 0.0), Eigen::Vector4f(0.95f, 0.30f, 0.30f, 1.0f)));
        vertices.push_back(vertex(Vec3(0.0, -length, 0.0), Eigen::Vector4f(0.20f, 0.50f, 0.20f, 1.0f)));
        vertices.push_back(vertex(Vec3(0.0, 0.0, 0.0), Eigen::Vector4f(0.20f, 0.50f, 0.20f, 1.0f)));
        vertices.push_back(vertex(Vec3(0.0, 0.0, 0.0), Eigen::Vector4f(0.35f, 0.85f, 0.35f, 1.0f)));
        vertices.push_back(vertex(Vec3(0.0, length, 0.0), Eigen::Vector4f(0.35f, 0.85f, 0.35f, 1.0f)));
        vertices.push_back(vertex(Vec3(0.0, 0.0, 0.0), Eigen::Vector4f(0.35f, 0.55f, 1.00f, 1.0f)));
        vertices.push_back(vertex(Vec3(0.0, 0.0, spacing * 10.0), Eigen::Vector4f(0.35f, 0.55f, 1.00f, 1.0f)));
    }
    lines_->set(vertices);
    drawLines(context, *lines_);
}

void GridLayer::shutdown()
{
    lines_.reset();
}

bool MeshLayer::initialize(QOpenGLFunctions_4_5_Core& gl, ShaderLibrary&)
{
    gl_ = &gl;
    return true;
}

void MeshLayer::draw(const RenderContext& context)
{
    QOpenGLShaderProgram* program = context.shaders->get("mesh");
    if (!program || !gl_)
        return;

    std::set<const TriangleMesh*> used;
    program->bind();
    program->setUniformValue("uProjection", toQt(context.projection));
    program->setUniformValue("uHighlight", QVector3D(1.0f, 0.62f, 0.12f));
    for (const DrawItem& item : items_) {
        if (!item.mesh || item.mesh->empty())
            continue;
        used.insert(item.mesh.get());
        auto& entry = cache_[item.mesh.get()];
        if (!entry.second) {
            entry.first = item.mesh;  // keeps the key's address from being reused
            entry.second = std::make_unique<GpuMesh>(*gl_, *item.mesh);
        }
        const Eigen::Matrix4d modelView = context.view * item.model;
        // Normals transform with the inverse transpose, which matters for
        // non-uniform scale.
        const Eigen::Matrix3d normalMatrix = modelView.block<3, 3>(0, 0).inverse().transpose();
        QMatrix3x3 qNormal;
        for (int row = 0; row < 3; ++row)
            for (int column = 0; column < 3; ++column)
                qNormal(row, column) = static_cast<float>(normalMatrix(row, column));
        program->setUniformValue("uModelView", toQt(modelView));
        program->setUniformValue("uNormalMatrix", qNormal);
        program->setUniformValue("uColor", QVector4D(item.color.x(), item.color.y(), item.color.z(), item.color.w()));
        program->setUniformValue("uHighlightMix", item.selected ? 1.0f : 0.0f);
        entry.second->draw();
    }
    program->release();

    std::erase_if(cache_, [&used](const auto& entry) { return !used.contains(entry.first); });
}

void MeshLayer::shutdown()
{
    cache_.clear();
    gl_ = nullptr;
}

bool LineLayer::initialize(QOpenGLFunctions_4_5_Core& gl, ShaderLibrary&)
{
    buffer_ = std::make_unique<LineBuffer>(gl);
    return true;
}

void LineLayer::draw(const RenderContext& context)
{
    if (!buffer_ || lines_.empty())
        return;
    std::vector<LineBuffer::Vertex> vertices;
    vertices.reserve(lines_.size() * 2);
    for (const LineItem& line : lines_) {
        vertices.push_back(vertex(line.from, line.color));
        vertices.push_back(vertex(line.to, line.color));
    }
    buffer_->set(vertices);
    drawLines(context, *buffer_);
}

void LineLayer::shutdown()
{
    buffer_.reset();
}

}  // namespace resonant::render
