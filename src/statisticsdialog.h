#ifndef STATISTICSDIALOG_H
#define STATISTICSDIALOG_H

#include <QDialog>
#include <QList>
#include <QPair>

class Canvas;
class QCheckBox;
class QGroupBox;

/*  Chooses which statistics to overlay on the viewport. A master switch
 *  enables/disables the whole overlay; the individual items below select
 *  what is shown when it is enabled. Applies live and persists. */
class StatisticsDialog : public QDialog
{
    Q_OBJECT
public:
    StatisticsDialog(QWidget* parent, Canvas* canvas);

private:
    void apply(); // recompute flags, push to the canvas, and persist

    Canvas* canvas;
    QCheckBox* enableBox;
    QGroupBox* itemsBox;
    QList<QPair<QCheckBox*, int>> items;

    static const QString ENABLED_KEY;
    static const QString ITEMS_KEY;
};

#endif // STATISTICSDIALOG_H
