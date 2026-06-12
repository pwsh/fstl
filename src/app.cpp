#include <QDebug>
#include <QDir>
#include <QFileOpenEvent>

#include "app.h"
#include "window.h"

App::App(int& argc, char* argv[]) : QApplication(argc, argv), window(new Window())
{
    // First positional (non-flag) argument is the file to open
    QString filename;
    const auto args = QCoreApplication::arguments();
    for (int i = 1; i < args.size(); ++i) {
        if (!args.at(i).startsWith('-')) {
            filename = args.at(i);
            break;
        }
    }
    if (!filename.isEmpty()) {
        if (filename.startsWith("~")) {
            filename.replace(0, 1, QDir::homePath());
        }
        window->load_stl(filename);
    } else {
        window->load_stl(":gl/sphere.stl");
    }
    window->show();
}

App::~App()
{
    delete window;
}

bool App::event(QEvent* e)
{
    if (e->type() == QEvent::FileOpen) {
        window->load_stl(static_cast<QFileOpenEvent*>(e)->file());
        return true;
    } else {
        return QApplication::event(e);
    }
}
