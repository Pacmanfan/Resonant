#include "GpuMesh.h"

namespace resonant::render {

GpuMesh::GpuMesh(QOpenGLFunctions_4_5_Core& gl, const TriangleMesh& mesh) : gl_(gl)
{
    std::vector<float> vertices;
    vertices.reserve(mesh.positions.size() * 6);
    for (size_t i = 0; i < mesh.positions.size(); ++i) {
        const auto& p = mesh.positions[i];
        const auto& n = mesh.normals[i];
        vertices.insert(vertices.end(), {p.x(), p.y(), p.z(), n.x(), n.y(), n.z()});
    }
    indexCount_ = static_cast<GLsizei>(mesh.indices.size());

    gl_.glCreateBuffers(1, &vertexBuffer_);
    gl_.glNamedBufferData(vertexBuffer_, static_cast<GLsizeiptr>(vertices.size() * sizeof(float)), vertices.data(),
                          GL_STATIC_DRAW);
    gl_.glCreateBuffers(1, &indexBuffer_);
    gl_.glNamedBufferData(indexBuffer_, static_cast<GLsizeiptr>(mesh.indices.size() * sizeof(std::uint32_t)),
                          mesh.indices.data(), GL_STATIC_DRAW);

    gl_.glCreateVertexArrays(1, &vertexArray_);
    gl_.glVertexArrayVertexBuffer(vertexArray_, 0, vertexBuffer_, 0, 6 * sizeof(float));
    gl_.glVertexArrayElementBuffer(vertexArray_, indexBuffer_);
    gl_.glEnableVertexArrayAttrib(vertexArray_, 0);
    gl_.glVertexArrayAttribFormat(vertexArray_, 0, 3, GL_FLOAT, GL_FALSE, 0);
    gl_.glVertexArrayAttribBinding(vertexArray_, 0, 0);
    gl_.glEnableVertexArrayAttrib(vertexArray_, 1);
    gl_.glVertexArrayAttribFormat(vertexArray_, 1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float));
    gl_.glVertexArrayAttribBinding(vertexArray_, 1, 0);
}

GpuMesh::~GpuMesh()
{
    gl_.glDeleteVertexArrays(1, &vertexArray_);
    gl_.glDeleteBuffers(1, &vertexBuffer_);
    gl_.glDeleteBuffers(1, &indexBuffer_);
}

void GpuMesh::draw() const
{
    if (indexCount_ == 0)
        return;
    gl_.glBindVertexArray(vertexArray_);
    gl_.glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, nullptr);
    gl_.glBindVertexArray(0);
}

LineBuffer::LineBuffer(QOpenGLFunctions_4_5_Core& gl) : gl_(gl)
{
    gl_.glCreateBuffers(1, &vertexBuffer_);
    gl_.glCreateVertexArrays(1, &vertexArray_);
    gl_.glVertexArrayVertexBuffer(vertexArray_, 0, vertexBuffer_, 0, sizeof(Vertex));
    gl_.glEnableVertexArrayAttrib(vertexArray_, 0);
    gl_.glVertexArrayAttribFormat(vertexArray_, 0, 3, GL_FLOAT, GL_FALSE, 0);
    gl_.glVertexArrayAttribBinding(vertexArray_, 0, 0);
    gl_.glEnableVertexArrayAttrib(vertexArray_, 1);
    gl_.glVertexArrayAttribFormat(vertexArray_, 1, 4, GL_FLOAT, GL_FALSE, 3 * sizeof(float));
    gl_.glVertexArrayAttribBinding(vertexArray_, 1, 0);
}

LineBuffer::~LineBuffer()
{
    gl_.glDeleteVertexArrays(1, &vertexArray_);
    gl_.glDeleteBuffers(1, &vertexBuffer_);
}

void LineBuffer::set(const std::vector<Vertex>& vertices)
{
    const auto bytes = static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex));
    if (bytes > capacity_) {
        gl_.glNamedBufferData(vertexBuffer_, bytes, vertices.data(), GL_DYNAMIC_DRAW);
        capacity_ = bytes;
    } else if (bytes > 0) {
        gl_.glNamedBufferSubData(vertexBuffer_, 0, bytes, vertices.data());
    }
    vertexCount_ = static_cast<GLsizei>(vertices.size());
}

void LineBuffer::draw() const
{
    if (vertexCount_ == 0)
        return;
    gl_.glBindVertexArray(vertexArray_);
    gl_.glDrawArrays(GL_LINES, 0, vertexCount_);
    gl_.glBindVertexArray(0);
}

}  // namespace resonant::render
