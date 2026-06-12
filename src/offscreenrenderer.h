#ifndef OFFSCREENRENDERER_H
#define OFFSCREENRENDERER_H

#include <QColor>
#include <QImage>
#include <QMatrix4x4>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QSize>
#include <QVector3D>

#include <memory>
#include <vector>

class Mesh;
class GLMesh;

/*  Appearance parameters for offscreen rendering. */
struct RenderStyle {
    QColor modelColor = Qt::white;
    QColor background = Qt::transparent;
    double opacity = 1.0;    // model alpha, 0..1
    double brightness = 1.0; // light multiplier
    QVector3D lightDirection = QVector3D(0, -0.5, 1);
};

/*  Renders a mesh without a window, for command-line exports.
 *  The background may be any color including transparent, and the model
 *  is lit with a single directional light tinted by the model color. */
class OffscreenRenderer : protected QOpenGLFunctions
{
public:
    OffscreenRenderer(const Mesh* mesh, const RenderStyle& style);
    ~OffscreenRenderer();

    bool isValid() const
    {
        return valid;
    }
    QString errorString() const
    {
        return error;
    }

    /*  Picks an output size whose aspect ratio matches the model's
     *  projected bounds over all given rotations, fitting in maxW x maxH.
     *  Must be called before renderFrame; it also centers the model. */
    QSize layoutForAngles(const std::vector<float>& angles, const QVector3D& axis, int maxW, int maxH);

    /*  Renders one frame at the given rotation (about the view-space axis,
     *  relative to a straight-on front view). */
    QImage renderFrame(float angleDeg, const QVector3D& axis, const QSize& size);

    /*  Multi-axis variants (X-then-Y-then-Z rotations, as the GUI's
     *  animation uses), for MP4 export. */
    QSize layoutForTriples(const std::vector<QVector3D>& angles, int maxW, int maxH);
    QImage renderFrameTriple(const QVector3D& anglesDeg, const QSize& size);

private:
    QMatrix4x4 modelMatrix(float angleDeg, const QVector3D& axis) const;
    QMatrix4x4 modelMatrixTriple(const QVector3D& anglesDeg) const;
    QMatrix4x4 projMatrix() const;
    QSize layoutFor(const std::vector<QMatrix4x4>& models, int maxW, int maxH);
    QImage renderWithTransform(const QMatrix4x4& transform, const QSize& size);

    QOffscreenSurface surface;
    QOpenGLContext context;
    std::unique_ptr<QOpenGLShaderProgram> shader;
    std::unique_ptr<GLMesh> glmesh;
    std::unique_ptr<QOpenGLFramebufferObject> fbo;

    RenderStyle style;

    QVector3D center;
    float scale = 1;
    QVector3D corners[8]; // mesh bounding-box corners

    // Projected-bounds fit, set by layoutForAngles
    float cx = 0, cy = 0, hx = 1, hy = 1;

    const float perspective = 0.25f;

    bool valid = false;
    QString error;
};

#endif // OFFSCREENRENDERER_H
