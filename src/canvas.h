#ifndef CANVAS_H
#define CANVAS_H

#include <QImage>
#include <QMatrix4x4>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLWidget>
#include <QElapsedTimer>
#include <QPropertyAnimation>
#include <QSurfaceFormat>
#include <QTimer>

#include <memory>

class GLMesh;
class Mesh;
class Backdrop;
class Axis;

enum ViewPoint { centerview, isoview, topview, bottomview, leftview, rightview, frontview, backview };
enum DrawMode { shaded, wireframe, surfaceangle, meshlight, DRAWMODECOUNT };

class Canvas : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit Canvas(const QSurfaceFormat& format, QWidget* parent = 0);
    ~Canvas();

    const static float P_PERSPECTIVE;
    const static float P_ORTHOGRAPHIC;

    void view_perspective(float p, bool animate);
    void draw_axes(bool d);
    void invert_zoom(bool d);
    void set_drawMode(enum DrawMode mode);
    void common_view_change(enum ViewPoint c);
    void setResetTransformOnLoad(bool d);

    QColor getAmbientColor();
    void setAmbientColor(QColor c);
    double getAmbientFactor();
    void setAmbientFactor(double f);
    void resetAmbientColor();

    QColor getDirectiveColor();
    void setDirectiveColor(QColor c);
    double getDirectiveFactor();
    void setDirectiveFactor(double f);
    void resetDirectiveColor();

    QList<QString> getNameDir();
    int getCurrentLightDirection();
    void setCurrentLightDirection(int ind);
    void resetCurrentLightDirection();

    /*  Overall light brightness: multiplies both the ambient and the
     *  directive factors in the lit draw mode (1.0 = neutral). */
    double getLightBrightness() const;
    void setLightBrightness(double b);
    void resetLightBrightness();

    /*  Model opacity, applied in every draw mode (1.0 = opaque). */
    double getModelOpacity() const;
    void setModelOpacity(double o);
    void resetModelOpacity();
    void setAnimationModelOpacity(double o); // transient; < 0 clears

    /*  Viewport background: an invalid color means the default gradient */
    QColor getBackgroundColor() const;
    void setBackgroundColor(const QColor& c);
    void resetBackgroundColor();

    /*  Transient overrides used by the rotation animation's color/light
     *  cycling (never persisted; cleared when the animation stops).
     *  Model and light colors affect the lit draw mode. */
    void setAnimationModelColor(const QColor& c);
    void setAnimationLightColor(const QColor& c);
    void setAnimationBackgroundColor(const QColor& c);
    void setAnimationLightDirection(const QVector3D& d);
    void clearAnimationLightDirection();
    void clearAnimationOverrides();
    QVector3D getLightDirectionVector(int index) const;
    enum DrawMode getDrawMode() const
    {
        return drawMode;
    }

    /*  Renders the scene rotated by angleDeg about the given view-space
     *  axis (relative to the current orientation) and returns the image.
     *  The on-screen orientation is left untouched. */
    QImage grabRotatedFrame(float angleDeg, const QVector3D& axis);

    /*  Momentum spin: when enabled, releasing a drag keeps the model
     *  rotating with the drag's velocity and trajectory. */
    void setMomentumEnabled(bool enabled);
    void stopSpin();

    /*  Continuous-rotation animation support.  Angles are absolute and
     *  applied on top of a fixed base orientation - by default the reset
     *  orientation, so identical inputs reproduce identical results.
     *  Rotation order: X, then Y, then Z (view-space axes). */
    QMatrix4x4 defaultOrientation() const;
    QMatrix4x4 currentOrientation() const;
    void setAnimationAngles(float ax, float ay, float az);
    void setAnimationAngles(float ax, float ay, float az, const QMatrix4x4& base);
    QImage grabAnimationFrame(float ax, float ay, float az);
    QImage grabAnimationFrame(float ax, float ay, float az, const QMatrix4x4& base);

    /*  Keyboard navigation: rotate/roll about view axes (degrees), pan
     *  by a fraction of the viewport, multiply the zoom factor. */
    void rotateView(float degX, float degY);
    void rollView(float deg);
    void panView(float fx, float fy);
    void zoomView(float factor);

public slots:
    void set_status(const QString& s);
    void clear_status();
    void load_mesh(Mesh* m, bool is_reload);

protected:
    void paintGL() override;
    void initializeGL() override;
    void resizeGL(int width, int height) override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

    void set_perspective(float p);
    void view_anim(float v);

private:
    void draw_mesh();

    QMatrix4x4 orient_matrix() const;
    QMatrix4x4 transform_matrix() const;
    QMatrix4x4 aspect_matrix() const;
    QMatrix4x4 view_matrix() const;
    void resetTransform();
    QPointF changeMouseCoordinates(QPoint p);
    void calcArcballTransform(QPointF p1, QPointF p2);

    std::unique_ptr<QOpenGLShader> mesh_vertshader;
    QOpenGLShaderProgram mesh_shader;
    QOpenGLShaderProgram mesh_wireframe_shader;
    QOpenGLShaderProgram mesh_surfaceangle_shader;
    QOpenGLShaderProgram mesh_meshlight_shader;

    QColor ambientColor;
    QColor directiveColor;
    QColor backgroundColor; // invalid = default gradient backdrop

    // Animation overrides (invalid / unset = use the configured values)
    QColor animModelColor;
    QColor animLightColor;
    QColor animBackgroundColor;
    QVector3D animLightDirection;
    bool animLightDirectionSet = false;
    float ambientFactor;
    float directiveFactor;
    float lightBrightness = 1.0f;
    float modelOpacity = 1.0f;
    float animModelOpacity = -1.0f; // < 0 = no override
    QList<QString> nameDir;
    QList<QVector3D> listDir;
    int currentLightDirection;

    const static QColor defaultAmbientColor;
    const static QColor defaultDirectiveColor;
    const static float defaultAmbientFactor;
    const static float defaultDirectiveFactor;
    const static int defaultCurrentLightDirection;
    const static QString AMBIENT_COLOR;
    const static QString AMBIENT_FACTOR;
    const static QString DIRECTIVE_COLOR;
    const static QString DIRECTIVE_FACTOR;
    const static QString CURRENT_LIGHT_DIRECTION;
    const static QString BACKGROUND_COLOR;
    const static QString LIGHT_BRIGHTNESS;
    const static QString MODEL_OPACITY;

    std::unique_ptr<GLMesh> mesh;
    std::unique_ptr<Backdrop> backdrop;
    std::unique_ptr<Axis> axis;
    std::unique_ptr<Mesh> pending_mesh; // mesh delivered before GL initialization
    bool pending_is_reload = false;
    bool hideHud = false; // suppress axes/text overlays during exports

    // Momentum spin state
    bool momentumEnabled = false;
    QTimer spin_timer;
    QElapsedTimer spin_clock;  // dt between spin ticks
    QElapsedTimer drag_clock;  // dt between drag steps
    QVector3D last_drag_axis;  // view-space
    float last_drag_speed = 0; // deg/s

    QVector3D center, default_center;
    float scale, default_scale;
    float zoom;
    QMatrix4x4 currentTransform;

    float perspective = 0.25f;
    enum DrawMode drawMode = shaded;
    bool drawAxes = false;
    bool invertZoom = false;
    bool resetTransformOnLoad = true;
    Q_PROPERTY(float perspective MEMBER perspective WRITE set_perspective);
    QPropertyAnimation anim;

    QPoint mouse_pos;
    QString status;
    QString meshInfo;
};

#endif // CANVAS_H
