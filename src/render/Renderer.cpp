#include "Renderer.h"

#include "ShaderSources.h"
#include "foundation/Log.h"

#include <QOpenGLVersionFunctionsFactory>
#include <QSurfaceFormat>

namespace resonant::render {

QSurfaceFormat requiredSurfaceFormat()
{
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setVersion(4, 5);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    format.setSamples(4);
    return format;
}

Renderer::Renderer()
{
    auto grid = std::make_unique<GridLayer>();
    auto meshes = std::make_unique<MeshLayer>();
    auto lines = std::make_unique<LineLayer>();
    auto field = std::make_unique<FieldLayer>();
    field_ = field.get();
    grid_ = grid.get();
    meshes_ = meshes.get();
    lines_ = lines.get();
    layers_.push_back(std::move(grid));
    layers_.push_back(std::move(field));
    layers_.push_back(std::move(meshes));
    layers_.push_back(std::move(lines));
}

Renderer::~Renderer() = default;

bool Renderer::initialize(QOpenGLContext* context)
{
    if (!context) {
        lastError_ = "no OpenGL context";
        return false;
    }
    const QSurfaceFormat format = context->format();
    const bool versionOk = format.majorVersion() > 4 || (format.majorVersion() == 4 && format.minorVersion() >= 5);
    auto* gl = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_4_5_Core>(context);
    if (!versionOk || format.profile() != QSurfaceFormat::CoreProfile || !gl) {
        lastError_ = "OpenGL 4.5 Core is required, but the system provided OpenGL "
                     + std::to_string(format.majorVersion()) + "." + std::to_string(format.minorVersion())
                     + (format.profile() == QSurfaceFormat::CoreProfile ? " Core" : " (not a core profile)")
                     + ". Update the graphics driver or run on hardware that supports OpenGL 4.5.";
        logError("render", lastError_);
        return false;
    }
    gl->initializeOpenGLFunctions();

    const auto text = [gl](GLenum name) {
        const GLubyte* value = gl->glGetString(name);
        return value ? std::string(reinterpret_cast<const char*>(value)) : std::string("unknown");
    };
    contextDescription_ = "OpenGL " + text(GL_VERSION) + " / " + text(GL_RENDERER) + " / " + text(GL_VENDOR);
    logInfo("render", "graphics initialized: " + contextDescription_);

    if (!shaders_.add("mesh", shaders::meshVertex, shaders::meshFragment)
        || !shaders_.add("line", shaders::lineVertex, shaders::lineFragment)) {
        lastError_ = "a built-in shader failed to compile; see the log for the compiler output";
        shaders_.clear();
        return false;
    }
    for (const auto& layer : layers_) {
        if (!layer->initialize(*gl, shaders_)) {
            lastError_ = "a render layer failed to initialize";
            logError("render", lastError_);
            for (const auto& started : layers_)
                started->shutdown();
            shaders_.clear();
            return false;
        }
    }
    gl_ = gl;
    lastError_.clear();
    return true;
}

void Renderer::shutdown()
{
    if (!gl_)
        return;
    for (const auto& layer : layers_)
        layer->shutdown();
    shaders_.clear();
    gl_ = nullptr;
    logInfo("render", "graphics shut down");
}

void Renderer::resize(int width, int height)
{
    width_ = std::max(1, width);
    height_ = std::max(1, height);
}

void Renderer::render(const Camera& camera)
{
    if (!gl_)
        return;
    gl_->glViewport(0, 0, width_, height_);
    gl_->glClearColor(background.x(), background.y(), background.z(), 1.0f);
    gl_->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    gl_->glEnable(GL_DEPTH_TEST);
    gl_->glDepthFunc(GL_LEQUAL);
    gl_->glDisable(GL_CULL_FACE);  // open surfaces are visible from both sides
    gl_->glEnable(GL_MULTISAMPLE);

    RenderContext context;
    context.gl = gl_;
    context.shaders = &shaders_;
    context.camera = &camera;
    context.view = camera.viewMatrix();
    context.projection = camera.projectionMatrix(static_cast<double>(width_) / height_);
    context.width = width_;
    context.height = height_;
    for (const auto& layer : layers_)
        layer->draw(context);
}

RenderLayer* Renderer::addLayer(std::unique_ptr<RenderLayer> layer)
{
    RenderLayer* raw = layer.get();
    if (gl_ && !raw->initialize(*gl_, shaders_)) {
        logError("render", "an added render layer failed to initialize");
        return nullptr;
    }
    layers_.push_back(std::move(layer));
    return raw;
}

}  // namespace resonant::render
