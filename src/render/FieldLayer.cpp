#include "FieldLayer.h"

#include "foundation/Log.h"

#include <array>
#include <cmath>

namespace resonant::render {

namespace {

// Eleven samples of the viridis colour map (perceptually uniform, readable
// with colour-vision deficiencies), linearly interpolated.
constexpr std::array<std::array<float, 3>, 11> kSequential = {{
    {0.267f, 0.005f, 0.329f}, {0.283f, 0.141f, 0.458f}, {0.254f, 0.265f, 0.530f}, {0.207f, 0.372f, 0.553f},
    {0.164f, 0.471f, 0.558f}, {0.128f, 0.567f, 0.551f}, {0.135f, 0.659f, 0.518f}, {0.267f, 0.749f, 0.441f},
    {0.478f, 0.821f, 0.318f}, {0.741f, 0.873f, 0.150f}, {0.993f, 0.906f, 0.144f},
}};
constexpr std::array<float, 3> kNegative = {0.230f, 0.299f, 0.754f};
constexpr std::array<float, 3> kNeutral = {0.865f, 0.865f, 0.865f};
constexpr std::array<float, 3> kPositive = {0.706f, 0.016f, 0.150f};

const char* const kVertex = R"(#version 450 core
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inCoordinate;
uniform mat4 uViewProjection;
out vec3 vCoordinate;
void main()
{
    vCoordinate = inCoordinate;
    gl_Position = uViewProjection * vec4(inPosition, 1.0);
}
)";

const char* const kFragment = R"(#version 450 core
in vec3 vCoordinate;
uniform sampler3D uField;      // RG = real, imaginary
uniform int uQuantity;         // 0 magnitude, 1 phase, 2 instantaneous, 3 relative intensity, 4 decibels
uniform float uDecibelRange;
uniform float uScale;
uniform float uPhase;
uniform vec3 uSequential[11];
uniform vec3 uNegative;
uniform vec3 uNeutral;
uniform vec3 uPositive;
out vec4 outColor;

vec3 sequential(float t)
{
    float x = clamp(t, 0.0, 1.0) * 10.0;
    int i = min(int(floor(x)), 9);
    return mix(uSequential[i], uSequential[i + 1], x - float(i));
}

vec3 diverging(float t)
{
    t = clamp(t, 0.0, 1.0);
    return t < 0.5 ? mix(uNegative, uNeutral, t * 2.0) : mix(uNeutral, uPositive, t * 2.0 - 1.0);
}

vec3 cyclic(float t)  // hue wheel
{
    vec3 k = vec3(0.0, 2.0 / 3.0, 1.0 / 3.0);
    return clamp(abs(fract(vec3(t) + k) * 6.0 - 3.0) - 1.0, 0.0, 1.0) * 0.85 + 0.1;
}

void main()
{
    vec2 p = texture(uField, vCoordinate).rg;
    float magnitude = length(p);
    vec3 color;
    if (uQuantity == 0) {
        color = sequential(magnitude / uScale);
    } else if (uQuantity == 1) {
        color = cyclic(atan(p.y, p.x) / 6.28318530718 + 0.5);
    } else if (uQuantity == 2) {
        float value = p.x * cos(uPhase) - p.y * sin(uPhase);
        color = diverging(0.5 + 0.5 * value / uScale);
    } else if (uQuantity == 3) {
        color = sequential((magnitude * magnitude) / (uScale * uScale));
    } else {
        float level = 20.0 * log(max(magnitude / uScale, 1e-12)) / log(10.0);
        color = sequential(1.0 + level / uDecibelRange);
    }
    outColor = vec4(color, 1.0);
}
)";

struct Vertex {
    float position[3];
    float coordinate[3];
};

template <typename T>
Eigen::Vector3f toVector(const T& c)
{
    return Eigen::Vector3f(c[0], c[1], c[2]);
}

}  // namespace

