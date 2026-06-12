#include "offscreenrenderer.h"
#include "glmesh.h"
#include "mesh.h"

#include <QVector4D>

#include <algorithm>
#include <cmath>

OffscreenRenderer::OffscreenRenderer(const Mesh* mesh, const RenderStyle& style) : style(style)
{
    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setVersion(2, 1);

    surface.setFormat(format);
    surface.create();
    if (!surface.isValid()) {
        error = "could not create an offscreen GL surface";
        return;
    }

    context.setFormat(format);
    if (!context.create() || !context.makeCurrent(&surface)) {
        error = "could not create an OpenGL context";
        return;
    }
    initializeOpenGLFunctions();

    shader.reset(new QOpenGLShaderProgram);
    if (!shader->addShaderFromSourceFile(QOpenGLShader::Vertex, ":/gl/mesh.vert") ||
        !shader->addShaderFromSourceFile(QOpenGLShader::Fragment, ":/gl/mesh_light.frag") || !shader->link()) {
        error = "could not compile shaders: " + shader->log();
        return;
    }

    glmesh.reset(new GLMesh(mesh));

    const QVector3D lower(mesh->xmin(), mesh->ymin(), mesh->zmin());
    const QVector3D upper(mesh->xmax(), mesh->ymax(), mesh->zmax());
    center = (lower + upper) / 2;
    scale = 2 / (upper - lower).length();
    for (int i = 0; i < 8; ++i) {
        corners[i] = QVector3D((i & 1) ? upper.x() : lower.x(), (i & 2) ? upper.y() : lower.y(), (i & 4) ? upper.z() : lower.z());
    }

    valid = true;
}

OffscreenRenderer::~OffscreenRenderer()
{
    if (context.isValid()) {
        context.makeCurrent(&surface);
        fbo.reset();
        glmesh.reset();
        shader.reset();
        context.doneCurrent();
    }
}

QMatrix4x4 OffscreenRenderer::modelMatrix(float angleDeg, const QVector3D& axis) const
{
    // Same straight-on front view as Canvas::common_view_change(frontview)
    QMatrix4x4 base;
    base.rotate(180, QVector3D(0, 0, 1));
    base.rotate(90, QVector3D(1, 0, 0));

    QMatrix4x4 rotation;
    rotation.rotate(angleDeg, axis);

    QMatrix4x4 m = rotation * base;
    m.scale(scale);
    m.translate(-center);
    return m;
}

QMatrix4x4 OffscreenRenderer::projMatrix() const
{
    QMatrix4x4 m;
    m(2, 2) = 0.5f;        // compress depth range, as the canvas does
    m(3, 2) = perspective; // perspective term
    return m;
}

QMatrix4x4 OffscreenRenderer::modelMatrixTriple(const QVector3D& anglesDeg) const
{
    QMatrix4x4 base;
    base.rotate(180, QVector3D(0, 0, 1));
    base.rotate(90, QVector3D(1, 0, 0));

    QMatrix4x4 rotation;
    rotation.rotate(anglesDeg.x(), QVector3D(1, 0, 0));
    rotation.rotate(anglesDeg.y(), QVector3D(0, 1, 0));
    rotation.rotate(anglesDeg.z(), QVector3D(0, 0, 1));

    QMatrix4x4 m = rotation * base;
    m.scale(scale);
    m.translate(-center);
    return m;
}

QSize OffscreenRenderer::layoutForAngles(const std::vector<float>& angles, const QVector3D& axis, int maxW, int maxH)
{
    std::vector<QMatrix4x4> models;
    models.reserve(angles.size());
    for (const float angle : angles) {
        models.push_back(modelMatrix(angle, axis));
    }
    return layoutFor(models, maxW, maxH);
}

QSize OffscreenRenderer::layoutForTriples(const std::vector<QVector3D>& angles, int maxW, int maxH)
{
    std::vector<QMatrix4x4> models;
    models.reserve(angles.size());
    for (const QVector3D& a : angles) {
        models.push_back(modelMatrixTriple(a));
    }
    return layoutFor(models, maxW, maxH);
}

