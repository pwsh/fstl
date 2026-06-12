#include "animatedialog.h"
#include "canvas.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>
#include <QQuaternion>
#include <QPushButton>
#include <QRandomGenerator>
#include <QSettings>
#include <QStandardPaths>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace
{
const int RECORD_FPS = 30;

// Built-in defaults (also used by the Defaults button)
const double DEF_SPEED_X = 0, DEF_SPEED_Y = 20, DEF_SPEED_Z = 0;
const double DEF_INTERVAL_MIN = 2, DEF_INTERVAL_MAX = 5;
const double DEF_SPEED_MIN = -60, DEF_SPEED_MAX = 60;
const double DEF_COLOR_DUR_MIN = 3, DEF_COLOR_DUR_MAX = 8;
const double DEF_LIGHT_SPEED = 0.1;

QDoubleSpinBox* makeSpeedSpin(double value)
{
    auto spin = new QDoubleSpinBox;
    spin->setRange(-360, 360);
    spin->setDecimals(1);
    spin->setSingleStep(5);
    spin->setSuffix(QString::fromUtf8(" °/s"));
    spin->setValue(value);
    return spin;
}

QDoubleSpinBox* makeSecondsSpin(double value)
{
    auto spin = new QDoubleSpinBox;
    spin->setRange(0.5, 120);
    spin->setSuffix(" s");
    spin->setValue(value);
    return spin;
}

/*  Formats a speed for a file name: "20", "-12.5", ... */
QString speed_tag(double v)
{
    QString s = QString::number(v, 'f', 1);
    if (s.endsWith(".0")) {
        s.chop(2);
    }
    return s;
}

QColor lerp_color(const QColor& a, const QColor& b, double k)
{
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * k, a.greenF() + (b.greenF() - a.greenF()) * k,
                            a.blueF() + (b.blueF() - a.blueF()) * k);
}

double smoothstep(double f)
{
    f = std::clamp(f, 0.0, 1.0);
    return f * f * (3 - 2 * f);
}

QString format_position(const QVector3D& v)
{
    return QString("x %1   y %2   z %3").arg(v.x(), 0, 'f', 2).arg(v.y(), 0, 'f', 2).arg(v.z(), 0, 'f', 2);
}

/*  A titled section that expands/collapses its content on click. */
QWidget* makeSection(const QString& title, QWidget* content, bool expanded)
{
    auto box = new QWidget;
    auto layout = new QVBoxLayout(box);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    auto header = new QToolButton;
    header->setText(title);
    header->setCheckable(true);
    header->setChecked(expanded);
    header->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    header->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
    header->setStyleSheet("QToolButton { border: none; font-weight: bold; }");
    content->setVisible(expanded);
    QObject::connect(header, &QToolButton::toggled, content, [header, content](bool on) {
        content->setVisible(on);
        header->setArrowType(on ? Qt::DownArrow : Qt::RightArrow);
    });

    layout->addWidget(header);
    layout->addWidget(content);
    return box;
}
} // namespace

