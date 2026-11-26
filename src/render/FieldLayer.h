// Displays a complex scalar field sampled on a regular grid: a plane as one
// coloured rectangle, a volume as up to three orthogonal slices through it.
//
// The layer draws stored data. The complex samples are uploaded once as a
// texture; the quantity shown (magnitude, phase, instantaneous value,
// relative intensity), the colour scale and the animation phase are
// uniforms, so changing them or the camera never recomputes or re-uploads
// the field.
#pragma once

#include "RenderLayer.h"

#include <complex>
#include <memory>
#include <vector>

namespace resonant::render {

// A complex field on a grid, independent of the physics that produced it.
// Sample (iu, iv, iw) is stored at index iu + nu * (iv + nv * iw) and lies
// at origin + iu/(nu-1) axisU + iv/(nv-1) axisV + iw/(nw-1) axisW.
struct ComplexFieldGrid {
    Vec3 origin = Vec3::Zero();
    Vec3 axisU = Vec3::UnitX();
    Vec3 axisV = Vec3::UnitY();
    Vec3 axisW = Vec3::Zero();
    int nu = 0;
    int nv = 0;
    int nw = 1;
    // Keeps the samples alive; `samples` points into it.
    std::shared_ptr<const void> owner;
    const std::complex<float>* samples = nullptr;

    bool isVolume() const { return nw > 1; }
    bool empty() const { return !samples || nu < 2 || nv < 2; }
};

enum class FieldQuantity {
    Magnitude,          // |P| / scale, sequential colours
    Phase,              // arg P, cyclic colours
    Instantaneous,      // Re{P exp(j phase)} / scale, diverging colours
    RelativeIntensity,  // |P|^2 / scale^2, sequential colours
};

struct FieldDisplay {
    FieldQuantity quantity = FieldQuantity::Magnitude;
    // The value mapped to the top of the colour scale, in the field's unit.
    double scale = 1.0;
    // Phase of exp(j omega t) for the instantaneous view, radians.
    double animationPhase = 0.0;
    // Slice positions through a volume as fractions in [0, 1] of each axis.
    double sliceU = 0.5;
    double sliceV = 0.5;
    double sliceW = 0.5;
    bool showSliceU = true;
    bool showSliceV = true;
    bool showSliceW = true;
    bool visible = true;
};

class FieldLayer final : public RenderLayer {
public:
    // Sets (or clears, with an empty grid) the field to draw. The texture is
    // rebuilt on the next draw only when the grid changed.
    void setField(const ComplexFieldGrid& grid);
    void setDisplay(const FieldDisplay& display) { display_ = display; }
    const FieldDisplay& display() const { return display_; }
    bool hasField() const { return !grid_.empty(); }
    // Number of texture uploads so far (for verifying that display changes
    // do not re-upload).
    int uploadCount() const { return uploads_; }

    bool initialize(QOpenGLFunctions_4_5_Core& gl, ShaderLibrary& shaders) override;
    void draw(const RenderContext& context) override;
    void shutdown() override;

private:
    void upload();

    QOpenGLFunctions_4_5_Core* gl_ = nullptr;
    ComplexFieldGrid grid_;
    FieldDisplay display_;
    bool dirty_ = false;
    GLuint texture_ = 0;
    GLuint vertexArray_ = 0;
    GLuint vertexBuffer_ = 0;
    int uploads_ = 0;
};

// The colour of a normalized value t in [0, 1] on the sequential scale and
// on the diverging scale (t = 0.5 is zero), as the shader computes them.
// Exposed so legends and tests use the same mapping.
Eigen::Vector3f sequentialColor(float t);
Eigen::Vector3f divergingColor(float t);

}  // namespace resonant::render
