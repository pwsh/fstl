#ifndef CLIEXPORT_H
#define CLIEXPORT_H

#include <QStringList>

/*  True if the arguments request a command-line export (no GUI). */
bool cli_export_requested(int argc, char* argv[]);

/*  Runs the command-line export. Requires a Q(Gui)Application to exist.
 *  Returns a process exit code. */
int run_cli_export(const QStringList& args);

#endif // CLIEXPORT_H