Eigen::Vector3f sequentialColor(float t)
{
    const float x = std::clamp(t, 0.0f, 1.0f) * 10.0f;
    const int i = std::min(static_cast<int>(std::floor(x)), 9);
    const float f = x - static_cast<float>(i);
    return (1.0f - f) * toVector(kSequential[static_cast<size_t>(i)]) + f * toVector(kSequential[static_cast<size_t>(i + 1)]);
}

Eigen::Vector3f divergingColor(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    if (t < 0.5f)
        return (1.0f - 2.0f * t) * toVector(kNegative) + 2.0f * t * toVector(kNeutral);
    return (2.0f - 2.0f * t) * toVector(kNeutral) + (2.0f * t - 1.0f) * toVector(kPositive);
}

void FieldLayer::setField(const ComplexFieldGrid& grid)
{
    const bool same = grid.samples == grid_.samples && grid.nu == grid_.nu && grid.nv == grid_.nv
                      && grid.nw == grid_.nw;
    grid_ = grid;
    if (!same)
        dirty_ = true;
}

bool FieldLayer::initialize(QOpenGLFunctions_4_5_Core& gl, ShaderLibrary& shaders)
{
    gl_ = &gl;
    if (!shaders.get("field") && !shaders.add("field", kVertex, kFragment))
        return false;
    gl_->glCreateBuffers(1, &vertexBuffer_);
    gl_->glNamedBufferData(vertexBuffer_, 18 * sizeof(Vertex), nullptr, GL_DYNAMIC_DRAW);
    gl_->glCreateVertexArrays(1, &vertexArray_);
    gl_->glVertexArrayVertexBuffer(vertexArray_, 0, vertexBuffer_, 0, sizeof(Vertex));
    gl_->glEnableVertexArrayAttrib(vertexArray_, 0);
    gl_->glVertexArrayAttribFormat(vertexArray_, 0, 3, GL_FLOAT, GL_FALSE, 0);
    gl_->glVertexArrayAttribBinding(vertexArray_, 0, 0);
    gl_->glEnableVertexArrayAttrib(vertexArray_, 1);
    gl_->glVertexArrayAttribFormat(vertexArray_, 1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float));
    gl_->glVertexArrayAttribBinding(vertexArray_, 1, 0);
    dirty_ = !grid_.empty();
    return true;
}

