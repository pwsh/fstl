#include <QApplication>
#include <QDir>
#include <QFile>

#include <cstring>

#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

#include "app.h"
#include "cliexport.h"

namespace
{
/*  Qt warns and falls back to a shared /tmp path when XDG_RUNTIME_DIR is
 *  unset (common under sudo or a bare shell); provide a private dir
 *  ourselves to keep the launch quiet. */
void ensure_runtime_dir()
{
#ifdef Q_OS_UNIX
    if (qEnvironmentVariableIsEmpty("XDG_RUNTIME_DIR")) {
        const QString path = QString("/tmp/fstl-runtime-%1").arg(getuid());
        QDir().mkpath(path);
        QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
        qputenv("XDG_RUNTIME_DIR", path.toLocal8Bit());
    }
#endif
}

/*  Detach the GUI from the launching terminal so the shell prompt returns
 *  immediately. Must run before the QApplication exists. */
void detach_from_terminal()
{
#ifdef Q_OS_UNIX
    const pid_t pid = fork();
    if (pid > 0) {
        _exit(0); // parent: release the terminal
    }
    if (pid == 0) {
        setsid(); // child: new session, survives the terminal closing
    }
    // on fork failure just continue attached
#endif
}
} // namespace

int main(int argc, char* argv[])
{
    // Force C locale to force decimal point
    QLocale::setDefault(QLocale::c());

    QCoreApplication::setOrganizationName("fstl-app");
    QCoreApplication::setOrganizationDomain("https://github.com/fstl-app/fstl");
    QCoreApplication::setApplicationName("fstl");
    QCoreApplication::setApplicationVersion(FSTL_VERSION);
    QGuiApplication::setDesktopFileName("fstlapp-fstl.desktop");

    ensure_runtime_dir();

    if (cli_export_requested(argc, argv)) {
        // Headless export: no window is shown, so fall back to the
        // offscreen platform when there is no display to connect to
        if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM") && !qEnvironmentVariableIsSet("DISPLAY") &&
            !qEnvironmentVariableIsSet("WAYLAND_DISPLAY")) {
            qputenv("QT_QPA_PLATFORM", "offscreen");
        }
        QGuiApplication cli_app(argc, argv);
        return run_cli_export(cli_app.arguments());
    }

    bool foreground = false;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--foreground") || !strcmp(argv[i], "-f")) {
            foreground = true;
        }
    }
    if (!foreground) {
        detach_from_terminal();
    }

    App a(argc, argv);

    return a.exec();
}
