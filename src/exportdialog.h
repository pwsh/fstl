#ifndef EXPORTDIALOG_H
#define EXPORTDIALOG_H

#include <QDialog>
#include <QVector3D>

#include <vector>

class QComboBox;
class QSpinBox;
class QDoubleSpinBox;

/*  Options shared by the PNG-sequence and animated-GIF exporters */
struct RotationExportOptions {
    QVector3D axis;          // view-space rotation axis
    bool bounce = false;     // false: sweep then loop; true: bounce between angles
    float sweep = 360.0f;    // total rotation in loop mode (degrees, <= 360)
    float startAngle = -90;  // bounce mode: first endpoint (degrees)
    float endAngle = 90;     // bounce mode: second endpoint (degrees)
    int frames = 8;          // PNG mode: number of images
    float step = 3.0f;       // GIF mode: degrees per frame
    int fps = 25;            // GIF playback speed
    int gifWidth = 480;      // GIF output width in pixels (height keeps aspect)

    /*  The sequence of rotation angles (relative to the current view)
     *  that realizes these options, for either exporter. */
    std::vector<float> pngAngles() const;
    std::vector<float> gifAngles() const;
};

/*  Dialog for exporting a sequence of PNGs at evenly-spaced rotations */
class PngExportDialog : public QDialog
{
    Q_OBJECT
public:
    explicit PngExportDialog(QWidget* parent = nullptr);
    RotationExportOptions options() const;

    void accept() override; // persists the chosen values

private:
    QComboBox* axisCombo;
    QSpinBox* framesSpin;
    QDoubleSpinBox* sweepSpin;
};

/*  Options for MP4 export: total degrees per axis over a fixed duration.
 *  Speeds follow from degrees/duration, so e.g. x=720 with y=360 spins
 *  around X twice as fast as around Y. */
struct Mp4ExportOptions {
    double duration = 8;  // seconds
    double degreesX = 0;  // total rotation, may exceed 360
    double degreesY = 360;
    double degreesZ = 0;
    int fps = 30;
    int width = 960; // max output width in pixels
};

/*  Dialog for exporting an MP4 of the model rotating about up to three axes */
class Mp4ExportDialog : public QDialog
{
    Q_OBJECT
public:
    explicit Mp4ExportDialog(QWidget* parent = nullptr);
    Mp4ExportOptions options() const;

    void accept() override; // persists the chosen values

private:
    QDoubleSpinBox* durationSpin;
    QDoubleSpinBox* degXSpin;
    QDoubleSpinBox* degYSpin;
    QDoubleSpinBox* degZSpin;
    QSpinBox* fpsSpin;
    QSpinBox* widthSpin;
};

/*  Dialog for exporting an animated GIF of the rotating model */
class GifExportDialog : public QDialog
{
    Q_OBJECT
public:
    explicit GifExportDialog(QWidget* parent = nullptr);
    RotationExportOptions options() const;

    void accept() override; // persists the chosen values

private slots:
    void modeChanged(int index);

private:
    QComboBox* axisCombo;
    QComboBox* modeCombo;
    QDoubleSpinBox* sweepSpin;
    QDoubleSpinBox* startSpin;
    QDoubleSpinBox* endSpin;
    QDoubleSpinBox* stepSpin;
    QSpinBox* fpsSpin;
    QSpinBox* widthSpin;
};

#endif // EXPORTDIALOG_H
