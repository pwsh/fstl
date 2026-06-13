#include <QDirIterator>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QImage>
#include <QKeyEvent>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QDialogButtonBox>
#include <QProcess>
#include <QProgressDialog>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QSettings>
#include <QStandardPaths>

#include "animatedialog.h"
#include "canvas.h"
#include "exportdialog.h"
#include "gif.h"
#include "loader.h"
#include "shaderlightprefs.h"
#include "statisticsdialog.h"
#include "window.h"

const QString Window::OPEN_EXTERNAL_KEY = "externalCmd";
const QString Window::RECENT_FILE_KEY = "recentFiles";
const QString Window::INVERT_ZOOM_KEY = "invertZoom";
const QString Window::AUTORELOAD_KEY = "autoreload";
const QString Window::DRAW_AXES_KEY = "drawAxes";
const QString Window::PROJECTION_KEY = "projection";
const QString Window::DRAW_MODE_KEY = "drawMode";
const QString Window::WINDOW_GEOM_KEY = "windowGeometry";
const QString Window::RESET_TRANSFORM_ON_LOAD_KEY = "resetTransformOnLoad";
const QString Window::MOMENTUM_KEY = "momentumSpin";

Window::Window(QWidget* parent) :
    QMainWindow(parent),
    open_action(new QAction("&Open", this)),
    open_external_action(new QAction("Open w&ith", this)),
    about_action(new QAction("&About", this)),
    quit_action(new QAction("&Quit", this)),
    perspective_action(new QAction("&Perspective", this)),
    common_view_center_action(new QAction("&Center the model", this)),
    common_view_iso_action(new QAction("&Isometric", this)),
    common_view_top_action(new QAction("&Top", this)),
    common_view_bottom_action(new QAction("&Bottom", this)),
    common_view_left_action(new QAction("&Left", this)),
    common_view_right_action(new QAction("&Right", this)),
    common_view_front_action(new QAction("&Front", this)),
    common_view_back_action(new QAction("B&ack", this)),
    orthographic_action(new QAction("&Orthographic", this)),
    shaded_action(new QAction("&Shaded", this)),
    wireframe_action(new QAction("&Wireframe", this)),
    surfaceangle_action(new QAction("Surface A&ngle", this)),
    meshlight_action(new QAction("Shaded &ambient and directive light source", this)),
    drawModePrefs_action(new QAction("Draw Mode &Settings")),
    axes_action(new QAction("Draw &Axes", this)),
    invert_zoom_action(new QAction("Invert &Zoom", this)),
    reload_action(new QAction("Re&load", this)),
    autoreload_action(new QAction("&Autoreload", this)),
    save_screenshot_action(new QAction("Save &Screenshot", this)),
    export_png_rotation_action(new QAction("Export Rotation &PNGs...", this)),
    export_gif_rotation_action(new QAction("Export Rotating &GIF...", this)),
    export_mp4_rotation_action(new QAction("Export Rotation &MP4...", this)),
    export_settings_action(new QAction("Export Se&ttings...", this)),
    import_settings_action(new QAction("Import Settin&gs...", this)),
    up_axis_z_action(new QAction("&Z up (default)", this)),
    up_axis_y_action(new QAction("&Y up", this)),
    momentum_spin_action(new QAction("Momentum &Spin", this)),
    animate_action(new QAction("Animate &Rotation...", this)),
    statistics_action(new QAction("S&tatistics...", this)),
    keybindings_action(new QAction("Configure &Keyboard Shortcuts...", this)),
    help_usage_action(new QAction("&Usage and Controls", this)),
    hide_menuBar_action(new QAction("Hide &Menu Bar", this)),
    fullscreen_action(new QAction("Toggle &Fullscreen", this)),
    resetTransformOnLoadAction(new QAction("Reset rotation on load", this)),
    recent_files(new QMenu("Open &recent", this)),
    recent_files_group(new QActionGroup(this)),
    recent_files_clear_action(new QAction("&Clear recent files", this)),
    watcher(new QFileSystemWatcher(this))

