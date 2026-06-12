#include "shaderlightprefs.h"
#include "canvas.h"
#include <QApplication>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QDoubleValidator>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

const QString ShaderLightPrefs::PREFS_GEOM = "shaderPrefsGeometry";

ShaderLightPrefs::ShaderLightPrefs(QWidget* parent, Canvas* _canvas) : QDialog(parent)
{
    canvas = _canvas;

    QVBoxLayout* prefsLayout = new QVBoxLayout;
    this->setLayout(prefsLayout);

    QLabel* title = new QLabel("Shader preferences");
    QFont boldFont = QApplication::font();
    boldFont.setWeight(QFont::Bold);
    title->setFont(boldFont);
    title->setAlignment(Qt::AlignCenter);
    prefsLayout->addWidget(title);

    QWidget* middleWidget = new QWidget;
    QGridLayout* middleLayout = new QGridLayout;
    middleWidget->setLayout(middleLayout);
    this->layout()->addWidget(middleWidget);

    // labels
    middleLayout->addWidget(new QLabel("Ambient Color"), 0, 0);
    middleLayout->addWidget(new QLabel("Directive Color"), 1, 0);
    middleLayout->addWidget(new QLabel("Direction"), 2, 0);
    middleLayout->addWidget(new QLabel("Brightness"), 3, 0);
    middleLayout->addWidget(new QLabel("Opacity"), 4, 0);
    middleLayout->addWidget(new QLabel("Background"), 5, 0);

    QPixmap dummy(20, 20);

    dummy.fill(canvas->getAmbientColor());
    buttonAmbientColor = new QPushButton;
    buttonAmbientColor->setIcon(QIcon(dummy));
    middleLayout->addWidget(buttonAmbientColor, 0, 1);
    buttonAmbientColor->setFocusPolicy(Qt::NoFocus);
    connect(buttonAmbientColor, &QPushButton::clicked, this, &ShaderLightPrefs::buttonAmbientColorClicked);

    editAmbientFactor = new QLineEdit;
    // setValidator does not take ownership; parent to the line edit
    editAmbientFactor->setValidator(new QDoubleValidator(editAmbientFactor));
    editAmbientFactor->setText(QString("%1").arg(canvas->getAmbientFactor()));
    middleLayout->addWidget(editAmbientFactor, 0, 2);
    connect(editAmbientFactor, &QLineEdit::editingFinished, this, &ShaderLightPrefs::editAmbientFactorFinished);

    QPushButton* buttonResetAmbientColor = new QPushButton("Reset");
    middleLayout->addWidget(buttonResetAmbientColor, 0, 3);
    buttonResetAmbientColor->setFocusPolicy(Qt::NoFocus);
    connect(buttonResetAmbientColor, &QPushButton::clicked, this, &ShaderLightPrefs::resetAmbientColorClicked);

    dummy.fill(canvas->getDirectiveColor());
    buttonDirectiveColor = new QPushButton;
    buttonDirectiveColor->setIcon(QIcon(dummy));
    middleLayout->addWidget(buttonDirectiveColor, 1, 1);
    buttonDirectiveColor->setFocusPolicy(Qt::NoFocus);
    connect(buttonDirectiveColor, &QPushButton::clicked, this, &ShaderLightPrefs::buttonDirectiveColorClicked);

    editDirectiveFactor = new QLineEdit;
    editDirectiveFactor->setValidator(new QDoubleValidator(editDirectiveFactor));
    editDirectiveFactor->setText(QString("%1").arg(canvas->getDirectiveFactor()));
    middleLayout->addWidget(editDirectiveFactor, 1, 2);
    connect(editDirectiveFactor, &QLineEdit::editingFinished, this, &ShaderLightPrefs::editDirectiveFactorFinished);

    QPushButton* buttonResetDirectiveColor = new QPushButton("Reset");
    middleLayout->addWidget(buttonResetDirectiveColor, 1, 3);
    buttonResetDirectiveColor->setFocusPolicy(Qt::NoFocus);
    connect(buttonResetDirectiveColor, &QPushButton::clicked, this, &ShaderLightPrefs::resetDirectiveColorClicked);

    // Fill in directions

    comboDirections = new QComboBox;
    middleLayout->addWidget(comboDirections, 2, 1, 1, 2);
    comboDirections->addItems(canvas->getNameDir());
    comboDirections->setCurrentIndex(canvas->getCurrentLightDirection());
    connect(comboDirections, qOverload<int>(&QComboBox::currentIndexChanged), this, &ShaderLightPrefs::comboDirectionsChanged);

    QPushButton* buttonResetDirection = new QPushButton("Reset");
    middleLayout->addWidget(buttonResetDirection, 2, 3);
    buttonResetDirection->setFocusPolicy(Qt::NoFocus);
    connect(buttonResetDirection, &QPushButton::clicked, this, &ShaderLightPrefs::resetDirection);

    // Overall light brightness (multiplies both factors)
    spinBrightness = new QDoubleSpinBox;
    spinBrightness->setRange(0.0, 3.0);
    spinBrightness->setSingleStep(0.1);
    spinBrightness->setDecimals(2);
    spinBrightness->setValue(canvas->getLightBrightness());
    middleLayout->addWidget(spinBrightness, 3, 1, 1, 2);
    connect(spinBrightness, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [this](double b) { canvas->setLightBrightness(b); });

    QPushButton* buttonResetBrightness = new QPushButton("Reset");
    middleLayout->addWidget(buttonResetBrightness, 3, 3);
    buttonResetBrightness->setFocusPolicy(Qt::NoFocus);
    connect(buttonResetBrightness, &QPushButton::clicked, this, [this] {
        canvas->resetLightBrightness();
        spinBrightness->setValue(canvas->getLightBrightness());
    });

    // Model opacity (applies to every draw mode)
    spinOpacity = new QDoubleSpinBox;
    spinOpacity->setRange(0.0, 1.0);
    spinOpacity->setSingleStep(0.05);
    spinOpacity->setDecimals(2);
    spinOpacity->setValue(canvas->getModelOpacity());
    middleLayout->addWidget(spinOpacity, 4, 1, 1, 2);
    connect(spinOpacity, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [this](double o) { canvas->setModelOpacity(o); });

    QPushButton* buttonResetOpacity = new QPushButton("Reset");
    middleLayout->addWidget(buttonResetOpacity, 4, 3);
    buttonResetOpacity->setFocusPolicy(Qt::NoFocus);
    connect(buttonResetOpacity, &QPushButton::clicked, this, [this] {
        canvas->resetModelOpacity();
        spinOpacity->setValue(canvas->getModelOpacity());
    });

    // Background color (applies to every draw mode; reset = gradient)
    buttonBackgroundColor = new QPushButton;
    middleLayout->addWidget(buttonBackgroundColor, 5, 1);
    buttonBackgroundColor->setFocusPolicy(Qt::NoFocus);
    connect(buttonBackgroundColor, &QPushButton::clicked, this, &ShaderLightPrefs::buttonBackgroundColorClicked);
    updateBackgroundSwatch();

    QPushButton* buttonResetBackground = new QPushButton("Reset");
    middleLayout->addWidget(buttonResetBackground, 5, 3);
    buttonResetBackground->setFocusPolicy(Qt::NoFocus);
    connect(buttonResetBackground, &QPushButton::clicked, this, &ShaderLightPrefs::resetBackgroundColor);

    // Ok button
    QWidget* boxButton = new QWidget;
    QHBoxLayout* boxButtonLayout = new QHBoxLayout;
    boxButton->setLayout(boxButtonLayout);
    QFrame* spacerL = new QFrame;
    spacerL->setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Expanding));
    QPushButton* okButton = new QPushButton("Ok");
    boxButtonLayout->addWidget(spacerL);
    boxButtonLayout->addWidget(okButton);
    this->layout()->addWidget(boxButton);
    okButton->setFocusPolicy(Qt::NoFocus);
    connect(okButton, &QPushButton::clicked, this, &ShaderLightPrefs::okButtonClicked);

    QSettings settings;
    if (!settings.value(PREFS_GEOM).isNull()) {
        restoreGeometry(settings.value(PREFS_GEOM).toByteArray());
    }
}

