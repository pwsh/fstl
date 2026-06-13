#ifndef STATISTICSDIALOG_H
#define STATISTICSDIALOG_H

#include <QDialog>
#include <QList>
#include <QPair>
#include <QString>

class Canvas;
class QCheckBox;

/*  Chooses which statistics to overlay on the viewport. A master switch
 *  enables/disables the whole overlay; the individual items below select
 *  what is shown when it is enabled. Applies live and persists. */
class StatisticsDialog : public QDialog
{
    Q_OBJECT
public:
    StatisticsDialog(QWidget* parent, Canvas* canvas);

    /*  Pushes the persisted statistics selection to the canvas. Called
     *  once at startup so the overlay state is restored even if the
     *  dialog is never opened. */
    static void applySaved(Canvas* canvas);

    static const QString ENABLED_KEY;
    static const QString ITEMS_KEY;

private:
    void apply(); // recompute flags, push to the canvas, and persist

    Canvas* canvas;
    QCheckBox* enableBox;
    QList<QPair<QCheckBox*, int>> items;
};

#endif // STATISTICSDIALOG_H
