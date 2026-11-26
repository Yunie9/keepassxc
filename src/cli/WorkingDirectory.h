#ifndef KEEPASSXC_WORKINGDIRECTORY_H
#define KEEPASSXC_WORKINGDIRECTORY_H

#include "Command.h"

#include <QPointer>

class ChangeDirectory : public Command
{
public:
    ChangeDirectory();
    int execute(const QStringList& arguments) override;

private:
    QPointer<Group> m_previousGroup;
};

class PrintWorkingDirectory : public Command
{
public:
    PrintWorkingDirectory();
    int execute(const QStringList& arguments) override;
};

#endif // KEEPASSXC_WORKINGDIRECTORY_H