void ShaderLightPrefs::buttonAmbientColorClicked()
{
    QColor newColor =
        QColorDialog::getColor(canvas->getAmbientColor(), this, QString("Choose color"), QColorDialog::DontUseNativeDialog);
    if (newColor.isValid() == true) {
        canvas->setAmbientColor(newColor);
        QPixmap dummy(20, 20);
        dummy.fill(canvas->getAmbientColor());
        buttonAmbientColor->setIcon(QIcon(dummy));
        canvas->update();
    }
}

void ShaderLightPrefs::editAmbientFactorFinished()
{
    canvas->setAmbientFactor(editAmbientFactor->text().toDouble());
    canvas->update();
}

void ShaderLightPrefs::resetAmbientColorClicked()
{
    canvas->resetAmbientColor();
    QPixmap dummy(20, 20);
    dummy.fill(canvas->getAmbientColor());
    buttonAmbientColor->setIcon(QIcon(dummy));
    editAmbientFactor->setText(QString("%1").arg(canvas->getAmbientFactor()));
    canvas->update();
}

void ShaderLightPrefs::buttonDirectiveColorClicked()
{
    QColor newColor =
        QColorDialog::getColor(canvas->getDirectiveColor(), this, QString("Choose color"), QColorDialog::DontUseNativeDialog);
    if (newColor.isValid() == true) {
        canvas->setDirectiveColor(newColor);
        QPixmap dummy(20, 20);
        dummy.fill(canvas->getDirectiveColor());
        buttonDirectiveColor->setIcon(QIcon(dummy));
        canvas->update();
    }
}