void FieldLayer::upload()
{
    if (texture_) {
        gl_->glDeleteTextures(1, &texture_);
        texture_ = 0;
    }
    dirty_ = false;
    if (grid_.empty())
        return;
    gl_->glCreateTextures(GL_TEXTURE_3D, 1, &texture_);
    gl_->glTextureStorage3D(texture_, 1, GL_RG32F, grid_.nu, grid_.nv, grid_.nw);
    gl_->glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    // std::complex<float> is layout-compatible with float[2].
    gl_->glTextureSubImage3D(texture_, 0, 0, 0, 0, grid_.nu, grid_.nv, grid_.nw, GL_RG, GL_FLOAT, grid_.samples);
    gl_->glTextureParameteri(texture_, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl_->glTextureParameteri(texture_, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl_->glTextureParameteri(texture_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl_->glTextureParameteri(texture_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl_->glTextureParameteri(texture_, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    ++uploads_;
}

void FieldLayer::draw(const RenderContext& context)
{
    if (!gl_)
        return;
    if (dirty_)
        upload();
    if (!texture_ || !display_.visible)
        return;
    QOpenGLShaderProgram* program = context.shaders->get("field");
    if (!program)
        return;

    // Texel centres are at (i + 0.5) / n, sample i at fraction i / (n - 1).
    const auto coordinate = [](double fraction, int count) {
        return count > 1 ? static_cast<float>((fraction * (count - 1) + 0.5) / count) : 0.5f;
    };
    std::vector<Vertex> vertices;
    const auto addQuad = [&](const Vec3& corner, const Vec3& edgeA, const Vec3& edgeB, const std::array<double, 3>& c0,
                             const std::array<double, 3>& cA, const std::array<double, 3>& cB) {
        const auto vertex = [&](double a, double b) {
            const Vec3 p = corner + a * edgeA + b * edgeB;
            Vertex v{};
            for (int i = 0; i < 3; ++i) {
                v.position[i] = static_cast<float>(p[i]);
                const double fraction = c0[static_cast<size_t>(i)] + a * cA[static_cast<size_t>(i)] + b * cB[static_cast<size_t>(i)];
                const int count = i == 0 ? grid_.nu : i == 1 ? grid_.nv : grid_.nw;
                v.coordinate[i] = coordinate(fraction, count);
            }
            return v;
        };
        for (const auto& [a, b] : {std::pair{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}})
            vertices.push_back(vertex(a, b));
    };

    if (!grid_.isVolume()) {
        addQuad(grid_.origin, grid_.axisU, grid_.axisV, {0, 0, 0}, {1, 0, 0}, {0, 1, 0});
    } else {
        const double u = std::clamp(display_.sliceU, 0.0, 1.0);
        const double v = std::clamp(display_.sliceV, 0.0, 1.0);
        const double w = std::clamp(display_.sliceW, 0.0, 1.0);
        if (display_.showSliceW)
            addQuad(grid_.origin + w * grid_.axisW, grid_.axisU, grid_.axisV, {0, 0, w}, {1, 0, 0}, {0, 1, 0});
        if (display_.showSliceV)
            addQuad(grid_.origin + v * grid_.axisV, grid_.axisU, grid_.axisW, {0, v, 0}, {1, 0, 0}, {0, 0, 1});
        if (display_.showSliceU)
            addQuad(grid_.origin + u * grid_.axisU, grid_.axisV, grid_.axisW, {u, 0, 0}, {0, 1, 0}, {0, 0, 1});
    }
    if (vertices.empty())
        return;
    gl_->glNamedBufferSubData(vertexBuffer_, 0, static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)), vertices.data());

    program->bind();
    program->setUniformValue("uViewProjection", toQt(context.projection * context.view));
    program->setUniformValue("uField", 0);
    program->setUniformValue("uQuantity", static_cast<int>(display_.quantity));
    program->setUniformValue("uScale", static_cast<float>(display_.scale > 0.0 ? display_.scale : 1.0));
    program->setUniformValue("uPhase", static_cast<float>(display_.animationPhase));
    program->setUniformValue("uDecibelRange", static_cast<float>(display_.decibelRange > 0.0 ? display_.decibelRange : 40.0));
    QVector3D colors[11];
    for (size_t i = 0; i < 11; ++i)
        colors[i] = QVector3D(kSequential[i][0], kSequential[i][1], kSequential[i][2]);
    program->setUniformValueArray("uSequential", colors, 11);
    program->setUniformValue("uNegative", QVector3D(kNegative[0], kNegative[1], kNegative[2]));
    program->setUniformValue("uNeutral", QVector3D(kNeutral[0], kNeutral[1], kNeutral[2]));
    program->setUniformValue("uPositive", QVector3D(kPositive[0], kPositive[1], kPositive[2]));
    gl_->glBindTextureUnit(0, texture_);
    gl_->glBindVertexArray(vertexArray_);
    gl_->glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
    gl_->glBindVertexArray(0);
    program->release();
}

void FieldLayer::shutdown()
{
    if (!gl_)
        return;
    if (texture_)
        gl_->glDeleteTextures(1, &texture_);
    gl_->glDeleteVertexArrays(1, &vertexArray_);
    gl_->glDeleteBuffers(1, &vertexBuffer_);
    texture_ = vertexArray_ = vertexBuffer_ = 0;
    gl_ = nullptr;
    dirty_ = !grid_.empty();
}

}  // namespace resonant::render
