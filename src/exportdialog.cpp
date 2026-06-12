#include "exportdialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QSettings>
#include <QSpinBox>

#include <cmath>

namespace
{
QComboBox* makeAxisCombo()
{
    auto combo = new QComboBox;
    combo->addItem("Y (vertical / turntable)", QVector3D(0, 1, 0));
    combo->addItem("X (horizontal / tumble)", QVector3D(1, 0, 0));
    combo->addItem("Z (screen normal / roll)", QVector3D(0, 0, 1));
    return combo;
}
} // namespace

std::vector<float> RotationExportOptions::pngAngles() const
{
    std::vector<float> angles;
    if (frames < 1) {
        return angles;
    }
    // A full circle divides evenly (the endpoint duplicates frame 0);
    // partial sweeps include both endpoints.
    const float divisor = (sweep >= 360.0f || frames == 1) ? frames : (frames - 1);
    for (int i = 0; i < frames; ++i) {
        angles.push_back(sweep * i / divisor);
    }
    return angles;
}

std::vector<float> RotationExportOptions::gifAngles() const
{
    std::vector<float> angles;
    const float s = std::max(0.1f, step);
    if (bounce) {
        const float lo = std::min(startAngle, endAngle);
        const float hi = std::max(startAngle, endAngle);
        // Forward leg, including both endpoints...
        for (float a = lo; a < hi; a += s) {
            angles.push_back(a);
        }
        angles.push_back(hi);
        // ...then back, excluding the endpoints so the loop is seamless
        for (float a = hi - s; a > lo; a -= s) {
            angles.push_back(a);
        }
    } else {
        for (float a = 0; a < sweep; a += s) {
            angles.push_back(a);
        }
    }
    return angles;
}

PngExportDialog::PngExportDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle("Export Rotation PNGs");

    auto layout = new QFormLayout(this);

    axisCombo = makeAxisCombo();
    layout->addRow("Rotation axis", axisCombo);

    framesSpin = new QSpinBox;
    framesSpin->setRange(1, 360);
    layout->addRow("Number of images", framesSpin);

    sweepSpin = new QDoubleSpinBox;
    sweepSpin->setRange(1, 360);
    sweepSpin->setSuffix(QString::fromUtf8("°"));
    layout->addRow("Total sweep", sweepSpin);

    // Restore the last-used values
    QSettings settings;
    axisCombo->setCurrentIndex(settings.value("exportPng/axis", 0).toInt());
    framesSpin->setValue(settings.value("exportPng/frames", 8).toInt());
    sweepSpin->setValue(settings.value("exportPng/sweep", 360).toDouble());

    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addRow(buttons);
}

void PngExportDialog::accept()
{
    QSettings settings;
    settings.setValue("exportPng/axis", axisCombo->currentIndex());
    settings.setValue("exportPng/frames", framesSpin->value());
    settings.setValue("exportPng/sweep", sweepSpin->value());
    QDialog::accept();
}

RotationExportOptions PngExportDialog::options() const
{
    RotationExportOptions opt;
    opt.axis = axisCombo->currentData().value<QVector3D>();
    opt.frames = framesSpin->value();
    opt.sweep = sweepSpin->value();
    return opt;
}

Mp4ExportDialog::Mp4ExportDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle("Export Rotation MP4");

    auto layout = new QFormLayout(this);

    auto makeDegSpin = [] {
        auto spin = new QDoubleSpinBox;
        spin->setRange(-3600, 3600);
        spin->setSuffix(QString::fromUtf8("°"));
        return spin;
    };

    durationSpin = new QDoubleSpinBox;
    durationSpin->setRange(0.5, 300);
    durationSpin->setSuffix(" s");
    layout->addRow("Duration", durationSpin);

    degXSpin = makeDegSpin();
    layout->addRow("X rotation (total)", degXSpin);
    degYSpin = makeDegSpin();
    layout->addRow("Y rotation (total)", degYSpin);
    degZSpin = makeDegSpin();
    layout->addRow("Z rotation (total)", degZSpin);

    fpsSpin = new QSpinBox;
    fpsSpin->setRange(10, 60);
    layout->addRow("Frames per second", fpsSpin);

    widthSpin = new QSpinBox;
    widthSpin->setRange(64, 4096);
    widthSpin->setSuffix(" px");
    layout->addRow("Video width", widthSpin);

    // Restore the last-used values
    QSettings settings;
    durationSpin->setValue(settings.value("exportMp4/duration", 8).toDouble());
    degXSpin->setValue(settings.value("exportMp4/degX", 0).toDouble());
    degYSpin->setValue(settings.value("exportMp4/degY", 360).toDouble());
    degZSpin->setValue(settings.value("exportMp4/degZ", 0).toDouble());
    fpsSpin->setValue(settings.value("exportMp4/fps", 30).toInt());
    widthSpin->setValue(settings.value("exportMp4/width", 960).toInt());

    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addRow(buttons);
}