{
    setWindowTitle("fstl");
    setWindowIcon(QIcon(":/qt/icons/fstl_64x64.png"));
    setAcceptDrops(true);

    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setVersion(2, 1);
    format.setProfile(QSurfaceFormat::CoreProfile);

    QSurfaceFormat::setDefaultFormat(format);

    canvas = new Canvas(format, this);
    setCentralWidget(canvas);

    meshlightprefs = new ShaderLightPrefs(this, canvas);

    QObject::connect(drawModePrefs_action, &QAction::triggered, this, &Window::on_drawModePrefs);

    QObject::connect(watcher, &QFileSystemWatcher::fileChanged, this, &Window::on_watched_change);

    open_action->setShortcut(QKeySequence::Open);
    QObject::connect(open_action, &QAction::triggered, this, &Window::on_open);
    this->addAction(open_action);

    open_external_action->setShortcut(QKeySequence::Open);
    QObject::connect(open_external_action, &QAction::triggered, this, &Window::on_open_external);
    this->addAction(open_external_action);
    open_external_action->setShortcut(QKeySequence(Qt::ALT | Qt::Key_S));

    QList<QKeySequence> quitShortcuts = {QKeySequence::Quit, QKeySequence::Close};
    quit_action->setShortcuts(quitShortcuts);
    QObject::connect(quit_action, &QAction::triggered, this, &Window::close);
    this->addAction(quit_action);

    autoreload_action->setCheckable(true);
    QObject::connect(autoreload_action, &QAction::triggered, this, &Window::on_autoreload_triggered);

    reload_action->setShortcut(QKeySequence::Refresh);
    reload_action->setEnabled(false);
    QObject::connect(reload_action, &QAction::triggered, this, &Window::on_reload);

    QObject::connect(about_action, &QAction::triggered, this, &Window::on_about);

    QObject::connect(recent_files_clear_action, &QAction::triggered, this, &Window::on_clear_recent);
    QObject::connect(recent_files_group, &QActionGroup::triggered, this, &Window::on_load_recent);

    save_screenshot_action->setCheckable(false);
    save_screenshot_action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_S));
    QObject::connect(save_screenshot_action, &QAction::triggered, this, &Window::on_save_screenshot);
    this->addAction(save_screenshot_action);

    export_png_rotation_action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    QObject::connect(export_png_rotation_action, &QAction::triggered, this, &Window::on_export_png_rotation);
    this->addAction(export_png_rotation_action);

    export_gif_rotation_action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_G));
    QObject::connect(export_gif_rotation_action, &QAction::triggered, this, &Window::on_export_gif_rotation);
    this->addAction(export_gif_rotation_action);

    export_mp4_rotation_action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_M));
    QObject::connect(export_mp4_rotation_action, &QAction::triggered, this, &Window::on_export_mp4_rotation);
    this->addAction(export_mp4_rotation_action);

    QObject::connect(export_settings_action, &QAction::triggered, this, &Window::on_export_settings);
    QObject::connect(import_settings_action, &QAction::triggered, this, &Window::on_import_settings);

    rebuild_recent_files();

    const auto file_menu = menuBar()->addMenu("&File");
    file_menu->addAction(open_action);
    file_menu->addAction(open_external_action);
    file_menu->addMenu(recent_files);
    file_menu->addSeparator();
    file_menu->addAction(reload_action);
    file_menu->addAction(autoreload_action);
    file_menu->addAction(save_screenshot_action);
    file_menu->addAction(export_png_rotation_action);
    file_menu->addAction(export_gif_rotation_action);
    file_menu->addAction(export_mp4_rotation_action);
    file_menu->addSeparator();
    file_menu->addAction(export_settings_action);
    file_menu->addAction(import_settings_action);
    file_menu->addSeparator();
    file_menu->addAction(quit_action);

    const auto view_menu = menuBar()->addMenu("&View");
    const auto projection_menu = view_menu->addMenu("&Projection");
    projection_menu->addAction(perspective_action);
    projection_menu->addAction(orthographic_action);
    const auto projections = new QActionGroup(projection_menu);
    for (auto p : {perspective_action, orthographic_action}) {
        projections->addAction(p);
        p->setCheckable(true);
    }
    projections->setExclusive(true);
    QObject::connect(projections, &QActionGroup::triggered, this, &Window::on_projection);

    const auto draw_menu = view_menu->addMenu("&Draw Mode");
    draw_menu->addAction(shaded_action);
    draw_menu->addAction(wireframe_action);
    draw_menu->addAction(surfaceangle_action);
    draw_menu->addAction(meshlight_action);
    const auto drawModes = new QActionGroup(draw_menu);
    for (auto p : {shaded_action, wireframe_action, surfaceangle_action, meshlight_action}) {
        drawModes->addAction(p);
        p->setCheckable(true);
    }
    drawModes->setExclusive(true);
    QObject::connect(drawModes, &QActionGroup::triggered, this, &Window::on_drawMode);
    view_menu->addAction(drawModePrefs_action);

    const auto common_menu = view_menu->addMenu("&Viewpoint");
    common_menu->addAction(common_view_iso_action);
    common_menu->addAction(common_view_top_action);
    common_menu->addAction(common_view_bottom_action);
    common_menu->addAction(common_view_front_action);
    common_menu->addAction(common_view_back_action);
    common_menu->addAction(common_view_left_action);
    common_menu->addAction(common_view_right_action);
    common_menu->addAction(common_view_center_action);
    const auto common_views = new QActionGroup(common_menu);
    common_views->addAction(common_view_iso_action);
    common_views->addAction(common_view_top_action);
    common_views->addAction(common_view_bottom_action);
    common_views->addAction(common_view_front_action);
    common_views->addAction(common_view_back_action);
    common_views->addAction(common_view_left_action);
    common_views->addAction(common_view_right_action);
    common_views->addAction(common_view_center_action);
    common_view_iso_action->setShortcut(Qt::Key_0);
    common_view_top_action->setShortcut(Qt::Key_1);
    common_view_bottom_action->setShortcut(Qt::Key_2);
    common_view_front_action->setShortcut(Qt::Key_3);
    common_view_back_action->setShortcut(Qt::Key_4);
    common_view_left_action->setShortcut(Qt::Key_5);
    common_view_right_action->setShortcut(Qt::Key_6);
    common_view_center_action->setShortcut(Qt::Key_9);
    QObject::connect(common_views, &QActionGroup::triggered, this, &Window::on_common_view_change);

    const auto up_axis_menu = view_menu->addMenu("&Up Axis");
    up_axis_menu->addAction(up_axis_z_action);
    up_axis_menu->addAction(up_axis_y_action);
    const auto up_axes = new QActionGroup(up_axis_menu);
    for (auto a : {up_axis_z_action, up_axis_y_action}) {
        up_axes->addAction(a);
        a->setCheckable(true);
    }
    up_axes->setExclusive(true);
    up_axis_z_action->setChecked(!canvas->upAxisIsY());
    up_axis_y_action->setChecked(canvas->upAxisIsY());
    QObject::connect(up_axes, &QActionGroup::triggered, this,
                     [this](QAction* a) { canvas->setUpAxisIsY(a == up_axis_y_action); });

    view_menu->addAction(axes_action);
    axes_action->setCheckable(true);
    QObject::connect(axes_action, &QAction::triggered, this, &Window::on_drawAxes);

    view_menu->addAction(statistics_action);
    QObject::connect(statistics_action, &QAction::triggered, this, &Window::on_statistics_dialog);

    view_menu->addAction(invert_zoom_action);
    invert_zoom_action->setCheckable(true);
    QObject::connect(invert_zoom_action, &QAction::triggered, this, &Window::on_invertZoom);

    view_menu->addAction(resetTransformOnLoadAction);
    resetTransformOnLoadAction->setCheckable(true);
    QObject::connect(resetTransformOnLoadAction, &QAction::triggered, this, &Window::on_resetTransformOnLoad);

    view_menu->addAction(momentum_spin_action);
    momentum_spin_action->setCheckable(true);
    momentum_spin_action->setToolTip("Release a drag to keep the model spinning with the drag's momentum");
    QObject::connect(momentum_spin_action, &QAction::triggered, this, [this](bool on) {
        canvas->setMomentumEnabled(on);
        QSettings().setValue(MOMENTUM_KEY, on);
    });

    view_menu->addAction(animate_action);
    QObject::connect(animate_action, &QAction::triggered, this, &Window::on_animate_dialog);

    view_menu->addAction(keybindings_action);
    QObject::connect(keybindings_action, &QAction::triggered, this, &Window::on_keybindings);

    view_menu->addAction(hide_menuBar_action);
    hide_menuBar_action->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_C);
    hide_menuBar_action->setCheckable(true);
    QObject::connect(hide_menuBar_action, &QAction::toggled, this, &Window::on_hide_menuBar);
    this->addAction(hide_menuBar_action);

    view_menu->addAction(fullscreen_action);
    fullscreen_action->setShortcut(Qt::Key_F11);
    fullscreen_action->setCheckable(true);
    QObject::connect(fullscreen_action, &QAction::toggled, this, &Window::on_fullscreen);
    this->addAction(fullscreen_action);

    animateDialog = new RotationAnimationDialog(this, canvas);
    // Restore the persisted statistics overlay without needing the dialog
    StatisticsDialog::applySaved(canvas);

    setup_bindable_actions();

    auto help_menu = menuBar()->addMenu("&Help");
    help_usage_action->setShortcut(QKeySequence::HelpContents); // F1
    QObject::connect(help_usage_action, &QAction::triggered, this, &Window::on_help_usage);
    help_menu->addAction(help_usage_action);
    this->addAction(help_usage_action);
    help_menu->addAction(about_action);

    load_persist_settings();
}

