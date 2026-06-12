#include "cliexport.h"
#include "exportdialog.h"
#include "gif.h"
#include "loader.h"
#include "mesh.h"
#include "offscreenrenderer.h"

#include <QCommandLineParser>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>

#include <algorithm>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryFile>

#include <cstdio>
#include <cstring>

bool cli_export_requested(int argc, char* argv[])
{
    for (int i = 1; i < argc; ++i) {
        // Export switches, and help/version requests, run without a GUI
        if (!strcmp(argv[i], "--export-png") || !strcmp(argv[i], "--export-gif") || !strcmp(argv[i], "--export-mp4") ||
            !strcmp(argv[i], "-h") ||
            !strcmp(argv[i], "--help") || !strcmp(argv[i], "--help-all") || !strcmp(argv[i], "-v") ||
            !strcmp(argv[i], "--version")) {
            return true;
        }
    }
    return false;
}

namespace
{

void err(const QString& message)
{
    fprintf(stderr, "fstl: %s\n", qPrintable(message));
}

void info(const QString& message)
{
    fprintf(stdout, "%s\n", qPrintable(message));
}

/*  Parses a color name ("red", "transparent", ...) or hex value
 *  ("#rgb", "#rrggbb", "#aarrggbb"). */
QColor parse_color(const QString& s, bool* ok)
{
    QColor c(s.trimmed());
    *ok = c.isValid();
    return c;
}

/*  Loads an STL synchronously using the same Loader as the GUI. */
Mesh* load_mesh_file(const QString& path, QString* error)
{
    Mesh* mesh = nullptr;
    Loader loader(nullptr, path, false);
    QObject::connect(&loader, &Loader::got_mesh, &loader, [&](Mesh* m, bool) { mesh = m; }, Qt::DirectConnection);
    QObject::connect(&loader, &Loader::error_bad_stl, &loader, [&] { *error = "invalid or corrupted .stl file"; },
                     Qt::DirectConnection);
    QObject::connect(&loader, &Loader::error_empty_mesh, &loader, [&] { *error = "file contains no triangles"; },
                     Qt::DirectConnection);
    QObject::connect(&loader, &Loader::error_missing_file, &loader, [&] { *error = "file not found"; }, Qt::DirectConnection);
    loader.run(); // synchronous: run() directly, not start()
    return mesh;
}

struct ExportJob {
    QString input;   // path, or "-" for stdin
    QString display; // name for messages and default output naming
};

QString output_path(const QString& outputName, const QString& outputDir, const QString& inputPath, const QString& displayName,
                    const QString& extension)
{
    QString dir = outputDir;
    if (dir.isEmpty()) {
        dir = (inputPath == "-") ? QDir::currentPath() : QFileInfo(inputPath).absolutePath();
    }
    QString base = outputName;
    if (base.isEmpty()) {
        base = QFileInfo(displayName).completeBaseName();
        if (base.isEmpty()) {
            base = "fstl";
        }
    }
    // Strip a known extension from --output so we can append the right one
    if (base.endsWith(".png", Qt::CaseInsensitive) || base.endsWith(".gif", Qt::CaseInsensitive) ||
        base.endsWith(".mp4", Qt::CaseInsensitive)) {
        base.chop(4);
    }
    return QDir(dir).filePath(base + extension);
}

} // namespace