RotationAnimationDialog::RotationAnimationDialog(QWidget* parent, Canvas* _canvas) : QDialog(parent), canvas(_canvas)
{
    setWindowTitle("Animate Rotation");

    QSettings settings;
    auto layout = new QVBoxLayout(this);
    // Shrink the dialog when sections collapse instead of spreading the
    // remaining sections over the old height
    layout->setSizeConstraint(QLayout::SetFixedSize);

    // --- Rotation section ---
    auto rotContent = new QWidget;
    auto rotLayout = new QVBoxLayout(rotContent);
    rotLayout->setContentsMargins(16, 2, 2, 2);
    auto form = new QFormLayout;
    rotationEnabled = new QCheckBox("Animate rotation");
    rotationEnabled->setToolTip("Off: the view is fully free while colors/light animate.\n"
                                "On: axes with a non-zero or random speed are animation-driven;\n"
                                "axes at 0 stay free for mouse movement.");
    rotationEnabled->setChecked(settings.value("animate/rotationEnabled", true).toBool());
    form->addRow(rotationEnabled);

    xSpeed = makeSpeedSpin(settings.value("animate/xSpeed", DEF_SPEED_X).toDouble());
    ySpeed = makeSpeedSpin(settings.value("animate/ySpeed", DEF_SPEED_Y).toDouble());
    zSpeed = makeSpeedSpin(settings.value("animate/zSpeed", DEF_SPEED_Z).toDouble());
    form->addRow("X axis speed", xSpeed);
    form->addRow("Y axis speed", ySpeed);
    form->addRow("Z axis speed", zSpeed);
    rotLayout->addLayout(form);

    // Per-axis random movement, each in its own collapsible sub-section
    const char* axisTitles[3] = {"X axis random movement", "Y axis random movement", "Z axis random movement"};
    const char* axisIds[3] = {"X", "Y", "Z"};
    for (int i = 0; i < 3; ++i) {
        auto axisContent = new QWidget;
        auto axisForm = new QFormLayout(axisContent);
        axisForm->setContentsMargins(16, 2, 2, 2);

        axisRandom[i] = new QCheckBox("Randomly change this axis");
        axisRandom[i]->setChecked(settings.value(QString("animate/random%1").arg(axisIds[i]), false).toBool());
        axisForm->addRow(axisRandom[i]);

        axisSpeedMin[i] = makeSpeedSpin(settings.value(QString("animate/speedMin%1").arg(axisIds[i]), DEF_SPEED_MIN).toDouble());
        axisSpeedMax[i] = makeSpeedSpin(settings.value(QString("animate/speedMax%1").arg(axisIds[i]), DEF_SPEED_MAX).toDouble());
        auto speedRow = new QHBoxLayout;
        speedRow->addWidget(axisSpeedMin[i]);
        speedRow->addWidget(new QLabel("to"));
        speedRow->addWidget(axisSpeedMax[i]);
        axisForm->addRow("Speed range", speedRow);

        axisIntMin[i] =
            makeSecondsSpin(settings.value(QString("animate/intervalMin%1").arg(axisIds[i]), DEF_INTERVAL_MIN).toDouble());
        axisIntMax[i] =
            makeSecondsSpin(settings.value(QString("animate/intervalMax%1").arg(axisIds[i]), DEF_INTERVAL_MAX).toDouble());
        auto intRow = new QHBoxLayout;
        intRow->addWidget(axisIntMin[i]);
        intRow->addWidget(new QLabel("to"));
        intRow->addWidget(axisIntMax[i]);
        axisForm->addRow("Change every", intRow);

        rotLayout->addWidget(makeSection(axisTitles[i], axisContent, false));
    }
    layout->addWidget(makeSection("Rotation", rotContent, true));

    // --- Color sections ---
    modelChannel.settingsId = "model";
    lightChannel.settingsId = "light";
    bgChannel.settingsId = "bg";
    layout->addWidget(buildColorSection(modelChannel, "Model color"));
    layout->addWidget(buildColorSection(lightChannel, "Light color"));
    layout->addWidget(buildColorSection(bgChannel, "Background color"));

    // --- Model transparency section ---
    auto opContent = new QWidget;
    auto opForm = new QFormLayout(opContent);
    opForm->setContentsMargins(16, 2, 2, 2);
    opacityEnabled = new QCheckBox("Randomly change model transparency");
    opacityEnabled->setChecked(settings.value("animate/opacityRandom", false).toBool());
    opForm->addRow(opacityEnabled);
    opacityMin = new QDoubleSpinBox;
    opacityMin->setRange(0.0, 1.0);
    opacityMin->setSingleStep(0.05);
    opacityMin->setDecimals(2);
    opacityMin->setValue(settings.value("animate/opacityMin", 0.3).toDouble());
    opacityMax = new QDoubleSpinBox;
    opacityMax->setRange(0.0, 1.0);
    opacityMax->setSingleStep(0.05);
    opacityMax->setDecimals(2);
    opacityMax->setValue(settings.value("animate/opacityMax", 1.0).toDouble());
    auto opRangeRow = new QHBoxLayout;
    opRangeRow->addWidget(opacityMin);
    opRangeRow->addWidget(new QLabel("to"));
    opRangeRow->addWidget(opacityMax);
    opForm->addRow("Opacity range", opRangeRow);
    opacityDurMin = makeSecondsSpin(settings.value("animate/opacityDurMin", DEF_COLOR_DUR_MIN).toDouble());
    opacityDurMax = makeSecondsSpin(settings.value("animate/opacityDurMax", DEF_COLOR_DUR_MAX).toDouble());
    auto opDurRow = new QHBoxLayout;
    opDurRow->addWidget(opacityDurMin);
    opDurRow->addWidget(new QLabel("to"));
    opDurRow->addWidget(opacityDurMax);
    opForm->addRow("Transition time", opDurRow);
    layout->addWidget(makeSection("Model transparency", opContent, false));

    // --- Light source section ---
    auto lightContent = new QWidget;
    auto lightForm = new QFormLayout(lightContent);
    lightForm->setContentsMargins(16, 2, 2, 2);
    lightDirCombo = new QComboBox;
    lightDirCombo->addItems(canvas->getNameDir());
    lightDirCombo->setCurrentIndex(canvas->getCurrentLightDirection());
    connect(lightDirCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int ind) {
        canvas->setCurrentLightDirection(ind);
        canvas->update();
        lightPosLabel->setText(format_position(canvas->getLightDirectionVector(ind)));
    });
    lightForm->addRow("Direction", lightDirCombo);
    lightMoveBox = new QCheckBox("Randomly move the light (smooth)");
    lightMoveBox->setChecked(settings.value("animate/randomLightMove", false).toBool());
    lightForm->addRow(lightMoveBox);

    lightSpeed = new QDoubleSpinBox;
    lightSpeed->setRange(0.05, 5.0);
    lightSpeed->setSingleStep(0.05);
    lightSpeed->setSuffix(" /s");
    lightSpeed->setToolTip("How far the light travels per second (direction units)");
    lightSpeed->setValue(settings.value("animate/lightSpeed", DEF_LIGHT_SPEED).toDouble());
    lightForm->addRow("Movement speed", lightSpeed);

    lightBrightness = new QDoubleSpinBox;
    lightBrightness->setRange(0.0, 3.0);
    lightBrightness->setSingleStep(0.1);
    lightBrightness->setDecimals(2);
    lightBrightness->setValue(canvas->getLightBrightness());
    connect(lightBrightness, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [this](double b) { canvas->setLightBrightness(b); });
    lightForm->addRow("Brightness", lightBrightness);

    const char* axisNames[3] = {"X range", "Y range", "Z range"};
    const QString minKeys[3] = {"animate/lightMinX", "animate/lightMinY", "animate/lightMinZ"};
    const QString maxKeys[3] = {"animate/lightMaxX", "animate/lightMaxY", "animate/lightMaxZ"};
    // Z minimum defaults to 0 so the light stays in front of the object
    const double minDefaults[3] = {-1.0, -1.0, 0.0};
    for (int i = 0; i < 3; ++i) {
        lightMin[i] = new QDoubleSpinBox;
        lightMin[i]->setRange(-1, 1);
        lightMin[i]->setSingleStep(0.1);
        lightMin[i]->setValue(settings.value(minKeys[i], minDefaults[i]).toDouble());
        lightMax[i] = new QDoubleSpinBox;
        lightMax[i]->setRange(-1, 1);
        lightMax[i]->setSingleStep(0.1);
        lightMax[i]->setValue(settings.value(maxKeys[i], 1.0).toDouble());
        auto row = new QHBoxLayout;
        row->addWidget(lightMin[i]);
        row->addWidget(new QLabel("to"));
        row->addWidget(lightMax[i]);
        lightForm->addRow(axisNames[i], row);
    }
    lightPosLabel = new QLabel("-");
    lightForm->addRow("Position", lightPosLabel);
    layout->addWidget(makeSection("Light source", lightContent, false));

    // --- Buttons ---
    auto buttons = new QHBoxLayout;
    playButton = new QPushButton("▶ Play");
    recordButton = new QPushButton("● Record");
    stopButton = new QPushButton("■ Stop");
    auto defaultsButton = new QPushButton("Defaults");
    defaultsButton->setToolTip("Reset every animation setting to its default");
    buttons->addWidget(playButton);
    buttons->addWidget(recordButton);
    buttons->addWidget(stopButton);
    buttons->addStretch();
    buttons->addWidget(defaultsButton);
    layout->addLayout(buttons);

    statusLabel = new QLabel(" ");
    layout->addWidget(statusLabel);

    connect(playButton, &QPushButton::clicked, this, &RotationAnimationDialog::play);
    connect(recordButton, &QPushButton::clicked, this, &RotationAnimationDialog::record);
    connect(stopButton, &QPushButton::clicked, this, &RotationAnimationDialog::stop);
    connect(defaultsButton, &QPushButton::clicked, this, [this] {
        stop();
        xSpeed->setValue(DEF_SPEED_X);
        ySpeed->setValue(DEF_SPEED_Y);
        zSpeed->setValue(DEF_SPEED_Z);
        rotationEnabled->setChecked(true);
        for (int i = 0; i < 3; ++i) {
            axisRandom[i]->setChecked(false);
            axisSpeedMin[i]->setValue(DEF_SPEED_MIN);
            axisSpeedMax[i]->setValue(DEF_SPEED_MAX);
            axisIntMin[i]->setValue(DEF_INTERVAL_MIN);
            axisIntMax[i]->setValue(DEF_INTERVAL_MAX);
        }
        for (ColorChannel* ch : {&modelChannel, &lightChannel, &bgChannel}) {
            ch->enabled->setChecked(false);
            ch->palette.clear();
            ch->minDur->setValue(DEF_COLOR_DUR_MIN);
            ch->maxDur->setValue(DEF_COLOR_DUR_MAX);
            rebuildPaletteRow(*ch);
        }
        opacityEnabled->setChecked(false);
        opacityMin->setValue(0.3);
        opacityMax->setValue(1.0);
        opacityDurMin->setValue(DEF_COLOR_DUR_MIN);
        opacityDurMax->setValue(DEF_COLOR_DUR_MAX);
        lightMoveBox->setChecked(false);
        lightSpeed->setValue(DEF_LIGHT_SPEED);
        canvas->resetLightBrightness();
        lightBrightness->setValue(canvas->getLightBrightness());
        for (int i = 0; i < 3; ++i) {
            lightMin[i]->setValue(i == 2 ? 0 : -1); // Z stays in front
            lightMax[i]->setValue(1);
        }
    });

    anim_timer.setInterval(16);
    connect(&anim_timer, &QTimer::timeout, this, &RotationAnimationDialog::tick);
    connect(&frame_timer, &QTimer::timeout, this, [this] {
        if (!ffmpeg) {
            return;
        }
        QImage frame = canvas->grabAnimationFrame(ax, ay, az, base);
        // libx264 requires even dimensions
        frame = frame.copy(0, 0, frame.width() & ~1, frame.height() & ~1).convertToFormat(QImage::Format_RGBA8888);
        if (frame.width() == record_w && frame.height() == record_h) {
            ffmpeg->write((const char*)frame.constBits(), qint64(record_w) * record_h * 4);
        }
    });
    frame_timer.setInterval(1000 / RECORD_FPS);

    updateButtons();
}