void Window::load_persist_settings()
{
    QSettings settings;
    bool invert_zoom = settings.value(INVERT_ZOOM_KEY, false).toBool();
    canvas->invert_zoom(invert_zoom);
    invert_zoom_action->setChecked(invert_zoom);

    bool resetTransformOnLoad = settings.value(RESET_TRANSFORM_ON_LOAD_KEY, true).toBool();
    canvas->setResetTransformOnLoad(resetTransformOnLoad);
    resetTransformOnLoadAction->setChecked(resetTransformOnLoad);

    autoreload_action->setChecked(settings.value(AUTORELOAD_KEY, true).toBool());

    bool draw_axes = settings.value(DRAW_AXES_KEY, false).toBool();
    canvas->draw_axes(draw_axes);
    axes_action->setChecked(draw_axes);

    bool momentum = settings.value(MOMENTUM_KEY, false).toBool();
    canvas->setMomentumEnabled(momentum);
    momentum_spin_action->setChecked(momentum);

    QString projection = settings.value(PROJECTION_KEY, "perspective").toString();
    if (projection == "perspective") {
        canvas->view_perspective(Canvas::P_PERSPECTIVE, false);
        perspective_action->setChecked(true);
    } else {
        canvas->view_perspective(Canvas::P_ORTHOGRAPHIC, false);
        orthographic_action->setChecked(true);
    }

    QString path = settings.value(OPEN_EXTERNAL_KEY, "").toString();
    if (!QDir::isAbsolutePath(path) && !path.isEmpty()) {
        path = QStandardPaths::findExecutable(path);
    }
    QString displayName = path.mid(path.lastIndexOf(QDir::separator()) + 1);
    open_external_action->setText("Open w&ith " + displayName);
    open_external_action->setData(path);

    DrawMode draw_mode = (DrawMode)settings.value(DRAW_MODE_KEY, DRAWMODECOUNT).toInt();

    if (draw_mode >= DRAWMODECOUNT) {
        draw_mode = shaded;
    }
    QAction* dm_acts[] = {shaded_action, wireframe_action, surfaceangle_action, meshlight_action};
    dm_acts[draw_mode]->setChecked(true);
    on_drawMode(dm_acts[draw_mode]);

    resize(600, 400);
    restoreGeometry(settings.value(WINDOW_GEOM_KEY).toByteArray());
}

