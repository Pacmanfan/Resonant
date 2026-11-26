#include "ShaderSources.h"

namespace resonant::render::shaders {

// Lit surfaces. Lighting is done in view space with a light that travels
// with the camera (a "headlight") plus ambient light, and is two-sided so
// open surfaces (planes, discs, reflector shells) are lit from both sides.
const char* const meshVertex = R"(#version 450 core
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

uniform mat4 uModelView;
uniform mat4 uProjection;
uniform mat3 uNormalMatrix;

out vec3 vViewPosition;
out vec3 vViewNormal;

void main()
{
    vec4 viewPosition = uModelView * vec4(inPosition, 1.0);
    vViewPosition = viewPosition.xyz;
    vViewNormal = uNormalMatrix * inNormal;
    gl_Position = uProjection * viewPosition;
}
)";

const char* const meshFragment = R"(#version 450 core
in vec3 vViewPosition;
in vec3 vViewNormal;

uniform vec4 uColor;
uniform vec3 uHighlight;      // selection tint
uniform float uHighlightMix;  // 0 = not selected

out vec4 outColor;

void main()
{
    vec3 normal = normalize(vViewNormal);
    if (!gl_FrontFacing)
        normal = -normal;
    vec3 toEye = normalize(-vViewPosition);
    vec3 lightDirection = normalize(vec3(0.35, 0.45, 1.0));  // from above the camera
    float diffuse = max(dot(normal, lightDirection), 0.0);
    vec3 halfVector = normalize(lightDirection + toEye);
    float specular = pow(max(dot(normal, halfVector), 0.0), 48.0);

    vec3 base = mix(uColor.rgb, uHighlight, uHighlightMix * 0.45);
    vec3 color = base * (0.28 + 0.72 * diffuse) + vec3(0.18) * specular;
    // A rim on selected objects keeps them readable from any direction.
    float rim = pow(1.0 - max(dot(normal, toEye), 0.0), 2.5);
    color += uHighlight * rim * uHighlightMix;
    outColor = vec4(color, uColor.a);
}
)";

// Unlit coloured lines (grid, axes, markers).
const char* const lineVertex = R"(#version 450 core
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;

uniform mat4 uViewProjection;

out vec4 vColor;

void main()
{
    vColor = inColor;
    gl_Position = uViewProjection * vec4(inPosition, 1.0);
}
)";

const char* const lineFragment = R"(#version 450 core
in vec4 vColor;
out vec4 outColor;

void main()
{
    outColor = vColor;
}
)";

}  // namespace resonant::render::shaders