QWidget* RotationAnimationDialog::buildColorSection(ColorChannel& ch, const QString& title)
{
    QSettings settings;
    auto content = new QWidget;
    auto form = new QFormLayout(content);
    form->setContentsMargins(16, 2, 2, 2);

    ch.enabled = new QCheckBox("Randomly change " + title.toLower());
    ch.enabled->setChecked(settings.value("animate/" + ch.settingsId + "Random", false).toBool());
    form->addRow(ch.enabled);

    for (const QString& name : settings.value("animate/" + ch.settingsId + "Palette").toStringList()) {
        const QColor c(name);
        if (c.isValid()) {
            ch.palette << c;
        }
    }
    ch.paletteRow = new QHBoxLayout;
    form->addRow(ch.paletteRow);
    rebuildPaletteRow(ch);

    ch.minDur = makeSecondsSpin(settings.value("animate/" + ch.settingsId + "MinDur", DEF_COLOR_DUR_MIN).toDouble());
    ch.maxDur = makeSecondsSpin(settings.value("animate/" + ch.settingsId + "MaxDur", DEF_COLOR_DUR_MAX).toDouble());
    auto durRow = new QHBoxLayout;
    durRow->addWidget(ch.minDur);
    durRow->addWidget(new QLabel("to"));
    durRow->addWidget(ch.maxDur);
    form->addRow("Transition time", durRow);

    return makeSection(title, content, false);
}

