#include "ScalarMeshLayer.h"

#include <array>

namespace resonant::render {
namespace {

constexpr std::array<std::array<float, 3>, 11> kSequential = {{
    {0.267f, 0.005f, 0.329f}, {0.283f, 0.141f, 0.458f}, {0.254f, 0.265f, 0.530f}, {0.207f, 0.372f, 0.553f},
    {0.164f, 0.471f, 0.558f}, {0.128f, 0.567f, 0.551f}, {0.135f, 0.659f, 0.518f}, {0.267f, 0.749f, 0.441f},
    {0.478f, 0.821f, 0.318f}, {0.741f, 0.873f, 0.150f}, {0.993f, 0.906f, 0.144f},
}};

const char* const kVertex = R"(#version 450 core
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in float inValue;
uniform mat4 uView;
uniform mat4 uProjection;
out vec3 vViewPosition;
out vec3 vViewNormal;
out float vValue;
void main()
{
    vec4 viewPosition = uView * vec4(inPosition, 1.0);
    vViewPosition = viewPosition.xyz;
    vViewNormal = mat3(uView) * inNormal;
    vValue = inValue;
    gl_Position = uProjection * viewPosition;
}
)";

const char* const kFragment = R"(#version 450 core
in vec3 vViewPosition;
in vec3 vViewNormal;
in float vValue;
uniform vec3 uSequential[11];
uniform int uDiverging;
uniform float uScale;
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
    vec3 negative = vec3(0.230, 0.299, 0.754), neutral = vec3(0.865), positive = vec3(0.706, 0.016, 0.150);
    return t < 0.5 ? mix(negative, neutral, t * 2.0) : mix(neutral, positive, t * 2.0 - 1.0);
}

void main()
{
    vec3 normal = normalize(vViewNormal);
    if (!gl_FrontFacing)
        normal = -normal;
    vec3 lightDirection = normalize(vec3(0.35, 0.45, 1.0));
    float diffuse = abs(dot(normal, lightDirection));
    vec3 base = uDiverging != 0 ? diverging(0.5 + 0.5 * vValue / uScale) : sequential(vValue / uScale);
    outColor = vec4(base * (0.45 + 0.55 * diffuse), 1.0);
}
)";

struct Vertex {
    float position[3];
    float normal[3];
    float value;
};

}  // namespace

void ScalarMeshLayer::setMesh(std::shared_ptr<const ScalarMesh> mesh)
{
    mesh_ = std::move(mesh);
    dirty_ = true;
}

bool ScalarMeshLayer::initialize(QOpenGLFunctions_4_5_Core& gl, ShaderLibrary& shaders)
{
    gl_ = &gl;
    if (!shaders.get("scalarMesh") && !shaders.add("scalarMesh", kVertex, kFragment))
        return false;
    gl_->glCreateVertexArrays(1, &vertexArray_);
    gl_->glCreateBuffers(1, &vertexBuffer_);
    gl_->glCreateBuffers(1, &indexBuffer_);
    gl_->glVertexArrayVertexBuffer(vertexArray_, 0, vertexBuffer_, 0, sizeof(Vertex));
    gl_->glVertexArrayElementBuffer(vertexArray_, indexBuffer_);
    for (GLuint attribute = 0; attribute < 3; ++attribute) {
        gl_->glEnableVertexArrayAttrib(vertexArray_, attribute);
        gl_->glVertexArrayAttribBinding(vertexArray_, attribute, 0);
    }
    gl_->glVertexArrayAttribFormat(vertexArray_, 0, 3, GL_FLOAT, GL_FALSE, 0);
    gl_->glVertexArrayAttribFormat(vertexArray_, 1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float));
    gl_->glVertexArrayAttribFormat(vertexArray_, 2, 1, GL_FLOAT, GL_FALSE, 6 * sizeof(float));
    dirty_ = true;
    return true;
}

void ScalarMeshLayer::upload()
{
    dirty_ = false;
    indexCount_ = 0;
    if (!mesh_ || mesh_->empty())
        return;
    std::vector<Vertex> vertices(mesh_->positions.size());
    for (size_t i = 0; i < vertices.size(); ++i) {
        const Eigen::Vector3f& p = mesh_->positions[i];
        const Eigen::Vector3f n = i < mesh_->normals.size() ? mesh_->normals[i] : Eigen::Vector3f::UnitZ();
        vertices[i] = {{p.x(), p.y(), p.z()}, {n.x(), n.y(), n.z()}, i < mesh_->values.size() ? mesh_->values[i] : 0.0f};
    }
    gl_->glNamedBufferData(vertexBuffer_, static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)), vertices.data(), GL_DYNAMIC_DRAW);
    gl_->glNamedBufferData(indexBuffer_, static_cast<GLsizeiptr>(mesh_->indices.size() * sizeof(std::uint32_t)), mesh_->indices.data(),
                           GL_DYNAMIC_DRAW);
    indexCount_ = static_cast<GLsizei>(mesh_->indices.size());
}

void ScalarMeshLayer::draw(const RenderContext& context)
{
    if (!gl_)
        return;
    if (dirty_)
        upload();
    if (indexCount_ == 0)
        return;
    QOpenGLShaderProgram* program = context.shaders->get("scalarMesh");
    if (!program)
        return;
    program->bind();
    program->setUniformValue("uView", toQt(context.view));
    program->setUniformValue("uProjection", toQt(context.projection));
    QVector3D colours[11];
    for (size_t i = 0; i < 11; ++i)
        colours[i] = QVector3D(kSequential[i][0], kSequential[i][1], kSequential[i][2]);
    program->setUniformValueArray("uSequential", colours, 11);
    program->setUniformValue("uDiverging", mesh_->diverging ? 1 : 0);
    program->setUniformValue("uScale", mesh_->scale > 0.0f ? mesh_->scale : 1.0f);
    gl_->glBindVertexArray(vertexArray_);
    gl_->glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, nullptr);
    gl_->glBindVertexArray(0);
    program->release();
}

void ScalarMeshLayer::shutdown()
{
    if (!gl_)
        return;
    gl_->glDeleteBuffers(1, &vertexBuffer_);
    gl_->glDeleteBuffers(1, &indexBuffer_);
    gl_->glDeleteVertexArrays(1, &vertexArray_);
    vertexBuffer_ = indexBuffer_ = vertexArray_ = 0;
    indexCount_ = 0;
    gl_ = nullptr;
    dirty_ = true;
}

}  // namespace resonant::render
