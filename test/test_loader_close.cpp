// Regression test: destroying the Window while a Loader thread is still
// running used to hit qFatal("QThread: Destroyed while thread is still
// running"). ~Window must wait for the loader and hand any delivered mesh
// to the canvas (or free it) rather than abort.
#include <QApplication>

#include "../src/window.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    for (int i = 0; i < 5; ++i) {
        auto window = new Window();
        window->show();
        window->load_stl(":gl/sphere.stl"); // Loader thread is now running
        delete window;                      // must wait() for the loader, not abort
        app.processEvents();
    }

    printf("PASS: loader wait on close\n");
    return 0;
}