void Window::on_statistics_dialog()
{
    // Created fresh each time (like the Usage dialog) rather than kept as
    // a hidden-then-reshown window; reuse the open one if present.
    auto existing = findChild<StatisticsDialog*>();
    if (existing) {
        existing->raise();
        existing->activateWindow();
        return;
    }
    auto dialog = new StatisticsDialog(this, canvas);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void Window::setup_bindable_actions()
{
    // Movement and file-navigation actions live only as shortcuts (no
    // menu entries); auto-repeat makes held keys move continuously
    const auto makeMove = [this](const QString& id, const QString& label, const QKeySequence& def,
                                 const std::function<void()>& fn) {
        auto act = new QAction(label, this);
        act->setShortcut(def);
        act->setAutoRepeat(true);
        QObject::connect(act, &QAction::triggered, this, [fn] { fn(); });
        this->addAction(act);
        bindable_actions.append({id, label, act, def});
    };

    Canvas* c = canvas;
    makeMove("prev_file", "Previous file in folder", QKeySequence(Qt::Key_Left), [this] { load_prev(); });
    makeMove("next_file", "Next file in folder", QKeySequence(Qt::Key_Right), [this] { load_next(); });
    makeMove("rotate_up", "Rotate up", QKeySequence(Qt::Key_W), [c] { c->rotateView(-5, 0); });
    makeMove("rotate_down", "Rotate down", QKeySequence(Qt::Key_S), [c] { c->rotateView(5, 0); });
    makeMove("rotate_left", "Rotate left", QKeySequence(Qt::Key_A), [c] { c->rotateView(0, -5); });
    makeMove("rotate_right", "Rotate right", QKeySequence(Qt::Key_D), [c] { c->rotateView(0, 5); });
    makeMove("roll_ccw", "Roll counter-clockwise", QKeySequence(Qt::Key_Q), [c] { c->rollView(-5); });
    makeMove("roll_cw", "Roll clockwise", QKeySequence(Qt::Key_E), [c] { c->rollView(5); });
    makeMove("pan_up", "Pan up", QKeySequence(Qt::SHIFT | Qt::Key_W), [c] { c->panView(0, -0.05f); });
    makeMove("pan_down", "Pan down", QKeySequence(Qt::SHIFT | Qt::Key_S), [c] { c->panView(0, 0.05f); });
    makeMove("pan_left", "Pan left", QKeySequence(Qt::SHIFT | Qt::Key_A), [c] { c->panView(-0.05f, 0); });
    makeMove("pan_right", "Pan right", QKeySequence(Qt::SHIFT | Qt::Key_D), [c] { c->panView(0.05f, 0); });
    makeMove("zoom_in", "Zoom in", QKeySequence(Qt::Key_Plus), [c] { c->zoomView(1 / 1.15f); });
    makeMove("zoom_out", "Zoom out", QKeySequence(Qt::Key_Minus), [c] { c->zoomView(1.15f); });

    // Common operations are rebindable too
    const auto bindable = [this](const QString& id, const QString& label, QAction* act) {
        bindable_actions.append({id, label, act, act->shortcut()});
    };
    bindable("open", "Open file", open_action);
    bindable("reload", "Reload file", reload_action);
    bindable("screenshot", "Save screenshot", save_screenshot_action);
    bindable("export_png", "Export rotation PNGs", export_png_rotation_action);
    bindable("export_gif", "Export rotating GIF", export_gif_rotation_action);
    bindable("export_mp4", "Export rotation MP4", export_mp4_rotation_action);
    bindable("fullscreen", "Toggle fullscreen", fullscreen_action);
    bindable("hide_menubar", "Hide menu bar", hide_menuBar_action);

    apply_saved_key_bindings(bindable_actions);
}

void Window::on_keybindings()
{
    KeyBindingsDialog dialog(this, bindable_actions);
    dialog.exec();
}

void Window::on_animate_dialog()
{
    if (animateDialog->isVisible()) {
        animateDialog->hide();
    } else {
        animateDialog->show();
    }
}

void Window::on_export_mp4_rotation()
{
    const QString ffmpeg_path = QStandardPaths::findExecutable("ffmpeg");
    if (ffmpeg_path.isEmpty()) {
        QMessageBox::warning(this, tr("ffmpeg not found"),
                             tr("MP4 export requires ffmpeg.\n"
                                "Install it (e.g. 'sudo apt install ffmpeg') and try again."));
        return;
    }

    Mp4ExportDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const auto opt = dialog.options();

    auto file_name = QFileDialog::getSaveFileName(
        this, tr("Export Rotation MP4"),
        QStandardPaths::standardLocations(QStandardPaths::StandardLocation::MoviesLocation).first(), "MP4 video (*.mp4)");
    if (file_name.isEmpty()) {
        return;
    }
    if (!file_name.endsWith(".mp4", Qt::CaseInsensitive)) {
        file_name.append(".mp4");
    }

    // Frame size: scale the canvas frame to the requested width, even
    // dimensions (libx264 requirement)
    QImage first = canvas->grabAnimationFrame(0, 0, 0);
    if (first.width() > opt.width) {
        first = first.scaledToWidth(opt.width, Qt::SmoothTransformation);
    }
    const int w = first.width() & ~1;
    const int h = first.height() & ~1;

    QProcess proc;
    proc.setProcessChannelMode(QProcess::ForwardedErrorChannel);
    proc.start(ffmpeg_path, {"-y", "-loglevel", "error", "-f", "rawvideo", "-pix_fmt", "rgba", "-s",
                             QString("%1x%2").arg(w).arg(h), "-r", QString::number(opt.fps), "-i", "-", "-c:v", "libx264",
                             "-preset", "veryfast", "-pix_fmt", "yuv420p", "-movflags", "+faststart", file_name});
    if (!proc.waitForStarted(5000)) {
        QMessageBox::warning(this, tr("Export failed"), tr("Could not start ffmpeg."));
        return;
    }

    const int total = std::max(1, int(std::lround(opt.duration * opt.fps)));
    QProgressDialog progress(tr("Exporting MP4..."), tr("Cancel"), 0, total, this);
    progress.setWindowModality(Qt::WindowModal);

    bool cancelled = false;
    for (int i = 0; i < total; ++i) {
        progress.setValue(i);
        if (progress.wasCanceled()) {
            cancelled = true;
            break;
        }
        // Angles advance linearly: degrees * elapsed/duration, so the
        // per-axis speed ratio follows the requested totals
        const double f = (double(i) / opt.fps) / opt.duration;
        QImage frame = canvas->grabAnimationFrame(opt.degreesX * f, opt.degreesY * f, opt.degreesZ * f)
                           .scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                           .convertToFormat(QImage::Format_RGBA8888);
        proc.write((const char*)frame.constBits(), qint64(w) * h * 4);
        // Bound the write buffer so a slow encoder can't balloon memory
        while (proc.bytesToWrite() > 64 * 1024 * 1024) {
            proc.waitForBytesWritten(100);
        }
    }

    if (cancelled) {
        proc.kill();
        proc.waitForFinished(5000);
        QFile::remove(file_name);
        return;
    }

    progress.setLabelText(tr("Encoding..."));
    proc.closeWriteChannel();
    proc.waitForFinished(-1);
    progress.setValue(total);

    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        QMessageBox::warning(this, tr("Export failed"), tr("ffmpeg reported an error while encoding."));
    }
}

void Window::on_export_settings()
{
    auto path = QFileDialog::getSaveFileName(
        this, tr("Export fstl settings"),
        QStandardPaths::standardLocations(QStandardPaths::StandardLocation::DocumentsLocation).first() + "/fstl-settings.ini",
        "Settings files (*.ini)");
    if (path.isEmpty()) {
        return;
    }
    if (!path.endsWith(".ini", Qt::CaseInsensitive)) {
        path.append(".ini");
    }

    QSettings current;
    QSettings out(path, QSettings::IniFormat);
    out.clear();
    for (const QString& key : current.allKeys()) {
        // Window geometry is machine-specific; everything else travels
        if (key != WINDOW_GEOM_KEY) {
            out.setValue(key, current.value(key));
        }
    }
    out.sync();
    if (out.status() != QSettings::NoError) {
        QMessageBox::warning(this, tr("Export failed"), tr("Could not write %1").arg(path));
    }
}

void Window::on_import_settings()
{
    const auto path = QFileDialog::getOpenFileName(
        this, tr("Import fstl settings"),
        QStandardPaths::standardLocations(QStandardPaths::StandardLocation::DocumentsLocation).first(),
        "Settings files (*.ini)");
    if (path.isEmpty()) {
        return;
    }

    QSettings in(path, QSettings::IniFormat);
    QSettings current;
    for (const QString& key : in.allKeys()) {
        current.setValue(key, in.value(key));
    }
    current.sync();

    // Re-apply what the window controls directly; dialogs pick the rest
    // up the next time they are opened
    load_persist_settings();
    QMessageBox::information(this, tr("Settings imported"),
                             tr("Imported %1 settings from %2.\n"
                                "Some changes (dialogs, key bindings) apply when reopened;\n"
                                "restart fstl to apply everything.")
                                 .arg(in.allKeys().size())
                                 .arg(QFileInfo(path).fileName()));
}