void RotationAnimationDialog::rebuildPaletteRow(ColorChannel& ch)
{
    // deleteLater: this runs from the clicked() handler of one of these
    // buttons, and deleting a sender inside its own signal is unsafe
    while (auto item = ch.paletteRow->takeAt(0)) {
        if (auto w = item->widget()) {
            w->hide();
            w->deleteLater();
        }
        delete item;
    }

    ch.paletteRow->addWidget(new QLabel("Palette"));
    for (int i = 0; i < ch.palette.size(); ++i) {
        QPixmap swatch(18, 18);
        swatch.fill(ch.palette[i]);
        auto btn = new QPushButton;
        btn->setIcon(QIcon(swatch));
        btn->setFixedSize(26, 26);
        btn->setToolTip("Click to remove this color");
        connect(btn, &QPushButton::clicked, this, [this, &ch, i] {
            ch.palette.removeAt(i);
            rebuildPaletteRow(ch);
        });
        ch.paletteRow->addWidget(btn);
    }
    if (ch.palette.isEmpty()) {
        ch.paletteRow->addWidget(new QLabel("(empty = any color)"));
    }
    auto addBtn = new QPushButton("Add...");
    connect(addBtn, &QPushButton::clicked, this, [this, &ch] {
        const QColor c = QColorDialog::getColor(Qt::white, this, "Add palette color", QColorDialog::DontUseNativeDialog);
        if (c.isValid()) {
            ch.palette << c;
            rebuildPaletteRow(ch);
        }
    });
    ch.paletteRow->addWidget(addBtn);
    ch.paletteRow->addStretch();
}

QColor RotationAnimationDialog::pickTargetColor(const ColorChannel& ch, const QColor& current) const
{
    auto rng = QRandomGenerator::global();
    if (!ch.palette.isEmpty()) {
        // Locked to the palette; avoid a no-op transition when possible
        if (ch.palette.size() == 1) {
            return ch.palette.first();
        }
        QColor c = ch.palette.at(rng->bounded(ch.palette.size()));
        for (int attempt = 0; attempt < 8 && c == current; ++attempt) {
            c = ch.palette.at(rng->bounded(ch.palette.size()));
        }
        return c;
    }
    // Fully random: pleasant saturated hue
    return QColor::fromHsvF(rng->generateDouble(), 0.6 + 0.4 * rng->generateDouble(), 0.8 + 0.2 * rng->generateDouble());
}

void RotationAnimationDialog::startChannel(ColorChannel& ch, const QColor& fallback)
{
    // With a palette the starting color snaps to the palette, so the
    // animation never shows colors outside it
    ch.current = ch.palette.isEmpty() ? fallback : ch.palette.at(QRandomGenerator::global()->bounded(ch.palette.size()));
    ch.start = ch.current;
    nextChannelTarget(ch);
    ch.active = true;
}

