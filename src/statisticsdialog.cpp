#include "statisticsdialog.h"
#include "canvas.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QSettings>
#include <QVBoxLayout>

const QString StatisticsDialog::ENABLED_KEY = "statistics/enabled";
const QString StatisticsDialog::ITEMS_KEY = "statistics/items";

StatisticsDialog::StatisticsDialog(QWidget* parent, Canvas* _canvas) : QDialog(parent), canvas(_canvas)
{
    setWindowTitle("Statistics");

    auto layout = new QVBoxLayout(this);

    enableBox = new QCheckBox("Show statistics overlay");
    QFont f = enableBox->font();
    f.setBold(true);
    enableBox->setFont(f);
    layout->addWidget(enableBox);

    itemsBox = new QGroupBox("Items to show");
    auto itemsLayout = new QVBoxLayout(itemsBox);
    layout->addWidget(itemsBox);

    // label, StatFlag bit
    const QList<QPair<QString, int>> defs = {
        {"Triangle count", StatTriangles},     {"Bounding box", StatBoundingBox},
        {"Model size", StatModelSize},          {"Orientation", StatOrientation},
        {"Rotation speed", StatRotationSpeed},  {"Frame rate (FPS)", StatFps},
        {"Zoom / projection", StatZoomProjection}, {"Draw mode", StatDrawMode},
        {"Colors", StatColors},                 {"Lighting", StatLighting},
    };

    QSettings settings;
    const bool enabled = settings.value(ENABLED_KEY, false).toBool();
    const int savedItems = settings.value(ITEMS_KEY, 0).toInt();

    enableBox->setChecked(enabled);
    for (const auto& d : defs) {
        auto box = new QCheckBox(d.first);
        box->setChecked(savedItems & d.second);
        itemsLayout->addWidget(box);
        items.append({box, d.second});
        connect(box, &QCheckBox::toggled, this, &StatisticsDialog::apply);
    }
    connect(enableBox, &QCheckBox::toggled, this, &StatisticsDialog::apply);

    auto buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::hide);
    layout->addWidget(buttons);

    apply(); // push the persisted state to the canvas at startup
}

void StatisticsDialog::apply()
{
    const bool enabled = enableBox->isChecked();
    itemsBox->setEnabled(enabled); // grey out the items when the overlay is off

    int itemFlags = 0;
    for (const auto& pair : items) {
        if (pair.first->isChecked()) {
            itemFlags |= pair.second;
        }
    }

    canvas->setStatFlags(enabled ? itemFlags : 0);

    QSettings settings;
    settings.setValue(ENABLED_KEY, enabled);
    settings.setValue(ITEMS_KEY, itemFlags);
}