void Window::on_drawModePrefs()
{
    // For now only one draw mode has settings
    // when settings for other draw mode will be available
    // we will need to check the current mode
    if (meshlightprefs->isVisible()) {
        meshlightprefs->hide();
    } else {
        meshlightprefs->show();
    }
}

void Window::on_open()
{
    const QString filename = QFileDialog::getOpenFileName(this, "Load .stl file", QString(), "STL files (*.stl *.STL)");
    if (!filename.isNull()) {
        load_stl(filename);
    }
}

void Window::on_open_external() const
{
    if (current_file.isEmpty()) {
        return;
    }

    QString program = open_external_action->data().toString();
    if (program.isEmpty()) {
        program = QFileDialog::getOpenFileName((QWidget*)this, "Select program to open with", QDir::rootPath());
        if (!program.isEmpty()) {
            QSettings settings;
            settings.setValue(OPEN_EXTERNAL_KEY, program);
            QString displayName = program.mid(program.lastIndexOf(QDir::separator()) + 1);
            open_external_action->setText("Open w&ith " + displayName);
            open_external_action->setData(program);
        }
    }

    QProcess::startDetached(program, QStringList(current_file));
}

void Window::on_help_usage()
{
    // Modeless so the user can keep it open next to the model
    auto dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle("fstl - Usage and Controls");
    dialog->resize(620, 560);

    auto browser = new QTextBrowser(dialog);
    browser->setOpenExternalLinks(true);
    browser->setHtml(
        "<h2>Mouse controls</h2>"
        "<table cellspacing='4'>"
        "<tr><td><b>Left-drag</b></td><td>Rotate the model (arcball)</td></tr>"
        "<tr><td><b>Right-drag</b></td><td>Pan</td></tr>"
        "<tr><td><b>Scroll wheel</b></td><td>Zoom about the cursor (invert via View &gt; Invert Zoom)</td></tr>"
        "<tr><td><b>Drag &amp; drop</b></td><td>Drop an .stl file onto the window to open it</td></tr>"
        "</table>"

        "<h2>Keyboard shortcuts</h2>"
        "<table cellspacing='4'>"
        "<tr><td><b>Ctrl+O</b></td><td>Open a file</td></tr>"
        "<tr><td><b>Alt+S</b></td><td>Open the current file with an external program</td></tr>"
        "<tr><td><b>Left / Right</b></td><td>Previous / next .stl file in the same folder</td></tr>"
        "<tr><td><b>F5</b></td><td>Reload the current file</td></tr>"
        "<tr><td><b>Ctrl+S</b></td><td>Save a screenshot</td></tr>"
        "<tr><td><b>Ctrl+E</b></td><td>Export rotation PNGs</td></tr>"
        "<tr><td><b>Ctrl+G</b></td><td>Export a rotating GIF</td></tr>"
        "<tr><td><b>Ctrl+M</b></td><td>Export a rotation MP4 (requires ffmpeg)</td></tr>"
        "<tr><td><b>W / A / S / D</b></td><td>Rotate the model in steps (hold to keep rotating)</td></tr>"
        "<tr><td><b>Q / E</b></td><td>Roll counter-clockwise / clockwise</td></tr>"
        "<tr><td><b>Shift+W/A/S/D</b></td><td>Pan</td></tr>"
        "<tr><td><b>+ / -</b></td><td>Zoom in / out</td></tr>"
        "<tr><td><b>0&ndash;6, 9</b></td><td>Viewpoints: 0 isometric, 1 top, 2 bottom, 3 front, 4 back, "
        "5 left, 6 right, 9 center</td></tr>"
        "<tr><td><b>F11</b></td><td>Toggle fullscreen</td></tr>"
        "<tr><td><b>Ctrl+Shift+C</b></td><td>Hide the menu bar (<b>Esc</b> brings it back)</td></tr>"
        "<tr><td><b>F1</b></td><td>This help</td></tr>"
        "<tr><td><b>Ctrl+Q</b></td><td>Quit</td></tr>"
        "</table>"
        "<p>Movement, file-navigation, and the common shortcuts are rebindable via "
        "<b>View &gt; Configure Keyboard Shortcuts</b>.</p>"

        "<h2>Menus</h2>"
        "<p><b>File</b></p><ul>"
        "<li><b>Open / Open recent</b> &mdash; load an .stl file (the last 8 are remembered)</li>"
        "<li><b>Open with</b> &mdash; send the current file to an external program (chosen on first use)</li>"
        "<li><b>Reload / Autoreload</b> &mdash; re-read the file manually, or automatically whenever it "
        "changes on disk (useful while exporting from CAD)</li>"
        "<li><b>Save Screenshot</b> &mdash; save the current view as PNG or JPG</li>"
        "<li><b>Export Rotation PNGs</b> &mdash; save a series of views rotated in even steps about a "
        "chosen screen axis</li>"
        "<li><b>Export Rotating GIF</b> &mdash; save an animated turntable GIF; <i>Loop</i> sweeps up to "
        "360&deg; continuously, <i>Bounce</i> swings between two angles (e.g. -90&deg; to +90&deg;)</li>"
        "<li><b>Export Rotation MP4</b> &mdash; save a video of the model rotating; set the duration and "
        "the total degrees per axis (may exceed 360&deg; &mdash; e.g. X=720&deg; with Y=360&deg; spins X twice "
        "as fast). Requires ffmpeg.</li>"
        "</ul>"
        "<p><b>View</b></p><ul>"
        "<li><b>Projection</b> &mdash; perspective or orthographic camera</li>"
        "<li><b>Draw Mode</b> &mdash; <i>Shaded</i> (depth-shaded), <i>Wireframe</i>, <i>Surface Angle</i> "
        "(colors faces by inclination), or <i>Shaded ambient and directive light source</i> (configurable "
        "colors and light direction via <b>Draw Mode Settings</b>, which also offers a <i>background "
        "color</i> picker for every mode &mdash; Reset restores the gradient)</li>"
        "<li><b>Viewpoint</b> &mdash; jump to standard views or re-center the model</li>"
        "<li><b>Up Axis</b> &mdash; whether the viewpoint presets treat <i>Z</i> (the STL / "
        "3D-printing default) or <i>Y</i> as the model's up axis. Switch to <i>Y up</i> if Top and "
        "Front appear swapped, which means the model was authored Y-up.</li>"
        "<li><b>Draw Axes</b> &mdash; show the model-space axes and the orientation hud in the corner</li>"
        "<li><b>Statistics</b> &mdash; opens a dialog to choose which figures to overlay (triangle count, "
        "bounding box, model size, orientation, rotation speed, FPS, zoom/projection, draw mode, colors, "
        "lighting). A master <i>Show statistics overlay</i> switch turns the whole overlay on or off; "
        "off by default.</li>"
        "<li><b>Invert Zoom</b> &mdash; flip the scroll-wheel zoom direction</li>"
        "<li><b>Reset rotation on load</b> &mdash; whether opening a file resets the orientation</li>"
        "<li><b>Momentum Spin</b> &mdash; when enabled, releasing a drag keeps the model spinning with "
        "the drag's velocity and trajectory; click to stop (off by default)</li>"
        "<li><b>Animate Rotation</b> &mdash; continuous rotation with per-axis speeds (default: slow Y "
        "turntable). Random mode changes speed and axis at random intervals (configurable min/max "
        "seconds) with smooth transitions, continuing from the current view. Optional random cycling of "
        "the <i>model</i>, <i>light</i>, and <i>background</i> colors (through a user palette, or any "
        "color when the palette is empty), and a selectable or randomly moving light source &mdash; all "
        "smoothly blended. Recordings save next to the source file, named after it plus the axis speeds "
        "(or <code>random</code>). Fixed-speed playback always starts from the default orientation so "
        "the same settings reproduce the same motion.</li>"
        "<li><b>Configure Keyboard Shortcuts</b> &mdash; rebind file navigation, movement, and common "
        "operations</li>"
        "</ul>"

        "<h2>Command line</h2>"
        "<p>Run <code>fstl --help</code> in a terminal or see <code>man fstl</code> for full details.</p>"
        "<table cellspacing='4'>"
        "<tr><td><code>fstl model.stl</code></td><td>open the viewer</td></tr>"
        "<tr><td><code>-f, --foreground</code></td><td>keep the GUI attached to the terminal</td></tr>"
        "<tr><td><code>--export-png / --export-gif</code></td><td>render to images without a window</td></tr>"
        "<tr><td><code>--color, --bg</code></td><td>model / background color (names or hex; background "
        "defaults to transparent)</td></tr>"
        "<tr><td><code>--width, --height</code></td><td>max output size (PNG 1024, GIF 640; aspect kept)</td></tr>"
        "<tr><td><code>--axis, --angles</code></td><td>rotation axis (x/y/z) and PNG view angles</td></tr>"
        "<tr><td><code>--sweep, --step, --fps</code></td><td>GIF loop: degrees, step per frame, speed</td></tr>"
        "<tr><td><code>--bounce --from --to</code></td><td>GIF: bounce between two angles</td></tr>"
        "<tr><td><code>-i, --input-dir, -o, --output-dir</code></td><td>batch inputs and output naming; "
        "<code>-</code> reads stdin</td></tr>"
        "</table>");

    auto buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::close);

    auto layout = new QVBoxLayout(dialog);
    layout->addWidget(browser);
    layout->addWidget(buttons);
    dialog->show();
}

