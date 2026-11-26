// Renders through the real OpenGL 4.5 Core pipeline into an offscreen
// framebuffer and inspects the pixels.
#include "render/Renderer.h"

#include "geometry/Geometry.h"

#include <QImage>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>

#include <gtest/gtest.h>

#include <set>

using namespace resonant;

namespace {

class RenderTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        context_.setFormat(render::requiredSurfaceFormat());
        ASSERT_TRUE(context_.create()) << "could not create an OpenGL context";
        surface_.setFormat(context_.format());
        surface_.create();
        ASSERT_TRUE(context_.makeCurrent(&surface_));
        ASSERT_TRUE(renderer_.initialize(&context_)) << renderer_.lastError();
        QOpenGLFramebufferObjectFormat format;
        format.setAttachment(QOpenGLFramebufferObject::Depth);
        framebuffer_ = std::make_unique<QOpenGLFramebufferObject>(QSize(kSize, kSize), format);
        renderer_.resize(kSize, kSize);
    }

    void TearDown() override
    {
        framebuffer_.reset();
        renderer_.shutdown();
        context_.doneCurrent();
    }

    QImage draw()
    {
        framebuffer_->bind();
        renderer_.render(camera_);
        framebuffer_->release();
        return framebuffer_->toImage().convertToFormat(QImage::Format_RGB32);
    }

    QColor backgroundColor() const
    {
        return QColor::fromRgbF(renderer_.background.x(), renderer_.background.y(), renderer_.background.z());
    }

    static bool near(const QColor& a, const QColor& b, int tolerance = 6)
    {
        return std::abs(a.red() - b.red()) <= tolerance && std::abs(a.green() - b.green()) <= tolerance
               && std::abs(a.blue() - b.blue()) <= tolerance;
    }

    int countDifferentFrom(const QImage& image, const QColor& color) const
    {
        int count = 0;
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x)
                if (!near(image.pixelColor(x, y), color))
                    ++count;
        return count;
    }

    static constexpr int kSize = 256;
    QOpenGLContext context_;
    QOffscreenSurface surface_;
    render::Renderer renderer_;
    render::Camera camera_;
    std::unique_ptr<QOpenGLFramebufferObject> framebuffer_;
};

render::DrawItem boxItem(bool selected = false)
{
    render::DrawItem item;
    item.mesh = std::make_shared<const TriangleMesh>(Geometry::create("box").take().tessellate());
    item.color = Eigen::Vector4f(0.2f, 0.6f, 0.9f, 1.0f);
    item.selected = selected;
    return item;
}

}  // namespace

TEST_F(RenderTest, ContextIsOpenGl45CoreAndDescribed)
{
    const QSurfaceFormat format = context_.format();
    EXPECT_TRUE(format.majorVersion() > 4 || (format.majorVersion() == 4 && format.minorVersion() >= 5));
    EXPECT_EQ(format.profile(), QSurfaceFormat::CoreProfile);
    EXPECT_NE(renderer_.contextDescription().find("OpenGL"), std::string::npos);
    EXPECT_TRUE(renderer_.isInitialized());
}

TEST_F(RenderTest, EmptySceneWithoutGridIsTheBackgroundColor)
{
    renderer_.grid().showGrid = false;
    renderer_.grid().showAxes = false;
    const QImage image = draw();
    EXPECT_EQ(countDifferentFrom(image, backgroundColor()), 0);
}

TEST_F(RenderTest, GridAndAxesDrawLines)
{
    const QImage image = draw();
    const int drawn = countDifferentFrom(image, backgroundColor());
    EXPECT_GT(drawn, 500);
    EXPECT_LT(drawn, kSize * kSize / 2);  // lines, not a filled plane

    // The +X axis is red and the +Y axis green somewhere in the image.
    bool red = false, green = false;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor c = image.pixelColor(x, y);
            red = red || (c.red() > 200 && c.green() < 120 && c.blue() < 120);
            green = green || (c.green() > 180 && c.red() < 130 && c.blue() < 130);
        }
    }
    EXPECT_TRUE(red);
    EXPECT_TRUE(green);
}

