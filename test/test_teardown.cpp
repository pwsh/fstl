// Reproduces the close-time crash: build a Window (which owns the Canvas
// and the animation dialog), pump events, then destroy it. Before the
// QPointer fix this segfaulted in QWidget::update() because the dialog's
// destructor touched the already-destroyed Canvas.
#include <QApplication>
#include <QTimer>

#include "../src/window.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    auto window = new Window();
    window->show();
    app.processEvents();

    delete window; // runs ~Window -> child destruction -> dialog ~stop()
    app.processEvents();

    printf("PASS: clean teardown\n");
    return 0;
}