void Window::on_about()
{
    QMessageBox::about(this, "",
                       "<p align=\"center\"><b>fstl</b><br>" FSTL_VERSION "</p>"
                       "<p>A fast viewer for <code>.stl</code> files, designed to load and render "
                       "very high-polygon models instantly.</p>"
                       "<p>Exports the model as screenshots, rotation PNG series, and animated "
                       "turntable GIFs &mdash; from the File menu or entirely from the command line "
                       "(<code>fstl --help</code> or <code>man fstl</code>).<br>"
                       "See <i>Help &gt; Usage and Controls</i> for mouse, keyboard, and menu help.</p>"
                       "<p><a href=\"https://github.com/fstl-app/fstl\""
                       "   style=\"color: #93a1a1;\">https://github.com/fstl-app/fstl</a></p>"
                       "<p>© 2014-2025 Matthew Keeter<br>"
                       "<a href=\"mailto:matt.j.keeter@gmail.com\""
                       "   style=\"color: #93a1a1;\">matt.j.keeter@gmail.com</a></p>");
}

void Window::on_bad_stl()
{
    QMessageBox::critical(this, "Error",
                          "<b>Error:</b><br>"
                          "This <code>.stl</code> file is invalid or corrupted.<br>"
                          "Please export it from the original source, verify, and retry.");
}

void Window::on_empty_mesh()
{
    QMessageBox::critical(this, "Error",
                          "<b>Error:</b><br>"
                          "This file is syntactically correct<br>but contains no triangles.");
}

void Window::on_missing_file()
{
    QMessageBox::critical(this, "Error",
                          "<b>Error:</b><br>"
                          "The target file is missing.<br>");
}

void Window::enable_open()
{
    open_action->setEnabled(true);
}

void Window::disable_open()
{
    open_action->setEnabled(false);
}

void Window::set_watched(const QString& filename)
{
    const auto files = watcher->files();
    if (files.size()) {
        watcher->removePaths(watcher->files());
    }
    watcher->addPath(filename);

    QSettings settings;
    auto recent = settings.value(RECENT_FILE_KEY).toStringList();
    const auto f = QFileInfo(filename).absoluteFilePath();
    recent.removeAll(f);
    recent.prepend(f);
    while (recent.size() > MAX_RECENT_FILES) {
        recent.pop_back();
    }
    settings.setValue(RECENT_FILE_KEY, recent);
    rebuild_recent_files();
}

void Window::on_projection(QAction* proj)
{
    if (proj == perspective_action) {
        canvas->view_perspective(Canvas::P_PERSPECTIVE, true);
        QSettings().setValue(PROJECTION_KEY, "perspective");
    } else {
        canvas->view_perspective(Canvas::P_ORTHOGRAPHIC, true);
        QSettings().setValue(PROJECTION_KEY, "orthographic");
    }
}

