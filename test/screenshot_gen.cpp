// Generates documentation screenshots of fstl's interface, menus, and
// dialogs by building the real widgets and capturing them with grab().
// Output: docs/images/*.png. Run on a machine with a display.
#include <QApplication>
#include <QMenu>
#include <QMenuBar>
#include <QPainter>
#include <QPixmap>
#include <QThread>
#include <QTimer>

#include "../src/animatedialog.h"
#include "../src/canvas.h"
#include "../src/exportdialog.h"
#include "../src/keybindingsdialog.h"
#include "../src/loader.h"
#include "../src/shaderlightprefs.h"
#include "../src/window.h"

static QString outDir;

static void save(QWidget* w, const QString& name)
{
    if (!w) {
        return;
    }
    const QPixmap pm = w->grab();
    const QString path = outDir + "/" + name + ".png";
    if (pm.save(path)) {
        printf("wrote %s (%dx%d)\n", qPrintable(path), pm.width(), pm.height());
    } else {
        printf("FAILED %s\n", qPrintable(path));
    }
}

static void saveMenu(QMenu* m, const QString& name)
{
    if (!m) {
        return;
    }
    m->popup(QPoint(0, 0)); // realize and lay out the popup
    qApp->processEvents();
    save(m, name);
    m->close();
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    outDir = (argc > 1) ? argv[1] : "docs/images";

    // --- Main window with the demo model ---
    Window window;
    window.resize(900, 620);
    window.show();
    window.load_stl(":gl/sphere.stl"); // App normally does this, not Window
    // Let the async loader finish and the GL canvas render several frames
    for (int i = 0; i < 25; ++i) {
        qApp->processEvents();
        QThread::msleep(50);
    }
    // grab() captures window chrome but not QOpenGLWidget content, so
    // composite the canvas's framebuffer into the canvas's rectangle
    QPixmap shot = window.grab();
    if (auto canvasWidget = window.findChild<Canvas*>()) {
        const QImage gl = canvasWidget->grabFramebuffer();
        const QPoint origin = canvasWidget->mapTo(&window, QPoint(0, 0));
        QPainter painter(&shot);
        painter.drawImage(QRect(origin, canvasWidget->size()), gl);
    }
    if (shot.save(outDir + "/interface.png")) {
        printf("wrote %s/interface.png (%dx%d)\n", qPrintable(outDir), shot.width(), shot.height());
    }

    // --- Top-level menus ---
    const auto bar = window.menuBar();
    for (QAction* a : bar->actions()) {
        if (a->menu()) {
            const QString title = a->text().remove('&').toLower();
            saveMenu(a->menu(), "menu-" + title);
            // Submenus of View (Projection, Draw Mode, Viewpoint)
            for (QAction* sub : a->menu()->actions()) {
                if (sub->menu()) {
                    saveMenu(sub->menu(), "menu-" + title + "-" + sub->text().remove('&').toLower());
                }
            }
        }
    }

    // --- Dialogs (built against a Canvas; no GL needed for these) ---
    QSurfaceFormat fmt;
    fmt.setVersion(2, 1);
    auto canvas = new Canvas(fmt);

    ShaderLightPrefs prefs(nullptr, canvas);
    save(&prefs, "dialog-draw-mode-settings");

    RotationAnimationDialog anim(nullptr, canvas);
    save(&anim, "dialog-animate");

    PngExportDialog pngDlg;
    save(&pngDlg, "dialog-export-png");
    GifExportDialog gifDlg;
    save(&gifDlg, "dialog-export-gif");
    Mp4ExportDialog mp4Dlg;
    save(&mp4Dlg, "dialog-export-mp4");

    printf("done\n");
    QTimer::singleShot(0, &app, &QApplication::quit);
    return app.exec();
}