void Mp4ExportDialog::accept()
{
    QSettings settings;
    settings.setValue("exportMp4/duration", durationSpin->value());
    settings.setValue("exportMp4/degX", degXSpin->value());
    settings.setValue("exportMp4/degY", degYSpin->value());
    settings.setValue("exportMp4/degZ", degZSpin->value());
    settings.setValue("exportMp4/fps", fpsSpin->value());
    settings.setValue("exportMp4/width", widthSpin->value());
    QDialog::accept();
}

Mp4ExportOptions Mp4ExportDialog::options() const
{
    Mp4ExportOptions opt;
    opt.duration = durationSpin->value();
    opt.degreesX = degXSpin->value();
    opt.degreesY = degYSpin->value();
    opt.degreesZ = degZSpin->value();
    opt.fps = fpsSpin->value();
    opt.width = widthSpin->value();
    return opt;
}

GifExportDialog::GifExportDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle("Export Rotating GIF");

    auto layout = new QFormLayout(this);

    axisCombo = makeAxisCombo();
    layout->addRow("Rotation axis", axisCombo);

    modeCombo = new QComboBox;
    modeCombo->addItem("Loop (continuous rotation)");
    modeCombo->addItem("Bounce (back and forth between angles)");
    layout->addRow("Mode", modeCombo);
    connect(modeCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &GifExportDialog::modeChanged);

    sweepSpin = new QDoubleSpinBox;
    sweepSpin->setRange(1, 360);
    sweepSpin->setSuffix(QString::fromUtf8("°"));
    layout->addRow("Sweep (loop mode)", sweepSpin);

    startSpin = new QDoubleSpinBox;
    startSpin->setRange(-360, 360);
    startSpin->setSuffix(QString::fromUtf8("°"));
    layout->addRow("From angle (bounce)", startSpin);

    endSpin = new QDoubleSpinBox;
    endSpin->setRange(-360, 360);
    endSpin->setSuffix(QString::fromUtf8("°"));
    layout->addRow("To angle (bounce)", endSpin);

    stepSpin = new QDoubleSpinBox;
    stepSpin->setRange(0.5, 90);
    stepSpin->setSuffix(QString::fromUtf8("°"));
    layout->addRow("Degrees per frame", stepSpin);

    fpsSpin = new QSpinBox;
    fpsSpin->setRange(1, 50);
    layout->addRow("Frames per second", fpsSpin);

    widthSpin = new QSpinBox;
    widthSpin->setRange(64, 4096);
    widthSpin->setSuffix(" px");
    layout->addRow("GIF width", widthSpin);

    // Restore the last-used values
    QSettings settings;
    axisCombo->setCurrentIndex(settings.value("exportGif/axis", 0).toInt());
    modeCombo->setCurrentIndex(settings.value("exportGif/mode", 0).toInt());
    sweepSpin->setValue(settings.value("exportGif/sweep", 360).toDouble());
    startSpin->setValue(settings.value("exportGif/from", -90).toDouble());
    endSpin->setValue(settings.value("exportGif/to", 90).toDouble());
    stepSpin->setValue(settings.value("exportGif/step", 3).toDouble());
    fpsSpin->setValue(settings.value("exportGif/fps", 25).toInt());
    widthSpin->setValue(settings.value("exportGif/width", 480).toInt());

    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addRow(buttons);

    modeChanged(modeCombo->currentIndex());
}

void GifExportDialog::accept()
{
    QSettings settings;
    settings.setValue("exportGif/axis", axisCombo->currentIndex());
    settings.setValue("exportGif/mode", modeCombo->currentIndex());
    settings.setValue("exportGif/sweep", sweepSpin->value());
    settings.setValue("exportGif/from", startSpin->value());
    settings.setValue("exportGif/to", endSpin->value());
    settings.setValue("exportGif/step", stepSpin->value());
    settings.setValue("exportGif/fps", fpsSpin->value());
    settings.setValue("exportGif/width", widthSpin->value());
    QDialog::accept();
}

void GifExportDialog::modeChanged(int index)
{
    const bool bounce = (index == 1);
    sweepSpin->setEnabled(!bounce);
    startSpin->setEnabled(bounce);
    endSpin->setEnabled(bounce);
}

RotationExportOptions GifExportDialog::options() const
{
    RotationExportOptions opt;
    opt.axis = axisCombo->currentData().value<QVector3D>();
    opt.bounce = (modeCombo->currentIndex() == 1);
    opt.sweep = sweepSpin->value();
    opt.startAngle = startSpin->value();
    opt.endAngle = endSpin->value();
    opt.step = stepSpin->value();
    opt.fps = fpsSpin->value();
    opt.gifWidth = widthSpin->value();
    return opt;
}