void RotationAnimationDialog::nextChannelTarget(ColorChannel& ch)
{
    ch.start = ch.current;
    ch.target = pickTargetColor(ch, ch.current);
    const double lo = std::min(ch.minDur->value(), ch.maxDur->value());
    const double hi = std::max(ch.minDur->value(), ch.maxDur->value());
    ch.duration = lo + QRandomGenerator::global()->generateDouble() * (hi - lo);
    ch.elapsed = 0;
}

void RotationAnimationDialog::tickChannel(ColorChannel& ch, double dt)
{
    ch.elapsed += dt;
    const double f = ch.duration > 0 ? ch.elapsed / ch.duration : 1.0;
    ch.current = lerp_color(ch.start, ch.target, smoothstep(f));
    if (f >= 1.0) {
        nextChannelTarget(ch);
    }
}

RotationAnimationDialog::~RotationAnimationDialog()
{
    // Finalize any in-flight recording before children are destroyed
    // (destroying the QProcess would kill ffmpeg and truncate the file)
    stop();
}

void RotationAnimationDialog::setSourceFile(const QString& path)
{
    source_file = path;
}

bool RotationAnimationDialog::needsLitMode() const
{
    return modelChannel.enabled->isChecked() || lightChannel.enabled->isChecked() || lightMoveBox->isChecked();
}

bool RotationAnimationDialog::anyAxisRandom() const
{
    return axisRandom[0]->isChecked() || axisRandom[1]->isChecked() || axisRandom[2]->isChecked();
}

bool RotationAnimationDialog::axisDriven(int axis) const
{
    if (!rotationEnabled->isChecked()) {
        return false;
    }
    if (axisRandom[axis]->isChecked()) {
        return true;
    }
    const QDoubleSpinBox* spins[3] = {xSpeed, ySpeed, zSpeed};
    return std::abs(spins[axis]->value()) > 0.01;
}

/*  Mouse drags during playback change the canvas orientation away from
 *  what the animation last applied.  Fold that user rotation into the
 *  base - but zero its component along every animation-driven axis, so
 *  driven axes are locked while free axes follow the mouse. */
void RotationAnimationDialog::absorbUserRotation()
{
    QMatrix4x4 rot;
    rot.rotate(ax, QVector3D(1, 0, 0));
    rot.rotate(ay, QVector3D(0, 1, 0));
    rot.rotate(az, QVector3D(0, 0, 1));
    const QMatrix4x4 expected = rot * base;
    const QMatrix4x4 actual = canvas->currentOrientation();
    const QMatrix4x4 delta = actual * expected.inverted();

    const float m3[9] = {delta(0, 0), delta(0, 1), delta(0, 2), delta(1, 0), delta(1, 1),
                         delta(1, 2), delta(2, 0), delta(2, 1), delta(2, 2)};
    QVector3D axis_v;
    float angle = 0;
    QQuaternion::fromRotationMatrix(QMatrix3x3(m3)).getAxisAndAngle(&axis_v, &angle);
    if (std::abs(angle) < 1e-3f || axis_v.lengthSquared() < 1e-9f) {
        return; // no user input this tick
    }

    // Rotation vector with driven components removed (per-frame deltas
    // are small, so the vector decomposition is accurate)
    QVector3D omega = axis_v.normalized() * angle;
    for (int i = 0; i < 3; ++i) {
        if (axisDriven(i)) {
            omega[i] = 0;
        }
    }
    QMatrix4x4 filtered;
    const float len = omega.length();
    if (len > 1e-5f) {
        filtered.rotate(len, omega / len);
    }
    base = rot.inverted() * filtered * rot * base;
}

void RotationAnimationDialog::nextRotationTarget(int axis)
{
    auto rng = QRandomGenerator::global();
    const double lo = std::min(axisSpeedMin[axis]->value(), axisSpeedMax[axis]->value());
    const double hi = std::max(axisSpeedMin[axis]->value(), axisSpeedMax[axis]->value());
    rotStart[axis] = curSpeed[axis];
    // Roughly one pick in three eases the axis to a stop so the motion
    // reads as "changing axis", not just jitter
    rotTarget[axis] = (rng->bounded(3) == 0) ? 0.0 : lo + rng->generateDouble() * (hi - lo);
    const double ilo = std::min(axisIntMin[axis]->value(), axisIntMax[axis]->value());
    const double ihi = std::max(axisIntMin[axis]->value(), axisIntMax[axis]->value());
    rotDuration[axis] = ilo + rng->generateDouble() * (ihi - ilo);
    rotElapsed[axis] = 0;
}

