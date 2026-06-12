#include "keybindingsdialog.h"

#include <QAction>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QVBoxLayout>

namespace
{
QString settings_key(const QString& id)
{
    return "keys/" + id;
}
} // namespace

void apply_saved_key_bindings(const QList<BindableAction>& actions)
{
    QSettings settings;
    for (const auto& a : actions) {
        const QString saved = settings.value(settings_key(a.id)).toString();
        a.action->setShortcut(settings.contains(settings_key(a.id)) ? QKeySequence(saved) : a.defaultShortcut);
    }
}

KeyBindingsDialog::KeyBindingsDialog(QWidget* parent, const QList<BindableAction>& actions) : QDialog(parent), acts(actions)
{
    setWindowTitle("Keyboard Shortcuts");
    resize(420, 520);

    auto layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel("Click a field and press the new key combination.\n"
                                 "Backspace clears a binding."));

    auto scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    auto inner = new QWidget;
    auto form = new QFormLayout(inner);
    for (const auto& a : acts) {
        auto edit = new QKeySequenceEdit(a.action->shortcut());
        edits << edit;
        form->addRow(a.label, edit);
    }
    scroll->setWidget(inner);
    layout->addWidget(scroll);

    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults);
    connect(buttons, &QDialogButtonBox::accepted, this, &KeyBindingsDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this, &KeyBindingsDialog::resetDefaults);
    layout->addWidget(buttons);
}

void KeyBindingsDialog::resetDefaults()
{
    for (int i = 0; i < acts.size(); ++i) {
        edits[i]->setKeySequence(acts[i].defaultShortcut);
    }
}

void KeyBindingsDialog::accept()
{
    // Warn on duplicate assignments (empty bindings are fine)
    for (int i = 0; i < acts.size(); ++i) {
        const auto seq = edits[i]->keySequence();
        if (seq.isEmpty()) {
            continue;
        }
        for (int j = i + 1; j < acts.size(); ++j) {
            if (seq == edits[j]->keySequence()) {
                QMessageBox::warning(this, tr("Duplicate shortcut"),
                                     tr("\"%1\" and \"%2\" are both bound to %3.\nPlease change one of them.")
                                         .arg(acts[i].label, acts[j].label, seq.toString()));
                return;
            }
        }
    }

    QSettings settings;
    for (int i = 0; i < acts.size(); ++i) {
        const auto seq = edits[i]->keySequence();
        acts[i].action->setShortcut(seq);
        if (seq == acts[i].defaultShortcut) {
            settings.remove(settings_key(acts[i].id));
        } else {
            settings.setValue(settings_key(acts[i].id), seq.toString());
        }
    }
    QDialog::accept();
}