TEST_F(RenderTest, MeshCoversTheCentreAndIsLit)
{
    renderer_.grid().showGrid = false;
    renderer_.grid().showAxes = false;
    camera_.distance = 4.0;
    renderer_.meshes().setItems({boxItem()});
    const QImage image = draw();

    // The box is at the target, so the centre pixel shows it, in a shade of
    // its own colour (blue strongest), neither black nor saturated.
    const QColor centre = image.pixelColor(kSize / 2, kSize / 2);
    EXPECT_FALSE(near(centre, backgroundColor()));
    EXPECT_GT(centre.blue(), centre.red());
    EXPECT_GT(centre.blue(), 60);
    // The corners are far outside the box.
    EXPECT_TRUE(near(image.pixelColor(2, 2), backgroundColor()));
    EXPECT_TRUE(near(image.pixelColor(kSize - 3, kSize - 3), backgroundColor()));
    // Faces with different orientations get different shading.
    std::set<QRgb> shades;
    for (int y = 0; y < image.height(); y += 4)
        for (int x = 0; x < image.width(); x += 4)
            if (!near(image.pixelColor(x, y), backgroundColor()))
                shades.insert(image.pixel(x, y));
    EXPECT_GE(shades.size(), 2u);
    EXPECT_EQ(renderer_.meshes().cachedMeshCount(), 1u);
}

TEST_F(RenderTest, SelectionChangesTheAppearance)
{
    renderer_.grid().showGrid = false;
    renderer_.grid().showAxes = false;
    camera_.distance = 4.0;
    renderer_.meshes().setItems({boxItem(false)});
    const QImage plain = draw();
    renderer_.meshes().setItems({boxItem(true)});
    const QImage selected = draw();

    const QColor a = plain.pixelColor(kSize / 2, kSize / 2);
    const QColor b = selected.pixelColor(kSize / 2, kSize / 2);
    EXPECT_FALSE(near(a, b));
    EXPECT_GT(b.red(), a.red());  // the highlight is orange
}

TEST_F(RenderTest, NearerObjectHidesTheFartherOne)
{
    renderer_.grid().showGrid = false;
    renderer_.grid().showAxes = false;
    camera_.yaw = 0.0;  // eye on +X looking along -X
    camera_.pitch = 0.0;
    camera_.distance = 6.0;

    render::DrawItem nearItem = boxItem();
    nearItem.color = Eigen::Vector4f(0.9f, 0.1f, 0.1f, 1.0f);
    nearItem.model(0, 3) = 1.5;
    render::DrawItem farItem = boxItem();
    farItem.color = Eigen::Vector4f(0.1f, 0.9f, 0.1f, 1.0f);
    farItem.model(0, 3) = -1.5;

    // Whatever the draw order, the nearer (red) box wins at the centre.
    for (const auto& order : {std::vector{nearItem, farItem}, std::vector{farItem, nearItem}}) {
        renderer_.meshes().setItems(order);
        const QColor centre = draw().pixelColor(kSize / 2, kSize / 2);
        EXPECT_GT(centre.red(), centre.green());
    }
}

TEST_F(RenderTest, ResizeChangesTheViewport)
{
    renderer_.grid().showGrid = false;
    renderer_.grid().showAxes = false;
    camera_.distance = 4.0;
    renderer_.meshes().setItems({boxItem()});
    renderer_.resize(kSize / 2, kSize / 2);  // draw into the lower-left quarter only
    const QImage image = draw();
    // Framebuffer row 0 is the bottom; QImage row 0 is the top.
    EXPECT_FALSE(near(image.pixelColor(kSize / 4, 3 * kSize / 4), backgroundColor()));
    renderer_.resize(kSize, kSize);
}

TEST_F(RenderTest, MeshCacheDropsMeshesThatAreNoLongerDrawn)
{
    renderer_.meshes().setItems({boxItem(), boxItem()});
    draw();
    EXPECT_EQ(renderer_.meshes().cachedMeshCount(), 2u);
    renderer_.meshes().setItems({});
    draw();
    EXPECT_EQ(renderer_.meshes().cachedMeshCount(), 0u);
}

TEST_F(RenderTest, ShutdownAndReinitialize)
{
    renderer_.shutdown();
    EXPECT_FALSE(renderer_.isInitialized());
    ASSERT_TRUE(renderer_.initialize(&context_));
    renderer_.resize(kSize, kSize);
    renderer_.meshes().setItems({boxItem()});
    EXPECT_FALSE(near(draw().pixelColor(kSize / 2, kSize / 2), backgroundColor()));
}
