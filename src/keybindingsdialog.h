#ifndef KEYBINDINGSDIALOG_H
#define KEYBINDINGSDIALOG_H

#include <QDialog>
#include <QKeySequence>
#include <QList>

class QAction;
class QKeySequenceEdit;

/*  One user-rebindable action: a stable settings id, a human label,
 *  the action it drives, and its built-in default shortcut. */
struct BindableAction {
    QString id;
    QString label;
    QAction* action;
    QKeySequence defaultShortcut;
};

/*  Applies shortcuts saved in QSettings (falling back to defaults) to
 *  the actions; called once at startup. */
void apply_saved_key_bindings(const QList<BindableAction>& actions);

/*  Dialog listing every bindable action with an editable key field,
 *  plus reset-to-defaults. Saves and applies on OK. */
class KeyBindingsDialog : public QDialog
{
    Q_OBJECT
public:
    KeyBindingsDialog(QWidget* parent, const QList<BindableAction>& actions);

    void accept() override;

private slots:
    void resetDefaults();

private:
    QList<BindableAction> acts;
    QList<QKeySequenceEdit*> edits;
};

#endif // KEYBINDINGSDIALOG_H
