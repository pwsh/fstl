#include "statisticsdialog.h"
#include "canvas.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QSettings>
#include <QVBoxLayout>

const QString StatisticsDialog::ENABLED_KEY = "statistics/enabled";
const QString StatisticsDialog::ITEMS_KEY = "statistics/items";

namespace
{
// label, StatFlag bit
const QList<QPair<QString, int>>& statItems()
{
    static const QList<QPair<QString, int>> defs = {
        {"Triangle count", StatTriangles},     {"Bounding box", StatBoundingBox},
        {"Model size", StatModelSize},          {"Orientation", StatOrientation},
        {"Rotation speed", StatRotationSpeed},  {"Frame rate (FPS)", StatFps},
        {"Zoom / projection", StatZoomProjection}, {"Draw mode", StatDrawMode},
        {"Colors", StatColors},                 {"Lighting", StatLighting},
    };
    return defs;
}
} // namespace

void StatisticsDialog::applySaved(Canvas* canvas)
{
    QSettings settings;
    const bool enabled = settings.value(ENABLED_KEY, false).toBool();
    const int itemFlags = settings.value(ITEMS_KEY, 0).toInt();
    canvas->setStatFlags(enabled ? itemFlags : 0);
}

StatisticsDialog::StatisticsDialog(QWidget* parent, Canvas* _canvas) : QDialog(parent), canvas(_canvas)
{
    setWindowTitle("Statistics");

    auto layout = new QVBoxLayout(this);

    enableBox = new QCheckBox("Show statistics overlay");
    QFont f = enableBox->font();
    f.setBold(true);
    enableBox->setFont(f);
    layout->addWidget(enableBox);

    layout->addWidget(new QLabel("Items to show:"));

    QSettings settings;
    const bool enabled = settings.value(ENABLED_KEY, false).toBool();
    const int savedItems = settings.value(ITEMS_KEY, 0).toInt();

    enableBox->setChecked(enabled);
    for (const auto& d : statItems()) {
        auto box = new QCheckBox(d.first);
        box->setChecked(savedItems & d.second);
        box->setEnabled(enabled); // greyed while the overlay is off
        layout->addWidget(box);
        items.append({box, d.second});
        connect(box, &QCheckBox::toggled, this, &StatisticsDialog::apply);
    }
    connect(enableBox, &QCheckBox::toggled, this, &StatisticsDialog::apply);

    auto buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
    layout->addWidget(buttons);

    // No canvas call here: the persisted state is applied at startup by
    // applySaved(); constructing the dialog has no side effects.
}

void StatisticsDialog::apply()
{
    const bool enabled = enableBox->isChecked();

    int itemFlags = 0;
    for (const auto& pair : items) {
        pair.first->setEnabled(enabled);
        if (pair.first->isChecked()) {
            itemFlags |= pair.second;
        }
    }

    canvas->setStatFlags(enabled ? itemFlags : 0);

    QSettings settings;
    settings.setValue(ENABLED_KEY, enabled);
    settings.setValue(ITEMS_KEY, itemFlags);
}