void RotationAnimationDialog::play()
{
    canvas->stopSpin();
    ax = ay = az = 0;
    curSpeed[0] = xSpeed->value();
    curSpeed[1] = ySpeed->value();
    curSpeed[2] = zSpeed->value();

    // Fixed-speed playback restarts from the default orientation so the
    // same settings reproduce the same motion; with random axes (or
    // rotation off) it continues from the current view
    const bool deterministic = rotationEnabled->isChecked() && !anyAxisRandom();
    if (deterministic) {
        base = canvas->defaultOrientation();
        canvas->setAnimationAngles(0, 0, 0, base);
    } else {
        base = canvas->currentOrientation();
    }
    for (int i = 0; i < 3; ++i) {
        if (axisRandom[i]->isChecked()) {
            nextRotationTarget(i);
        }
    }

    savedDrawMode = canvas->getDrawMode();
    modelChannel.active = lightChannel.active = bgChannel.active = false;
    opacityActive = false;
    wasLightMove = false;
    syncFeatureStates();

    clock.start();
    anim_timer.start();
    playing = true;
    statusLabel->setText(recording ? "Recording..." : "Playing");
    updateButtons();
}

void RotationAnimationDialog::syncFeatureStates()
{
    if (needsLitMode() && canvas->getDrawMode() != meshlight) {
        canvas->set_drawMode(meshlight);
    }

    if (modelChannel.enabled->isChecked() && !modelChannel.active) {
        startChannel(modelChannel, canvas->getAmbientColor());
        canvas->setAnimationModelColor(modelChannel.current);
    } else if (!modelChannel.enabled->isChecked() && modelChannel.active) {
        modelChannel.active = false;
        canvas->setAnimationModelColor(QColor());
    }
    if (lightChannel.enabled->isChecked() && !lightChannel.active) {
        startChannel(lightChannel, canvas->getDirectiveColor());
        canvas->setAnimationLightColor(lightChannel.current);
    } else if (!lightChannel.enabled->isChecked() && lightChannel.active) {
        lightChannel.active = false;
        canvas->setAnimationLightColor(QColor());
    }
    if (bgChannel.enabled->isChecked() && !bgChannel.active) {
        const QColor bg = canvas->getBackgroundColor();
        startChannel(bgChannel, bg.isValid() ? bg : QColor(0, 25, 35));
        canvas->setAnimationBackgroundColor(bgChannel.current);
    } else if (!bgChannel.enabled->isChecked() && bgChannel.active) {
        bgChannel.active = false;
        canvas->setAnimationBackgroundColor(QColor());
    }

    if (opacityEnabled->isChecked() && !opacityActive) {
        opStart = opTarget = opCurrent = canvas->getModelOpacity();
        opElapsed = 0;
        opDuration = 0.01; // pick a real target on the first tick
        opacityActive = true;
    } else if (!opacityEnabled->isChecked() && opacityActive) {
        opacityActive = false;
        canvas->setAnimationModelOpacity(-1);
    }

    if (lightMoveBox->isChecked() && !wasLightMove) {
        curDir = tgtDir = canvas->getLightDirectionVector(canvas->getCurrentLightDirection());
        lightPosLabel->setText(format_position(curDir));
    } else if (!lightMoveBox->isChecked() && wasLightMove) {
        canvas->clearAnimationLightDirection();
        lightPosLabel->setText("-");
    }
    wasLightMove = lightMoveBox->isChecked();
    lightDirCombo->setEnabled(!lightMoveBox->isChecked());
}

