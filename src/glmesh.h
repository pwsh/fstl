#ifndef GLMESH_H
#define GLMESH_H

#include <QOpenGLBuffer>
#include <QOpenGLFunctions>

// forward declaration
class Mesh;

class GLMesh : protected QOpenGLFunctions
{
public:
    GLMesh(const Mesh* const mesh);
    void draw(GLuint vp);

private:
    QOpenGLBuffer vertices;
    QOpenGLBuffer indices;
    GLsizei index_count = 0; // cached: QOpenGLBuffer::size() queries GL
};

#endif // GLMESH_H
