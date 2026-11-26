#include "ViewportWidget.h"

#include "DocumentController.h"
#include "foundation/Units.h"

#include <QApplication>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QWheelEvent>

using namespace resonant;

ViewportWidget::ViewportWidget(QWidget* parent) : QOpenGLWidget(parent)
{
    setFormat(render::requiredSurfaceFormat());
    setFocusPolicy(Qt::StrongFocus);
}

ViewportWidget::~ViewportWidget()
{
    // The base class destroys the context after this destructor has run;
    // by then the slot below must no longer be connected.
    disconnect(contextConnection_);
    releaseGraphics();
}

void ViewportWidget::setController(DocumentController* controller)
{
    controller_ = controller;
    connect(controller_, &DocumentController::sceneChanged, this, &ViewportWidget::refresh);
    connect(controller_, &DocumentController::selectionChanged, this, &ViewportWidget::refresh);
    connect(controller_, &DocumentController::projectReplaced, this, [this] {
        bridge_.clear();
        refresh();
    });
    refresh();
}

void ViewportWidget::setField(const render::ComplexFieldGrid& grid, const render::FieldDisplay& display)
{
    renderer_.field().setField(grid);
    renderer_.field().setDisplay(display);
    update();
}

void ViewportWidget::refresh()
{
    frameStale_ = true;
    update();
}

void ViewportWidget::rebuildFrame()
{
    if (!controller_)
        return;
    frame_ = bridge_.build(controller_->scene(), controller_->selection());
    renderer_.meshes().setItems(frame_.items);
    renderer_.lines().setLines(frame_.lines);
    frameStale_ = false;
}

void ViewportWidget::initializeGL()
{
    // The context can be destroyed and recreated (for example when the
    // widget is re-docked); release GPU resources with the old one.
    disconnect(contextConnection_);
    contextConnection_ =
        connect(context(), &QOpenGLContext::aboutToBeDestroyed, this, &ViewportWidget::releaseGraphics);
    if (renderer_.initialize(context())) {
        graphicsError_.clear();
        emit graphicsInitialized(QString::fromStdString(renderer_.contextDescription()));
    } else {
        graphicsError_ = QString::fromStdString(renderer_.lastError());
        emit graphicsFailed(graphicsError_);
    }
}

void ViewportWidget::releaseGraphics()
{
    if (!renderer_.isInitialized())
        return;
    makeCurrent();
    renderer_.shutdown();
    doneCurrent();
}

void ViewportWidget::resizeGL(int width, int height)
{
    const qreal ratio = devicePixelRatioF();
    renderer_.resize(static_cast<int>(width * ratio), static_cast<int>(height * ratio));
}

void ViewportWidget::paintGL()
{
    if (!renderer_.isInitialized())
        return;
    if (frameStale_)
        rebuildFrame();
    renderer_.render(camera_);
}

Id ViewportWidget::objectAt(const QPoint& position)
{
    if (frameStale_)
        rebuildFrame();
    const double w = std::max(1, width());
    const double h = std::max(1, height());
    const double ndcX = 2.0 * (position.x() + 0.5) / w - 1.0;
    const double ndcY = 1.0 - 2.0 * (position.y() + 0.5) / h;
    return bridge_.pick(camera_.pickRay(ndcX, ndcY, w / h));
}

void ViewportWidget::mousePressEvent(QMouseEvent* event)
{
    pressPosition_ = lastPosition_ = event->pos();
    dragging_ = false;
}

void ViewportWidget::mouseMoveEvent(QMouseEvent* event)
{
    const QPoint delta = event->pos() - lastPosition_;
    if (!dragging_ && (event->pos() - pressPosition_).manhattanLength() < QApplication::startDragDistance())
        return;
    dragging_ = true;
    lastPosition_ = event->pos();

    if (event->buttons() & Qt::LeftButton) {
        camera_.orbit(-delta.x() * 0.008, delta.y() * 0.008);
    } else if (event->buttons() & (Qt::RightButton | Qt::MiddleButton)) {
        const double h = std::max(1, height());
        camera_.pan(delta.x() / h, delta.y() / h);
    }
    update();
}

void ViewportWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (!dragging_ && event->button() == Qt::LeftButton && controller_) {
        const Id id = objectAt(event->pos());
        const bool toggle = event->modifiers() & Qt::ControlModifier;
        if (toggle) {
            if (!id.isNull())
                controller_->select(id, true);
        } else {
            controller_->select(id);
        }
    }
    dragging_ = false;
}

void ViewportWidget::wheelEvent(QWheelEvent* event)
{
    const double steps = event->angleDelta().y() / 120.0;
    camera_.zoom(std::pow(0.88, steps));
    update();
}

void ViewportWidget::setGridVisible(bool visible)
{
    renderer_.grid().showGrid = visible;
    update();
}

void ViewportWidget::setAxesVisible(bool visible)
{
    renderer_.grid().showAxes = visible;
    update();
}

void ViewportWidget::frameAll()
{
    if (frameStale_)
        rebuildFrame();
    if (frame_.bounds.isValid())
        camera_.frame(frame_.bounds);
    else
        camera_.reset();
    update();
}

void ViewportWidget::frameSelection()
{
    if (frameStale_)
        rebuildFrame();
    if (frame_.selectionBounds.isValid())
        camera_.frame(frame_.selectionBounds);
    else
        frameAll();
    update();
}

void ViewportWidget::resetCamera()
{
    camera_.reset();
    update();
}

void ViewportWidget::viewTop()
{
    camera_.yaw = -units::pi / 2.0;
    camera_.pitch = units::degreesToRadians(89.0);
    update();
}

void ViewportWidget::viewFront()
{
    camera_.yaw = -units::pi / 2.0;  // eye on the -Y side, looking along +Y
    camera_.pitch = 0.0;
    update();
}

void ViewportWidget::viewRight()
{
    camera_.yaw = 0.0;  // eye on the +X side, looking along -X
    camera_.pitch = 0.0;
    update();
}
