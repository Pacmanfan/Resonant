// The 3D viewport: an OpenGL 4.5 Core widget that shows the scene through
// the rendering subsystem and turns mouse input into camera navigation and
// selection.
//
//   left drag          orbit            wheel            zoom
//   right / middle drag  pan            left click       select (Ctrl: toggle)
#pragma once

#include "SceneRenderBridge.h"
#include "render/Renderer.h"

#include <QOpenGLWidget>
#include <QPoint>

class DocumentController;

class ViewportWidget : public QOpenGLWidget {
    Q_OBJECT
public:
    explicit ViewportWidget(QWidget* parent = nullptr);
    ~ViewportWidget() override;

    void setController(DocumentController* controller);

    resonant::render::Camera& camera() { return camera_; }
    resonant::render::Renderer& renderer() { return renderer_; }
    bool graphicsReady() const { return renderer_.isInitialized(); }
    QString graphicsError() const { return graphicsError_; }

    bool gridVisible() const { return renderer_.grid().showGrid; }
    bool axesVisible() const { return renderer_.grid().showAxes; }

    // The id of the object under a widget position, or a null id.
    resonant::Id objectAt(const QPoint& position);

public slots:
    void setGridVisible(bool visible);
    void setAxesVisible(bool visible);
    void frameAll();
    void frameSelection();
    void resetCamera();
    void viewTop();
    void viewFront();
    void viewRight();
    // Marks the scene content stale and schedules a repaint.
    void refresh();

signals:
    void graphicsInitialized(const QString& description);
    void graphicsFailed(const QString& message);

protected:
    void initializeGL() override;
    void resizeGL(int width, int height) override;
    void paintGL() override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void releaseGraphics();
    void rebuildFrame();

    DocumentController* controller_ = nullptr;
    resonant::render::Renderer renderer_;
    resonant::render::Camera camera_;
    SceneRenderBridge bridge_;
    SceneRenderBridge::Frame frame_;
    bool frameStale_ = true;
    QString graphicsError_;
    QMetaObject::Connection contextConnection_;

    QPoint pressPosition_;
    QPoint lastPosition_;
    bool dragging_ = false;
};