void ShaderLightPrefs::editDirectiveFactorFinished()
{
    canvas->setDirectiveFactor(editDirectiveFactor->text().toDouble());
    canvas->update();
}

void ShaderLightPrefs::resetDirectiveColorClicked()
{
    canvas->resetDirectiveColor();
    QPixmap dummy(20, 20);
    dummy.fill(canvas->getDirectiveColor());
    buttonDirectiveColor->setIcon(QIcon(dummy));
    editDirectiveFactor->setText(QString("%1").arg(canvas->getDirectiveFactor()));
    canvas->update();
}

void ShaderLightPrefs::updateBackgroundSwatch()
{
    const QColor bg = canvas->getBackgroundColor();
    QPixmap swatch(20, 20);
    if (bg.isValid()) {
        swatch.fill(bg);
        buttonBackgroundColor->setText("");
    } else {
        swatch.fill(QColor(0, 25, 35)); // approximate gradient tone
        buttonBackgroundColor->setText("Gradient");
    }
    buttonBackgroundColor->setIcon(QIcon(swatch));
}

void ShaderLightPrefs::buttonBackgroundColorClicked()
{
    const QColor initial = canvas->getBackgroundColor().isValid() ? canvas->getBackgroundColor() : QColor(0, 25, 35);
    QColor newColor = QColorDialog::getColor(initial, this, QString("Choose background color"), QColorDialog::DontUseNativeDialog);
    if (newColor.isValid()) {
        canvas->setBackgroundColor(newColor);
        updateBackgroundSwatch();
    }
}

void ShaderLightPrefs::resetBackgroundColor()
{
    canvas->resetBackgroundColor();
    updateBackgroundSwatch();
}

void ShaderLightPrefs::okButtonClicked()
{
    this->close();
}

void ShaderLightPrefs::comboDirectionsChanged(int ind)
{
    canvas->setCurrentLightDirection(ind);
    canvas->update();
}

void ShaderLightPrefs::resetDirection()
{
    canvas->resetCurrentLightDirection();
    comboDirections->setCurrentIndex(canvas->getCurrentLightDirection());
    canvas->update();
}

void ShaderLightPrefs::hideEvent(QHideEvent* event)
{
    // Persist geometry once when the dialog is dismissed rather than on
    // every move/resize (the dialog is hidden, not destroyed, on close)
    QSettings().setValue(PREFS_GEOM, saveGeometry());
    QDialog::hideEvent(event);
}
