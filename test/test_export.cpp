// Ad-hoc verification harness (not part of the app build).
// 1. Loads the bundled sphere STL into a real Canvas
// 2. Grabs frames at several rotations and checks they differ
// 3. Checks the angle-sequence generators
// 4. Encodes the frames with gif.h and checks the output file
#include <QApplication>
#include <QImage>
#include <QSurfaceFormat>
#include <cstdio>

#include "../src/canvas.h"
#include "../src/exportdialog.h"
#include "../src/gif.h"
#include "../src/loader.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setVersion(2, 1);

    Canvas canvas(format);
    canvas.resize(320, 240);
    canvas.show();
    app.processEvents();

    // Load the bundled sphere synchronously
    bool loaded = false;
    Loader loader(nullptr, ":gl/sphere.stl", false);
    QObject::connect(&loader, &Loader::got_mesh, &canvas,
                     [&](Mesh* m, bool reload) {
                         canvas.load_mesh(m, reload);
                         loaded = true;
                     },
                     Qt::DirectConnection);
    loader.run();
    if (!loaded) {
        printf("FAIL: mesh did not load\n");
        return 1;
    }
    app.processEvents();

    // Grab frames at different angles; they must be valid and not identical
    QImage f0 = canvas.grabRotatedFrame(0, QVector3D(0, 1, 0));
    QImage f90 = canvas.grabRotatedFrame(90, QVector3D(0, 1, 0));
    QImage f180 = canvas.grabRotatedFrame(180, QVector3D(1, 0, 0));
    if (f0.isNull() || f90.isNull() || f180.isNull()) {
        printf("FAIL: null frame\n");
        return 1;
    }
    if (f0 == f90 && f0 == f180) {
        printf("FAIL: rotated frames are identical\n");
        return 1;
    }

    // Animation frames are absolute (default-orientation based), so the
    // same angles must reproduce the identical image even after the view
    // has been disturbed
    canvas.rotateView(33, 21);
    QImage anim1 = canvas.grabAnimationFrame(0, 90, 0);
    canvas.rotateView(-10, 5);
    QImage anim2 = canvas.grabAnimationFrame(0, 90, 0);
    QImage anim3 = canvas.grabAnimationFrame(45, 90, 0);
    if (anim1 != anim2 || anim1 == anim3) {
        printf("FAIL: animation frames not reproducible/distinct\n");
        return 1;
    }

    // Angle sequences
    RotationExportOptions png;
    png.frames = 8;
    png.sweep = 360;
    if (png.pngAngles().size() != 8 || png.pngAngles()[1] != 45.0f) {
        printf("FAIL: pngAngles\n");
        return 1;
    }
    RotationExportOptions loop;
    loop.sweep = 360;
    loop.step = 3;
    if (loop.gifAngles().size() != 120) {
        printf("FAIL: gifAngles loop count %zu\n", loop.gifAngles().size());
        return 1;
    }
    RotationExportOptions bounce;
    bounce.bounce = true;
    bounce.startAngle = -90;
    bounce.endAngle = 90;
    bounce.step = 10;
    // forward: -90..90 inclusive = 19, back: 80..-80 = 17 → 36
    if (bounce.gifAngles().size() != 36) {
        printf("FAIL: gifAngles bounce count %zu\n", bounce.gifAngles().size());
        return 1;
    }

    // Encode a short GIF through the same calls the app uses
    GifWriter writer;
    const char* gif_path = "/tmp/fstl_test.gif";
    QImage a = f0.convertToFormat(QImage::Format_RGBA8888);
    QImage b = f90.convertToFormat(QImage::Format_RGBA8888);
    if (!GifBegin(&writer, gif_path, a.width(), a.height(), 4)) {
        printf("FAIL: GifBegin\n");
        return 1;
    }
    GifWriteFrame(&writer, a.constBits(), a.width(), a.height(), 4);
    GifWriteFrame(&writer, b.constBits(), b.width(), b.height(), 4);
    GifEnd(&writer);

    FILE* f = fopen(gif_path, "rb");
    char magic[7] = {0};
    if (!f || fread(magic, 1, 6, f) != 6 || QByteArray(magic) != "GIF89a") {
        printf("FAIL: gif output invalid\n");
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fclose(f);

    printf("PASS: frames differ, angles ok, gif written (%ld bytes)\n", size);
    return 0;
}