void RotationAnimationDialog::tick()
{
    const double dt = clock.restart() / 1000.0;
    syncFeatureStates();

    if (rotationEnabled->isChecked()) {
        // Keep the user's (axis-filtered) mouse movement, then advance
        // each axis: random axes glide between speeds over their own
        // interval (smoothstep) and show live values in the spinboxes
        absorbUserRotation();
        QDoubleSpinBox* spins[3] = {xSpeed, ySpeed, zSpeed};
        for (int i = 0; i < 3; ++i) {
            if (axisRandom[i]->isChecked()) {
                rotElapsed[i] += dt;
                const double f = rotDuration[i] > 0 ? rotElapsed[i] / rotDuration[i] : 1.0;
                curSpeed[i] = rotStart[i] + (rotTarget[i] - rotStart[i]) * smoothstep(f);
                if (f >= 1.0) {
                    nextRotationTarget(i);
                }
                spins[i]->blockSignals(true);
                spins[i]->setValue(curSpeed[i]);
                spins[i]->blockSignals(false);
            } else {
                curSpeed[i] = spins[i]->value();
            }
        }
    } else {
        // Rotation animation off: the view is fully free; colors and
        // light keep animating on top of whatever the user does
        base = canvas->currentOrientation();
        ax = ay = az = 0;
        curSpeed[0] = curSpeed[1] = curSpeed[2] = 0;
    }

    if (modelChannel.active) {
        tickChannel(modelChannel, dt);
        canvas->setAnimationModelColor(modelChannel.current);
    }
    if (lightChannel.active) {
        tickChannel(lightChannel, dt);
        canvas->setAnimationLightColor(lightChannel.current);
    }
    if (bgChannel.active) {
        tickChannel(bgChannel, dt);
        canvas->setAnimationBackgroundColor(bgChannel.current);
    }

    if (opacityActive) {
        opElapsed += dt;
        const double f = opDuration > 0 ? opElapsed / opDuration : 1.0;
        opCurrent = opStart + (opTarget - opStart) * smoothstep(f);
        canvas->setAnimationModelOpacity(opCurrent);
        if (f >= 1.0) {
            // Next random opacity within the user range, new duration
            auto rng = QRandomGenerator::global();
            const double lo = std::min(opacityMin->value(), opacityMax->value());
            const double hi = std::max(opacityMin->value(), opacityMax->value());
            opStart = opCurrent;
            opTarget = lo + rng->generateDouble() * (hi - lo);
            const double dlo = std::min(opacityDurMin->value(), opacityDurMax->value());
            const double dhi = std::max(opacityDurMin->value(), opacityDurMax->value());
            opDuration = dlo + rng->generateDouble() * (dhi - dlo);
            opElapsed = 0;
        }
    }

    if (lightMoveBox->isChecked()) {
        // Constant-speed travel; a new random target (within the per-axis
        // ranges) is picked the moment the light arrives
        const QVector3D delta = tgtDir - curDir;
        const float dist = delta.length();
        const float step = float(lightSpeed->value() * dt);
        if (dist <= step || dist < 1e-6f) {
            curDir = tgtDir;
            auto rng = QRandomGenerator::global();
            for (int attempt = 0; attempt < 20; ++attempt) {
                QVector3D v;
                for (int i = 0; i < 3; ++i) {
                    const double lo = std::min(lightMin[i]->value(), lightMax[i]->value());
                    const double hi = std::max(lightMin[i]->value(), lightMax[i]->value());
                    v[i] = lo + rng->generateDouble() * (hi - lo);
                }
                if (v.lengthSquared() >= 0.01) {
                    tgtDir = v;
                    break;
                }
            }
        } else {
            curDir += delta * (step / dist);
        }
        canvas->setAnimationLightDirection(curDir);
        lightPosLabel->setText(format_position(curDir));
    }

    ax += curSpeed[0] * dt;
    ay += curSpeed[1] * dt;
    az += curSpeed[2] * dt;
    canvas->setAnimationAngles(ax, ay, az, base);
}

QString RotationAnimationDialog::defaultRecordPath() const
{
    QString dir;
    QString stem = "fstl";
    if (!source_file.isEmpty() && !source_file.startsWith(":")) {
        const QFileInfo info(source_file);
        dir = info.absolutePath();
        stem = info.completeBaseName();
    }
    if (dir.isEmpty()) {
        dir = QStandardPaths::standardLocations(QStandardPaths::StandardLocation::MoviesLocation).first();
    }

    // Speeds (or "random" / "free") become part of the name
    QString suffix;
    if (!rotationEnabled->isChecked()) {
        suffix = QStringLiteral("free");
    } else if (anyAxisRandom()) {
        suffix = QStringLiteral("random");
    } else {
        suffix = QStringLiteral("x%1_y%2_z%3")
                     .arg(speed_tag(xSpeed->value()), speed_tag(ySpeed->value()), speed_tag(zSpeed->value()));
    }

    // Avoid clobbering earlier takes
    QString path = QDir(dir).filePath(QString("%1_%2.mp4").arg(stem, suffix));
    for (int n = 2; QFileInfo::exists(path); ++n) {
        path = QDir(dir).filePath(QString("%1_%2_%3.mp4").arg(stem, suffix).arg(n));
    }
    return path;
}

void RotationAnimationDialog::record()
{
    const QString ffmpeg_path = QStandardPaths::findExecutable("ffmpeg");
    if (ffmpeg_path.isEmpty()) {
        QMessageBox::warning(this, tr("ffmpeg not found"),
                             tr("Recording to MP4 requires ffmpeg.\n"
                                "Install it (e.g. 'sudo apt install ffmpeg') and try again."));
        return;
    }

    // Default next to the source file; prompt only if that isn't writable
    QString path = defaultRecordPath();
    {
        QFile probe(path);
        const bool writable = probe.open(QIODevice::WriteOnly);
        probe.close();
        probe.remove();
        if (!writable) {
            path = QFileDialog::getSaveFileName(this, tr("Record animation to MP4"), path, "MP4 video (*.mp4)");
            if (path.isEmpty()) {
                return;
            }
            if (!path.endsWith(".mp4", Qt::CaseInsensitive)) {
                path.append(".mp4");
            }
        }
    }

    recording = true;
    play(); // sets base and seeds the color channels (palette-locked)

    // Frame size is fixed by the first captured frame
    QImage first = canvas->grabAnimationFrame(ax, ay, az, base);
    record_w = first.width() & ~1;
    record_h = first.height() & ~1;

    ffmpeg = new QProcess(this);
    ffmpeg->setProcessChannelMode(QProcess::ForwardedErrorChannel);
    ffmpeg->start(ffmpeg_path,
                  {"-y", "-loglevel", "error", "-f", "rawvideo", "-pix_fmt", "rgba", "-s",
                   QString("%1x%2").arg(record_w).arg(record_h), "-r", QString::number(RECORD_FPS), "-i", "-", "-c:v",
                   "libx264", "-preset", "veryfast", "-pix_fmt", "yuv420p", "-movflags", "+faststart", path});
    if (!ffmpeg->waitForStarted(5000)) {
        QMessageBox::warning(this, tr("Recording failed"), tr("Could not start ffmpeg."));
        delete ffmpeg;
        ffmpeg = nullptr;
        recording = false;
        updateButtons();
        return;
    }

    record_path = path;
    statusLabel->setText(QString("Recording to %1").arg(QFileInfo(path).fileName()));
    frame_timer.start();
    updateButtons();
}

