#ifndef ANIMATEDIALOG_H
#define ANIMATEDIALOG_H

#include <QColor>
#include <QDialog>
#include <QElapsedTimer>
#include <QList>
#include <QMatrix4x4>
#include <QPointer>
#include <QTimer>
#include <QVector3D>

class Canvas;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QHBoxLayout;
class QLabel;
class QProcess;
class QPushButton;
class QVBoxLayout;

/*  Modeless dialog that continuously animates the model, organized in
 *  collapsible sections:
 *  - Rotation: per-axis speeds; random mode re-rolls target speeds
 *    within a user speed range at random intervals, gliding smoothly
 *    (smoothstep over the whole interval).
 *  - Model / Light / Background color: independent channels, each with
 *    its own palette and its own transition-duration range.  With a
 *    palette, colors are locked to it: the starting color snaps to a
 *    palette color and every target comes from the palette.
 *  - Light source: direction, brightness, and constant-speed random
 *    movement within per-axis ranges (a new target is picked on
 *    arrival).
 *  Live recording to MP4 via ffmpeg.
 *
 *  Fixed-speed playback starts from the default orientation so the
 *  same settings reproduce the same motion; random rotation continues
 *  from the current view instead. */
class RotationAnimationDialog : public QDialog
{
    Q_OBJECT
public:
    RotationAnimationDialog(QWidget* parent, Canvas* canvas);
    ~RotationAnimationDialog() override;

    /*  The .stl currently shown; used to derive the default recording
     *  location and file name. */
    void setSourceFile(const QString& path);

    bool isAnimating() const
    {
        return playing;
    }

protected:
    void hideEvent(QHideEvent* event) override; // stops playback/recording

private slots:
    void play();
    void record();
    void stop();

private:
    /*  One independently-animated color (model, light, or background):
     *  enable flag, palette, duration range, and the in-flight
     *  transition state. */
    struct ColorChannel {
        QString settingsId;
        QCheckBox* enabled = nullptr;
        QHBoxLayout* paletteRow = nullptr;
        QDoubleSpinBox* minDur = nullptr;
        QDoubleSpinBox* maxDur = nullptr;
        QList<QColor> palette;

        QColor start, target, current;
        double duration = 3, elapsed = 0;
        bool active = false; // currently driving an override
    };

    void tick();
    void tickChannel(ColorChannel& ch, double dt);
    void startChannel(ColorChannel& ch, const QColor& fallback);
    void nextChannelTarget(ColorChannel& ch);
    QColor pickTargetColor(const ColorChannel& ch, const QColor& current) const;
    QWidget* buildColorSection(ColorChannel& ch, const QString& title);
    void rebuildPaletteRow(ColorChannel& ch);
    void syncFeatureStates();
    void nextRotationTarget(int axis);
    bool anyAxisRandom() const;
    bool axisDriven(int axis) const; // animation owns this axis
    void absorbUserRotation();       // fold mouse drags into the base, per-axis filtered
    void updateButtons();
    QString defaultRecordPath() const;
    bool needsLitMode() const;

    // QPointer so it auto-nulls if the Canvas is destroyed first during
    // application teardown (stop() guards against that)
    QPointer<Canvas> canvas;

    // Rotation
    QDoubleSpinBox* xSpeed;
    QDoubleSpinBox* ySpeed;
    QDoubleSpinBox* zSpeed;
    QCheckBox* rotationEnabled; // master switch: off = view fully free
    // Per-axis random-rotation settings (X, Y, Z)
    QCheckBox* axisRandom[3];        // per-axis random movement
    QDoubleSpinBox* axisSpeedMin[3]; // random target speed range, deg/s
    QDoubleSpinBox* axisSpeedMax[3];
    QDoubleSpinBox* axisIntMin[3]; // seconds between random speed changes
    QDoubleSpinBox* axisIntMax[3];

    // Colors
    ColorChannel modelChannel;
    ColorChannel lightChannel;
    ColorChannel bgChannel;

    // Model transparency animation
    QCheckBox* opacityEnabled;
    QDoubleSpinBox* opacityMin;
    QDoubleSpinBox* opacityMax;
    QDoubleSpinBox* opacityDurMin;
    QDoubleSpinBox* opacityDurMax;
    double opStart = 1, opTarget = 1, opCurrent = 1;
    double opDuration = 3, opElapsed = 0;
    bool opacityActive = false;

    // Light
    QComboBox* lightDirCombo;
    QCheckBox* lightMoveBox;
    QDoubleSpinBox* lightSpeed;      // travel speed, units/s
    QDoubleSpinBox* lightBrightness; // shared canvas brightness
    QDoubleSpinBox* lightMin[3];     // per-axis random range
    QDoubleSpinBox* lightMax[3];
    QLabel* lightPosLabel; // live position readout

    QPushButton* playButton;
    QPushButton* recordButton;
    QPushButton* stopButton;
    QLabel* statusLabel;

    QTimer anim_timer;  // drives the animation
    QTimer frame_timer; // captures frames while recording
    QElapsedTimer clock;

    double ax = 0, ay = 0, az = 0; // accumulated angles (degrees)
    // Per-axis random-rotation transition state (smoothstep between speeds)
    double rotStart[3] = {0, 0, 0};
    double rotTarget[3] = {0, 0, 0};
    double rotDuration[3] = {3, 3, 3};
    double rotElapsed[3] = {0, 0, 0};
    double curSpeed[3] = {0, 0, 0};

    QMatrix4x4 base; // orientation the angles build on
    bool wasLightMove = false;
    int savedDrawMode = -1; // draw mode to restore after color/light animation

    QVector3D curDir, tgtDir;

    QString source_file;

    bool playing = false;
    bool recording = false;
    QProcess* ffmpeg = nullptr;
    QString record_path;
    int record_w = 0, record_h = 0;
};

#endif // ANIMATEDIALOG_H
