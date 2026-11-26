#include "WorkingDirectory.h"

#include "Utils.h"
#include "core/Group.h"

#include <QCommandLineParser>

ChangeDirectory::ChangeDirectory()
{
    name = "cd";
    description = QObject::tr("Change the current group. With no argument, go to the root.");
    optionalArguments.append({"group", QObject::tr("Absolute or relative group path, or - for the previous group."), "[group]"});
}

int ChangeDirectory::execute(const QStringList& arguments)
{
    const auto parser = getCommandLineParser(arguments);
    if (!parser) {
        return EXIT_FAILURE;
    }
    if (!workingGroup) {
        Utils::STDERR << QObject::tr("No database is open.") << Qt::endl;
        return EXIT_FAILURE;
    }

    const auto target = parser->positionalArguments().value(0, "/");
    const auto path = Utils::absoluteDatabasePath(*workingGroup, target);
    auto* group = target == "-" ? m_previousGroup.data() : currentDatabase->rootGroup()->findGroupByPath(path);
    if (target == "-" && (!group || group->database() != currentDatabase.data())) {
        Utils::STDERR << QObject::tr("No previous group in this database.") << Qt::endl;
        return EXIT_FAILURE;
    }
    if (!group) {
        Utils::STDERR << QObject::tr("Group %1 not found.").arg(path) << Qt::endl;
        return EXIT_FAILURE;
    }
    if (target == "-") {
        Utils::STDOUT << Utils::groupPath(*group) << Qt::endl;
    }
    if (group != workingGroup) {
        m_previousGroup = workingGroup;
        workingGroup = group;
    }
    return EXIT_SUCCESS;
}

PrintWorkingDirectory::PrintWorkingDirectory()
{
    name = "pwd";
    description = QObject::tr("Print the path of the current group.");
}

int PrintWorkingDirectory::execute(const QStringList& arguments)
{
    if (!getCommandLineParser(arguments)) {
        return EXIT_FAILURE;
    }
    if (!workingGroup) {
        Utils::STDERR << QObject::tr("No database is open.") << Qt::endl;
        return EXIT_FAILURE;
    }
    Utils::STDOUT << Utils::groupPath(*workingGroup) << Qt::endl;
    return EXIT_SUCCESS;
}
