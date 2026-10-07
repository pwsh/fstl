#include <QApplication>

#include <cstring>

#ifdef Q_OS_UNIX
#    include <cerrno>
#    include <sys/stat.h>
#    include <sys/types.h>
#    include <unistd.h>
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
        // The path is predictable, so another user could pre-create it:
        // only use it if it is a real directory we own with no group or
        // other access.  Otherwise leave XDG_RUNTIME_DIR unset (Qt then
        // just warns and uses its own fallback).
        const QByteArray path = QByteArray("/tmp/fstl-runtime-") + QByteArray::number(qulonglong(getuid()));
        if (mkdir(path.constData(), 0700) != 0 && errno != EEXIST) {
            return;
        }
        struct stat st;
        if (lstat(path.constData(), &st) != 0 || !S_ISDIR(st.st_mode) || st.st_uid != getuid() || (st.st_mode & 077) != 0) {
            return;
        }
        qputenv("XDG_RUNTIME_DIR", path);
    }
#endif
}

/*  Detach the GUI from the launching terminal so the shell prompt returns
 *  immediately. Must run before the QApplication exists.
 *
 *  Only forks when there is actually a controlling terminal to release.
 *  When launched from a desktop/file-manager there is no terminal, and
 *  forking there detaches the process from the desktop's launch tracking,
 *  which makes GNOME show a "'fstl' is ready" attention notification when
 *  a window later maps. So a GUI launch is left attached (it has nothing
 *  to detach from anyway). */
void detach_from_terminal()
{
#ifdef Q_OS_UNIX
    if (!isatty(STDIN_FILENO) && !isatty(STDOUT_FILENO) && !isatty(STDERR_FILENO)) {
        return; // not launched from a terminal; nothing to release
    }
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
#ifdef Q_OS_UNIX
        // Headless export on X11/Wayland: no window is shown, so fall back
        // to the offscreen platform when there is no display to connect to.
        // (Not on Windows, where the windows platform is always available.)
        if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM") && !qEnvironmentVariableIsSet("DISPLAY") &&
            !qEnvironmentVariableIsSet("WAYLAND_DISPLAY")) {
            qputenv("QT_QPA_PLATFORM", "offscreen");
        }
#endif
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