void RotationAnimationDialog::stop()
{
    anim_timer.stop();
    frame_timer.stop();
    playing = false;

    // Drop all transient overrides and restore the draw mode. Guarded
    // because stop() also runs from the destructor during app teardown,
    // where the Canvas may already be gone (QPointer auto-nulls).
    if (canvas) {
        canvas->clearAnimationOverrides();
        if (savedDrawMode >= 0 && savedDrawMode != canvas->getDrawMode()) {
            canvas->set_drawMode(DrawMode(savedDrawMode));
        }
    }
    modelChannel.active = lightChannel.active = bgChannel.active = false;
    opacityActive = false;
    savedDrawMode = -1;

    if (recording && ffmpeg) {
        statusLabel->setText("Finishing video...");
        ffmpeg->closeWriteChannel();
        ffmpeg->waitForFinished(60000);
        const bool ok = (ffmpeg->exitStatus() == QProcess::NormalExit && ffmpeg->exitCode() == 0);
        statusLabel->setText(ok ? QString("Saved %1").arg(record_path) : "Recording failed");
        delete ffmpeg;
        ffmpeg = nullptr;
    } else {
        statusLabel->setText(" ");
    }
    recording = false;
    record_w = record_h = 0;

    QSettings settings;
    settings.setValue("animate/xSpeed", xSpeed->value());
    settings.setValue("animate/ySpeed", ySpeed->value());
    settings.setValue("animate/zSpeed", zSpeed->value());
    settings.setValue("animate/rotationEnabled", rotationEnabled->isChecked());
    const char* axisIds[3] = {"X", "Y", "Z"};
    for (int i = 0; i < 3; ++i) {
        settings.setValue(QString("animate/random%1").arg(axisIds[i]), axisRandom[i]->isChecked());
        settings.setValue(QString("animate/speedMin%1").arg(axisIds[i]), axisSpeedMin[i]->value());
        settings.setValue(QString("animate/speedMax%1").arg(axisIds[i]), axisSpeedMax[i]->value());
        settings.setValue(QString("animate/intervalMin%1").arg(axisIds[i]), axisIntMin[i]->value());
        settings.setValue(QString("animate/intervalMax%1").arg(axisIds[i]), axisIntMax[i]->value());
    }
    for (const ColorChannel* ch : {&modelChannel, &lightChannel, &bgChannel}) {
        settings.setValue("animate/" + ch->settingsId + "Random", ch->enabled->isChecked());
        settings.setValue("animate/" + ch->settingsId + "MinDur", ch->minDur->value());
        settings.setValue("animate/" + ch->settingsId + "MaxDur", ch->maxDur->value());
        QStringList names;
        for (const QColor& c : ch->palette) {
            names << c.name();
        }
        settings.setValue("animate/" + ch->settingsId + "Palette", names);
    }
    settings.setValue("animate/opacityRandom", opacityEnabled->isChecked());
    settings.setValue("animate/opacityMin", opacityMin->value());
    settings.setValue("animate/opacityMax", opacityMax->value());
    settings.setValue("animate/opacityDurMin", opacityDurMin->value());
    settings.setValue("animate/opacityDurMax", opacityDurMax->value());
    settings.setValue("animate/randomLightMove", lightMoveBox->isChecked());
    settings.setValue("animate/lightSpeed", lightSpeed->value());
    const QString minKeys[3] = {"animate/lightMinX", "animate/lightMinY", "animate/lightMinZ"};
    const QString maxKeys[3] = {"animate/lightMaxX", "animate/lightMaxY", "animate/lightMaxZ"};
    for (int i = 0; i < 3; ++i) {
        settings.setValue(minKeys[i], lightMin[i]->value());
        settings.setValue(maxKeys[i], lightMax[i]->value());
    }

    updateButtons();
}

void RotationAnimationDialog::updateButtons()
{
    playButton->setEnabled(!playing);
    recordButton->setEnabled(!recording);
    stopButton->setEnabled(playing || recording);
}

void RotationAnimationDialog::hideEvent(QHideEvent* event)
{
    stop();
    QDialog::hideEvent(event);
}
