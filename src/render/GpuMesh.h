// A TriangleMesh uploaded to the GPU: one vertex array with interleaved
// position and normal, and an index buffer. Created and destroyed with the
// owning context current.
#pragma once

#include "geometry/TriangleMesh.h"

#include <QOpenGLFunctions_4_5_Core>

namespace resonant::render {

class GpuMesh {
public:
    GpuMesh(QOpenGLFunctions_4_5_Core& gl, const TriangleMesh& mesh);
    ~GpuMesh();
    GpuMesh(const GpuMesh&) = delete;
    GpuMesh& operator=(const GpuMesh&) = delete;

    void draw() const;

private:
    QOpenGLFunctions_4_5_Core& gl_;
    GLuint vertexArray_ = 0;
    GLuint vertexBuffer_ = 0;
    GLuint indexBuffer_ = 0;
    GLsizei indexCount_ = 0;
};

// A buffer of coloured line segments that can be refilled every frame.
class LineBuffer {
public:
    struct Vertex {
        float position[3];
        float color[4];
    };

    explicit LineBuffer(QOpenGLFunctions_4_5_Core& gl);
    ~LineBuffer();
    LineBuffer(const LineBuffer&) = delete;
    LineBuffer& operator=(const LineBuffer&) = delete;

    void set(const std::vector<Vertex>& vertices);
    void draw() const;

private:
    QOpenGLFunctions_4_5_Core& gl_;
    GLuint vertexArray_ = 0;
    GLuint vertexBuffer_ = 0;
    GLsizei vertexCount_ = 0;
    GLsizeiptr capacity_ = 0;
};

}  // namespace resonant::render