int run_cli_export(const QStringList& args)
{
    QCommandLineParser parser;
    parser.setApplicationDescription(
        "fstl - fast .stl viewer and exporter\n"
        "\n"
        "GUI mode (default):   fstl [-f|--foreground] [file.stl]\n"
        "  Opens the viewer. -f/--foreground keeps it attached to the terminal\n"
        "  (it forks into the background by default).\n"
        "\n"
        "Export mode: with --export-png and/or --export-gif, fstl renders STL files\n"
        "to PNG images or animated GIFs without opening a window. Reads from files,\n"
        "directories, or stdin (pass '-').\n"
        "With multiple --angles, PNGs are numbered name_000_0deg.png, name_001_45deg.png, ...\n"
        "Exit code: 0 on success, 1 if any input failed, 2 for bad arguments.\n"
        "See fstl(1) for full documentation.");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("files", "Input .stl files ('-' reads stdin)", "[files...]");

    const QCommandLineOption optPng("export-png", "Export PNG image(s).");
    const QCommandLineOption optGif("export-gif", "Export an animated GIF of the model rotating.");
    const QCommandLineOption optMp4("export-mp4", "Export an MP4 video of the model rotating (requires ffmpeg).");
    const QCommandLineOption optInput(QStringList() << "i" << "input", "Input .stl file ('-' for stdin); may repeat.", "file");
    const QCommandLineOption optInputDir("input-dir", "Process every .stl file in this directory.", "dir");
    const QCommandLineOption optOutput(QStringList() << "o" << "output", "Output file name (single input only).", "name");
    const QCommandLineOption optOutputDir("output-dir", "Output directory (default: alongside the input).", "dir");
    const QCommandLineOption optColor("color", "Model color: a common color name or hex value (default white).", "color", "white");
    const QCommandLineOption optBg("bg", "Background color name or hex value (default transparent).", "color", "transparent");
    const QCommandLineOption optWidth("width", "Maximum output width in pixels.", "px");
    const QCommandLineOption optHeight("height", "Maximum output height in pixels.", "px");
    const QCommandLineOption optAxis("axis", "Rotation axis: x, y, or z (default y).", "axis", "y");
    const QCommandLineOption optAngles("angles", "PNG: comma-separated rotation angles in degrees (default 0).", "list", "0");
    const QCommandLineOption optSweep("sweep", "GIF loop mode: total rotation in degrees, up to 360 (default 360).", "deg", "360");
    const QCommandLineOption optBounce("bounce", "GIF: bounce between --from and --to instead of looping.");
    const QCommandLineOption optFrom("from", "GIF bounce mode: start angle in degrees (default -90).", "deg", "-90");
    const QCommandLineOption optTo("to", "GIF bounce mode: end angle in degrees (default 90).", "deg", "90");
    const QCommandLineOption optStep("step", "GIF: degrees per frame (default 3).", "deg", "3");
    const QCommandLineOption optFps("fps", "GIF/MP4: playback frames per second (default 25 / 30).", "fps");
    const QCommandLineOption optOpacity("opacity", "Model opacity 0..1 (default 1).", "value");
    const QCommandLineOption optBrightness("brightness", "Light brightness multiplier (default 1).", "value");
    const QCommandLineOption optLightDir("light-dir", "Light direction as x,y,z (default 0,-0.5,1).", "vec");
    const QCommandLineOption optSettings("settings", "Read defaults from an exported fstl settings .ini file.", "file");
    const QCommandLineOption optDuration("duration", "MP4: length in seconds (default 8).", "s", "8");
    const QCommandLineOption optRx("rx", "MP4: total X rotation in degrees, may exceed 360 (default 0).", "deg", "0");
    const QCommandLineOption optRy("ry", "MP4: total Y rotation in degrees (default 360).", "deg", "360");
    const QCommandLineOption optRz("rz", "MP4: total Z rotation in degrees (default 0).", "deg", "0");

    for (const auto& o : {optPng,    optGif,   optMp4,    optInput,      optInputDir, optOutput,   optOutputDir,
                          optColor,  optBg,    optWidth,  optHeight,     optAxis,     optAngles,   optSweep,
                          optBounce, optFrom,  optTo,     optStep,       optFps,      optOpacity,  optBrightness,
                          optLightDir, optSettings, optDuration, optRx,  optRy,       optRz}) {
        parser.addOption(o);
    }
    parser.process(args);

    const bool do_png = parser.isSet(optPng);
    const bool do_gif = parser.isSet(optGif);
    const bool do_mp4 = parser.isSet(optMp4);
    if (!do_png && !do_gif && !do_mp4) {
        err("nothing to do: pass --export-png, --export-gif, and/or --export-mp4");
        return 2;
    }

    // Defaults may come from an exported settings file; explicit
    // switches always override
    RenderStyle style;
    bool settings_bg = false;
    if (parser.isSet(optSettings)) {
        const QString sp = parser.value(optSettings);
        if (!QFileInfo::exists(sp)) {
            err("settings file not found: " + sp);
            return 2;
        }
        QSettings ini(sp, QSettings::IniFormat);
        const QColor amb = ini.value("ambientColor").value<QColor>();
        if (amb.isValid()) {
            style.modelColor = amb;
        }
        const QColor bg = ini.value("backgroundColor").value<QColor>();
        if (bg.isValid()) {
            style.background = bg;
            settings_bg = true;
        }
        style.brightness = ini.value("lightBrightness", 1.0).toDouble();
        style.opacity = ini.value("modelOpacity", 1.0).toDouble();
        // Reproduce the canvas's 26-direction table to resolve the index
        const int dirIndex = ini.value("currentLightDirection", -1).toInt();
        if (dirIndex >= 0) {
            QList<QVector3D> dirs;
            for (int i = -1; i < 2; i++)
                for (int j = -1; j < 2; j++)
                    for (int k = -1; k < 2; k++)
                        if (!(i == 0 && j == 0 && k == 0))
                            dirs << QVector3D(i, j, k);
            if (dirIndex < dirs.size()) {
                style.lightDirection = dirs.at(dirIndex);
            }
        }
    }

    // Colors and appearance (switches override settings-file defaults)
    bool ok = false;
    if (parser.isSet(optColor) || !parser.isSet(optSettings)) {
        style.modelColor = parse_color(parser.value(optColor), &ok);
        if (!ok) {
            err("unrecognized model color '" + parser.value(optColor) + "'");
            return 2;
        }
    }
    if (parser.isSet(optBg) || !settings_bg) {
        style.background = parse_color(parser.value(optBg), &ok);
        if (!ok) {
            err("unrecognized background color '" + parser.value(optBg) + "'");
            return 2;
        }
    }
    if (parser.isSet(optOpacity)) {
        style.opacity = std::clamp(parser.value(optOpacity).toDouble(&ok), 0.0, 1.0);
        if (!ok) {
            err("invalid --opacity");
            return 2;
        }
    }
    if (parser.isSet(optBrightness)) {
        style.brightness = parser.value(optBrightness).toDouble(&ok);
        if (!ok || style.brightness < 0) {
            err("invalid --brightness");
            return 2;
        }
    }
    if (parser.isSet(optLightDir)) {
        const QStringList parts = parser.value(optLightDir).split(',');
        if (parts.size() != 3) {
            err("--light-dir expects x,y,z");
            return 2;
        }
        style.lightDirection = QVector3D(parts[0].toFloat(), parts[1].toFloat(), parts[2].toFloat());
    }
    const QColor background = style.background;

    // Axis
    const QString axisName = parser.value(optAxis).toLower();
    QVector3D axis(0, 1, 0);
    if (axisName == "x") {
        axis = QVector3D(1, 0, 0);
    } else if (axisName == "y") {
        axis = QVector3D(0, 1, 0);
    } else if (axisName == "z") {
        axis = QVector3D(0, 0, 1);
    } else {
        err("invalid --axis '" + axisName + "' (use x, y, or z)");
        return 2;
    }

    // PNG angles
    std::vector<float> pngAngles;
    for (const QString& part : parser.value(optAngles).split(',', Qt::SkipEmptyParts)) {
        const float a = part.trimmed().toFloat(&ok);
        if (!ok) {
            err("invalid angle '" + part.trimmed() + "' in --angles");
            return 2;
        }
        pngAngles.push_back(a);
    }
    if (pngAngles.empty()) {
        pngAngles.push_back(0);
    }

    // GIF angle sequence (reuses the GUI's option struct)
    RotationExportOptions gifOpt;
    gifOpt.axis = axis;
    gifOpt.bounce = parser.isSet(optBounce);
    gifOpt.sweep = std::min(360.0f, parser.value(optSweep).toFloat());
    gifOpt.startAngle = parser.value(optFrom).toFloat();
    gifOpt.endAngle = parser.value(optTo).toFloat();
    gifOpt.step = parser.value(optStep).toFloat();
    gifOpt.fps = parser.isSet(optFps) ? std::max(1, parser.value(optFps).toInt()) : 25;
    const int mp4Fps = parser.isSet(optFps) ? std::max(1, parser.value(optFps).toInt()) : 30;
    const double mp4Duration = std::max(0.1, parser.value(optDuration).toDouble());
    const QVector3D mp4Degrees(parser.value(optRx).toFloat(), parser.value(optRy).toFloat(), parser.value(optRz).toFloat());

    // Resolution limits: defaults are 1024 for PNG and 640 for GIF
    const int userW = parser.isSet(optWidth) ? parser.value(optWidth).toInt() : 0;
    const int userH = parser.isSet(optHeight) ? parser.value(optHeight).toInt() : 0;
    if ((parser.isSet(optWidth) && userW < 1) || (parser.isSet(optHeight) && userH < 1)) {
        err("--width/--height must be positive");
        return 2;
    }
    const int pngMaxW = userW ? userW : (userH ? userH : 1024);
    const int pngMaxH = userH ? userH : (userW ? userW : 1024);
    const int gifMaxW = userW ? userW : (userH ? userH : 640);
    const int gifMaxH = userH ? userH : (userW ? userW : 640);
    const int mp4MaxW = userW ? userW : (userH ? userH : 960);
    const int mp4MaxH = userH ? userH : (userW ? userW : 960);

    // Collect inputs: positional args, --input, and --input-dir contents
    QStringList inputs = parser.positionalArguments();
    inputs += parser.values(optInput);
    if (parser.isSet(optInputDir)) {
        const QDir dir(parser.value(optInputDir));
        if (!dir.exists()) {
            err("input directory not found: " + parser.value(optInputDir));
            return 2;
        }
        QStringList found;
        QDirIterator it(dir.absolutePath(), QStringList() << "*.stl" << "*.STL", QDir::Files | QDir::Readable);
        while (it.hasNext()) {
            found << it.next();
        }
        found.sort();
        if (found.isEmpty()) {
            err("no .stl files in " + dir.absolutePath());
            return 2;
        }
        inputs += found;
    }
    if (inputs.isEmpty()) {
        err("no input file: pass a .stl path, --input, --input-dir, or '-' for stdin");
        return 2;
    }
    if (!parser.value(optOutput).isEmpty() && inputs.size() > 1) {
        err("--output is only valid with a single input; use --output-dir for batches");
        return 2;
    }

    // Stdin support: spool the piped data to a temporary file for the Loader
    QTemporaryFile stdinFile;
    QList<ExportJob> jobs;
    for (const QString& in : inputs) {
        ExportJob job;
        if (in == "-") {
            QFile std_in;
            if (!std_in.open(stdin, QIODevice::ReadOnly)) {
                err("could not read stdin");
                return 1;
            }
            const QByteArray data = std_in.readAll();
            if (data.isEmpty()) {
                err("stdin was empty");
                return 1;
            }
            if (!stdinFile.open()) {
                err("could not create a temporary file for stdin");
                return 1;
            }
            stdinFile.write(data);
            stdinFile.flush();
            job.input = "-";
            job.display = "stdin";
        } else {
            job.input = in;
            job.display = QFileInfo(in).fileName();
        }
        jobs << job;
    }

    int failures = 0;
    for (const ExportJob& job : jobs) {
        const QString load_path = (job.input == "-") ? stdinFile.fileName() : job.input;

        QString load_error;
        Mesh* mesh = load_mesh_file(load_path, &load_error);
        if (!mesh) {
            err(job.display + ": " + (load_error.isEmpty() ? "could not load" : load_error));
            ++failures;
            continue;
        }

        OffscreenRenderer renderer(mesh, style);
        delete mesh;
        if (!renderer.isValid()) {
            err(job.display + ": " + renderer.errorString());
            ++failures;
            continue;
        }

        if (do_png) {
            const QSize size = renderer.layoutForAngles(pngAngles, axis, pngMaxW, pngMaxH);
            const QString path = output_path(parser.value(optOutput), parser.value(optOutputDir), job.input, job.display, ".png");
            bool all_ok = true;
            for (size_t i = 0; i < pngAngles.size(); ++i) {
                QString out = path;
                if (pngAngles.size() > 1) {
                    out.chop(4);
                    out += QString("_%1_%2deg.png").arg(i, 3, 10, QChar('0')).arg(qRound(pngAngles[i]));
                }
                const QImage frame = renderer.renderFrame(pngAngles[i], axis, size);
                if (frame.isNull() || !frame.save(out)) {
                    err(job.display + ": could not save " + out);
                    all_ok = false;
                } else {
                    info("wrote " + out + QString(" (%1x%2)").arg(frame.width()).arg(frame.height()));
                }
            }
            failures += all_ok ? 0 : 1;
        }

        if (do_gif) {
            const auto angles = gifOpt.gifAngles();
            const QSize size = renderer.layoutForAngles(angles, axis, gifMaxW, gifMaxH);
            const QString path = output_path(parser.value(optOutput), parser.value(optOutputDir), job.input, job.display, ".gif");
            const uint32_t delay_cs = std::max(1, 100 / gifOpt.fps);
            const bool transparent = background.alpha() < 255;

            GifWriter writer;
            if (!GifBegin(&writer, path.toLocal8Bit().constData(), size.width(), size.height(), delay_cs, 8, false, transparent)) {
                err(job.display + ": could not open " + path + " for writing");
                ++failures;
                continue;
            }
            bool all_ok = true;
            for (const float angle : angles) {
                const QImage frame = renderer.renderFrame(angle, axis, size);
                if (frame.isNull()) {
                    all_ok = false;
                    break;
                }
                GifWriteFrame(&writer, frame.constBits(), size.width(), size.height(), delay_cs);
            }
            GifEnd(&writer);
            if (all_ok) {
                info("wrote " + path +
                     QString(" (%1x%2, %3 frames)").arg(size.width()).arg(size.height()).arg(int(angles.size())));
            } else {
                err(job.display + ": rendering failed");
                QFile::remove(path);
                ++failures;
            }
        }

        if (do_mp4) {
            const QString ffmpeg_path = QStandardPaths::findExecutable("ffmpeg");
            if (ffmpeg_path.isEmpty()) {
                err("MP4 export requires ffmpeg on the PATH");
                ++failures;
                continue;
            }

            const int total = std::max(1, int(std::lround(mp4Duration * mp4Fps)));
            std::vector<QVector3D> triples;
            triples.reserve(total);
            for (int i = 0; i < total; ++i) {
                const double f = (double(i) / mp4Fps) / mp4Duration;
                triples.push_back(mp4Degrees * float(f));
            }

            QSize size = renderer.layoutForTriples(triples, mp4MaxW, mp4MaxH);
            size = QSize(size.width() & ~1, size.height() & ~1); // libx264: even dims
            if (size.width() < 2 || size.height() < 2) {
                err(job.display + ": output size too small");
                ++failures;
                continue;
            }
            const QString path = output_path(parser.value(optOutput), parser.value(optOutputDir), job.input, job.display, ".mp4");

            QProcess proc;
            proc.setProcessChannelMode(QProcess::ForwardedErrorChannel);
            proc.start(ffmpeg_path, {"-y", "-loglevel", "error", "-f", "rawvideo", "-pix_fmt", "rgba", "-s",
                                     QString("%1x%2").arg(size.width()).arg(size.height()), "-r", QString::number(mp4Fps),
                                     "-i", "-", "-c:v", "libx264", "-preset", "veryfast", "-pix_fmt", "yuv420p", "-movflags",
                                     "+faststart", path});
            if (!proc.waitForStarted(5000)) {
                err(job.display + ": could not start ffmpeg");
                ++failures;
                continue;
            }

            bool all_ok = true;
            for (const QVector3D& a : triples) {
                QImage frame = renderer.renderFrameTriple(a, size);
                if (frame.isNull()) {
                    all_ok = false;
                    break;
                }
                if (frame.size() != size) {
                    frame = frame.scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                                .convertToFormat(QImage::Format_RGBA8888);
                }
                proc.write((const char*)frame.constBits(), qint64(size.width()) * size.height() * 4);
                while (proc.bytesToWrite() > 64 * 1024 * 1024) {
                    proc.waitForBytesWritten(100);
                }
            }
            proc.closeWriteChannel();
            proc.waitForFinished(-1);
            if (all_ok && proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0) {
                info("wrote " + path +
                     QString(" (%1x%2, %3s @ %4fps)").arg(size.width()).arg(size.height()).arg(mp4Duration).arg(mp4Fps));
            } else {
                err(job.display + ": MP4 encoding failed");
                QFile::remove(path);
                ++failures;
            }
        }
    }

    return failures ? 1 : 0;
}