void Window::on_drawMode(QAction* act)
{
    // On mode change hide prefs first
    meshlightprefs->hide();

    // The settings dialog stays available in every mode: the background
    // picker applies to all of them (light settings only affect the lit mode)
    DrawMode mode = shaded;
    if (act == shaded_action) {
        mode = shaded;
    } else if (act == wireframe_action) {
        mode = wireframe;
    } else if (act == surfaceangle_action) {
        mode = surfaceangle;
    } else if (act == meshlight_action) {
        mode = meshlight;
    }
    canvas->set_drawMode(mode);
    QSettings().setValue(DRAW_MODE_KEY, mode);
}

void Window::on_drawAxes(bool d)
{
    canvas->draw_axes(d);
    QSettings().setValue(DRAW_AXES_KEY, d);
}

void Window::on_invertZoom(bool d)
{
    canvas->invert_zoom(d);
    QSettings().setValue(INVERT_ZOOM_KEY, d);
}

void Window::on_resetTransformOnLoad(bool d)
{
    canvas->setResetTransformOnLoad(d);
    QSettings().setValue(RESET_TRANSFORM_ON_LOAD_KEY, d);
}

void Window::on_watched_change(const QString& filename)
{
    if (autoreload_action->isChecked()) {
        load_stl(filename, true);
    }
}

void Window::on_autoreload_triggered(bool b)
{
    if (b) {
        on_reload();
    }
    QSettings().setValue(AUTORELOAD_KEY, b);
}

void Window::on_clear_recent()
{
    QSettings settings;
    settings.setValue(RECENT_FILE_KEY, QStringList());
    rebuild_recent_files();
}

void Window::on_load_recent(QAction* a)
{
    load_stl(a->data().toString());
}

void Window::on_loaded(const QString& filename)
{
    current_file = filename;
    animateDialog->setSourceFile(filename);
}

void Window::on_save_screenshot()
{
    const auto image = canvas->grabFramebuffer();
    auto file_name = QFileDialog::getSaveFileName(
        this, tr("Save Screenshot Image"),
        QStandardPaths::standardLocations(QStandardPaths::StandardLocation::PicturesLocation).first(), "Images (*.png *.jpg)");
    if (file_name.isEmpty()) {
        return; // dialog cancelled
    }

    const auto extension = QFileInfo(file_name).suffix().toLower();
    if (extension != "png" && extension != "jpg") {
        file_name.append(".png");
    }

    const auto save_ok = image.save(file_name);
    if (!save_ok) {
        QMessageBox::warning(this, tr("Error Saving Image"), tr("Unable to save screen shot image."));
    }
}

void Window::on_export_png_rotation()
{
    PngExportDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const auto opt = dialog.options();

    auto file_name = QFileDialog::getSaveFileName(
        this, tr("Export Rotation PNGs (base name)"),
        QStandardPaths::standardLocations(QStandardPaths::StandardLocation::PicturesLocation).first(), "Images (*.png)");
    if (file_name.isEmpty()) {
        return;
    }
    if (file_name.endsWith(".png", Qt::CaseInsensitive)) {
        file_name.chop(4);
    }

    const auto angles = opt.pngAngles();
    QProgressDialog progress(tr("Exporting images..."), tr("Cancel"), 0, angles.size(), this);
    progress.setWindowModality(Qt::WindowModal);

    bool all_ok = true;
    for (size_t i = 0; i < angles.size(); ++i) {
        progress.setValue(i);
        if (progress.wasCanceled()) {
            return;
        }
        const QImage frame = canvas->grabRotatedFrame(angles[i], opt.axis);
        const auto out = QString("%1_%2_%3deg.png")
                             .arg(file_name)
                             .arg(i, 3, 10, QChar('0'))
                             .arg(qRound(angles[i]));
        all_ok &= frame.save(out);
    }
    progress.setValue(angles.size());

    if (!all_ok) {
        QMessageBox::warning(this, tr("Error Saving Images"), tr("One or more images could not be saved."));
    }
}

void Window::on_export_gif_rotation()
{
    GifExportDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const auto opt = dialog.options();

    auto file_name = QFileDialog::getSaveFileName(
        this, tr("Export Rotating GIF"),
        QStandardPaths::standardLocations(QStandardPaths::StandardLocation::PicturesLocation).first(), "GIF images (*.gif)");
    if (file_name.isEmpty()) {
        return;
    }
    if (!file_name.endsWith(".gif", Qt::CaseInsensitive)) {
        file_name.append(".gif");
    }

    const auto angles = opt.gifAngles();
    if (angles.empty()) {
        return;
    }

    QProgressDialog progress(tr("Exporting GIF..."), tr("Cancel"), 0, angles.size(), this);
    progress.setWindowModality(Qt::WindowModal);

    // Render the first frame to fix the output size, then stream the rest
    const auto renderFrame = [&](float angle) {
        QImage frame = canvas->grabRotatedFrame(angle, opt.axis);
        if (frame.width() > opt.gifWidth) {
            frame = frame.scaledToWidth(opt.gifWidth, Qt::SmoothTransformation);
        }
        return frame.convertToFormat(QImage::Format_RGBA8888);
    };

    QImage first = renderFrame(angles[0]);
    const uint32_t w = first.width();
    const uint32_t h = first.height();
    const uint32_t delay_cs = std::max(1, 100 / opt.fps); // GIF delays are in centiseconds

    GifWriter writer;
    if (!GifBegin(&writer, file_name.toLocal8Bit().constData(), w, h, delay_cs)) {
        QMessageBox::warning(this, tr("Error Saving GIF"), tr("Unable to open the output file for writing."));
        return;
    }

    bool cancelled = false;
    for (size_t i = 0; i < angles.size(); ++i) {
        progress.setValue(i);
        if (progress.wasCanceled()) {
            cancelled = true;
            break;
        }
        QImage frame = (i == 0) ? first : renderFrame(angles[i]);
        if (uint32_t(frame.width()) != w || uint32_t(frame.height()) != h) {
            frame = frame.scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                        .convertToFormat(QImage::Format_RGBA8888);
        }
        GifWriteFrame(&writer, frame.constBits(), w, h, delay_cs);
    }
    GifEnd(&writer);
    progress.setValue(angles.size());

    if (cancelled) {
        QFile::remove(file_name);
    }
}

void Window::on_hide_menuBar()
{
    menuBar()->setVisible(!hide_menuBar_action->isChecked());
}