QSize OffscreenRenderer::layoutFor(const std::vector<QMatrix4x4>& models, int maxW, int maxH)
{
    // Project the bounding-box corners for every frame to find the screen
    // rectangle the model sweeps through
    const QMatrix4x4 proj = projMatrix();
    float xmin = 1e30f, xmax = -1e30f, ymin = 1e30f, ymax = -1e30f;
    for (const QMatrix4x4& model : models) {
        const QMatrix4x4 m = proj * model;
        for (const auto& corner : corners) {
            const QVector4D v = m * QVector4D(corner, 1);
            const float sx = v.x() / v.w();
            const float sy = v.y() / v.w();
            xmin = std::min(xmin, sx);
            xmax = std::max(xmax, sx);
            ymin = std::min(ymin, sy);
            ymax = std::max(ymax, sy);
        }
    }

    cx = (xmin + xmax) / 2;
    cy = (ymin + ymax) / 2;
    const float margin = 1.02f;
    hx = std::max(1e-6f, (xmax - xmin) / 2 * margin);
    hy = std::max(1e-6f, (ymax - ymin) / 2 * margin);

    // Fit the model's aspect ratio inside maxW x maxH
    int w = maxW;
    int h = int(std::lround(w * hy / hx));
    if (h > maxH) {
        h = maxH;
        w = int(std::lround(h * hx / hy));
    }
    return QSize(std::max(1, w), std::max(1, h));
}

QImage OffscreenRenderer::renderFrame(float angleDeg, const QVector3D& axis, const QSize& size)
{
    return renderWithTransform(modelMatrix(angleDeg, axis), size);
}

QImage OffscreenRenderer::renderFrameTriple(const QVector3D& anglesDeg, const QSize& size)
{
    return renderWithTransform(modelMatrixTriple(anglesDeg), size);
}

QImage OffscreenRenderer::renderWithTransform(const QMatrix4x4& transform, const QSize& size)
{
    if (!context.makeCurrent(&surface)) {
        return QImage();
    }

    if (!fbo || fbo->size() != size) {
        QOpenGLFramebufferObjectFormat fmt;
        fmt.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
        fbo.reset(new QOpenGLFramebufferObject(size, fmt));
    }
    fbo->bind();
    glViewport(0, 0, size.width(), size.height());

    // The FBO holds premultiplied alpha, so scale rgb by the alpha
    const float a = style.background.alphaF();
    glClearColor(style.background.redF() * a, style.background.greenF() * a, style.background.blueF() * a, a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    if (style.opacity < 1.0) {
        glEnable(GL_BLEND);
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    }

    // Map the projected bounds found by the layout call to the viewport;
    // x is mirrored to match the on-screen orientation of the GUI
    QMatrix4x4 post;
    post(0, 0) = -1.0f / hx;
    post(0, 3) = cx / hx;
    post(1, 1) = 1.0f / hy;
    post(1, 3) = -cy / hy;
    const QMatrix4x4 view = post * projMatrix();

    shader->bind();
    glUniformMatrix4fv(shader->uniformLocation("transform_matrix"), 1, GL_FALSE, transform.data());
    glUniformMatrix4fv(shader->uniformLocation("view_matrix"), 1, GL_FALSE, view.data());
    glUniform1f(shader->uniformLocation("zoom"), 1.0f);
    glUniform1f(shader->uniformLocation("model_alpha"), float(style.opacity));
    const float b = float(style.brightness);
    glUniform4f(shader->uniformLocation("ambient_light_color"), style.modelColor.redF(), style.modelColor.greenF(),
                style.modelColor.blueF(), 0.6f * b);
    glUniform4f(shader->uniformLocation("directive_light_color"), style.modelColor.redF(), style.modelColor.greenF(),
                style.modelColor.blueF(), 0.4f * b);
    glUniform3f(shader->uniformLocation("directive_light_direction"), style.lightDirection.x(), style.lightDirection.y(),
                style.lightDirection.z());

    const GLuint vp = shader->attributeLocation("vertex_position");
    glEnableVertexAttribArray(vp);
    glmesh->draw(vp);
    glDisableVertexAttribArray(vp);
    shader->release();
    glDisable(GL_BLEND);

    QImage img = fbo->toImage();
    fbo->release();
    return img.convertToFormat(QImage::Format_RGBA8888);
}
