#ifndef KEEPASSXC_INTERACTIVECOMPLETION_H
#define KEEPASSXC_INTERACTIVECOMPLETION_H

#include "LineReader.h"

class Group;

LineReader::Completion completeInteractiveCommand(const QString& line, qsizetype cursor, Group* workingGroup);

#endif // KEEPASSXC_INTERACTIVECOMPLETION_H