void Window::rebuild_recent_files()
{
    QSettings settings;
    QStringList files = settings.value(RECENT_FILE_KEY).toStringList();

    const auto actions = recent_files_group->actions();
    for (auto a : actions) {
        recent_files_group->removeAction(a);
    }
    recent_files->clear();

    for (const auto& f : files) {
        const auto a = new QAction(f, recent_files);
        a->setData(f);
        recent_files_group->addAction(a);
        recent_files->addAction(a);
    }
    if (files.size() == 0) {
        auto a = new QAction("No recent files", recent_files);
        recent_files->addAction(a);
        a->setEnabled(false);
    }
    recent_files->addSeparator();
    recent_files->addAction(recent_files_clear_action);
}

void Window::on_reload()
{
    auto fs = watcher->files();
    if (fs.size() == 1) {
        load_stl(fs[0], true);
    }
}

void Window::on_common_view_change(QAction* common)
{
    if (common == common_view_center_action)
        canvas->common_view_change(centerview);
    if (common == common_view_iso_action)
        canvas->common_view_change(isoview);
    if (common == common_view_top_action)
        canvas->common_view_change(topview);
    if (common == common_view_bottom_action)
        canvas->common_view_change(bottomview);
    if (common == common_view_left_action)
        canvas->common_view_change(leftview);
    if (common == common_view_right_action)
        canvas->common_view_change(rightview);
    if (common == common_view_front_action)
        canvas->common_view_change(frontview);
    if (common == common_view_back_action)
        canvas->common_view_change(backview);
}

bool Window::load_stl(const QString& filename, bool is_reload)
{
    if (!open_action->isEnabled())
        return false;

    canvas->set_status("Loading " + filename);

    Loader* loader = new Loader(this, filename, is_reload);
    connect(loader, &Loader::started, this, &Window::disable_open);

    connect(loader, &Loader::got_mesh, canvas, &Canvas::load_mesh);
    connect(loader, &Loader::error_bad_stl, this, &Window::on_bad_stl);
    connect(loader, &Loader::error_empty_mesh, this, &Window::on_empty_mesh);
    connect(loader, &Loader::error_missing_file, this, &Window::on_missing_file);

    connect(loader, &Loader::finished, loader, &Loader::deleteLater);
    connect(loader, &Loader::finished, this, &Window::enable_open);
    connect(loader, &Loader::finished, canvas, &Canvas::clear_status);

    if (filename[0] != ':') {
        connect(loader, &Loader::loaded_file, this, &Window::setWindowTitle);
        connect(loader, &Loader::loaded_file, this, &Window::set_watched);
        connect(loader, &Loader::loaded_file, this, &Window::on_loaded);
        reload_action->setEnabled(true);
    }

    loader->start();
    return true;
}

void Window::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        auto urls = event->mimeData()->urls();
        if (urls.size() == 1 && urls.front().path().endsWith(".stl", Qt::CaseInsensitive))
            event->acceptProposedAction();
    }
}

void Window::dropEvent(QDropEvent* event)
{
    load_stl(event->mimeData()->urls().front().toLocalFile());
}

void Window::closeEvent(QCloseEvent* event)
{
    // Persist geometry once on close rather than on every move/resize
    QSettings().setValue(WINDOW_GEOM_KEY, saveGeometry());
    QMainWindow::closeEvent(event);
}

void Window::sorted_insert(QStringList& list, const QCollator& collator, const QString& value)
{
    int start = 0;
    int end = list.size() - 1;
    int index = 0;
    while (start <= end) {
        int mid = (start + end) / 2;
        if (list[mid] == value) {
            return;
        }
        int compare = collator.compare(value, list[mid]);
        if (compare < 0) {
            end = mid - 1;
            index = mid;
        } else {
            start = mid + 1;
            index = start;
        }
    }

    list.insert(index, value);
}

void Window::build_folder_file_list()
{
    QString current_folder_path = QFileInfo(current_file).absoluteDir().absolutePath();
    if (!lookup_folder_files.isEmpty()) {
        if (current_folder_path == lookup_folder) {
            return;
        }

        lookup_folder_files.clear();
    }
    lookup_folder = current_folder_path;

    QCollator collator;
    collator.setNumericMode(true);

    QDirIterator dirIterator(lookup_folder, QStringList() << "*.stl", QDir::Files | QDir::Readable | QDir::Hidden);
    while (dirIterator.hasNext()) {
        dirIterator.next();

        QString name = dirIterator.fileName();
        sorted_insert(lookup_folder_files, collator, name);
    }
}

QPair<QString, QString> Window::get_file_neighbors()
{
    if (current_file.isEmpty()) {
        return QPair<QString, QString>(QString(), QString());
    }

    build_folder_file_list();

    QFileInfo fileInfo(current_file);

    QString current_dir = fileInfo.absoluteDir().absolutePath();
    QString current_name = fileInfo.fileName();

    QString prev = QString();
    QString next = QString();

    QListIterator<QString> fileIterator(lookup_folder_files);
    while (fileIterator.hasNext()) {
        QString name = fileIterator.next();

        if (name == current_name) {
            if (fileIterator.hasNext()) {
                next = current_dir + QDir::separator() + fileIterator.next();
            }
            break;
        }

        prev = name;
    }

    if (!prev.isEmpty()) {
        prev.prepend(QDir::separator());
        prev.prepend(current_dir);
    }

    return QPair<QString, QString>(prev, next);
}

bool Window::load_prev(void)
{
    QPair<QString, QString> neighbors = get_file_neighbors();
    if (neighbors.first.isEmpty()) {
        return false;
    }

    return load_stl(neighbors.first);
}

bool Window::load_next(void)
{
    QPair<QString, QString> neighbors = get_file_neighbors();
    if (neighbors.second.isEmpty()) {
        return false;
    }

    return load_stl(neighbors.second);
}

void Window::keyPressEvent(QKeyEvent* event)
{
    if (!open_action->isEnabled()) {
        QMainWindow::keyPressEvent(event);
        return;
    }

    // File navigation and movement are QActions now (rebindable via
    // View > Configure Keyboard Shortcuts); only Esc stays hard-wired
    if (event->key() == Qt::Key_Escape) {
        hide_menuBar_action->setChecked(false);
        return;
    }

    QMainWindow::keyPressEvent(event);
}

void Window::on_fullscreen()
{
    if (!this->isFullScreen()) {
        this->showFullScreen();
    } else {
        this->showNormal();
    }
}
