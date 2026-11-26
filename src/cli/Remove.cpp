/*
 *  Copyright (C) 2019 KeePassXC Team <team@keepassxc.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 or (at your option)
 *  version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "Remove.h"

#include "RemoveGroup.h"
#include "Utils.h"
#include "core/Global.h"
#include "core/Group.h"
#include "core/Metadata.h"

#include <QCommandLineParser>

static const QCommandLineOption recursiveOption({"r", "R", "recursive"},
                                                QObject::tr("Remove a group and all its contents."));

Remove::Remove(bool interactive)
{
    name = QString("rm");
    description = QString("Remove an entry from the database.");
    if (interactive) {
        options.append(recursiveOption);
        positionalArguments.append({"path", QObject::tr("Path of the entry or group to remove."), ""});
    } else {
        positionalArguments.append({"entry", QObject::tr("Path of the entry to remove."), ""});
    }
}

int Remove::executeWithDatabase(QSharedPointer<Database> database, QSharedPointer<QCommandLineParser> parser)
{
    auto& out = parser->isSet(Command::QuietOption) ? Utils::DEVNULL : Utils::STDOUT;
    auto& err = Utils::STDERR;

    const auto source = parser->positionalArguments().at(1);
    const auto entryPath = databasePath(source);
    auto* root = database->rootGroup();
    QPointer<Entry> entry = workingGroup && source.endsWith('/') ? nullptr : root->findEntryByPath(entryPath);
    if (!entry) {
        if (workingGroup) {
            if (auto* group = root->findGroupByPath(entryPath)) {
                if (parser->isSet(recursiveOption)) {
                    return RemoveGroup::removeGroup(*database, group, entryPath, out);
                }
                err << QObject::tr("%1 is a group. Use rm -r to remove it recursively.").arg(entryPath) << Qt::endl;
                return EXIT_FAILURE;
            }
        }
        err << QObject::tr("Entry %1 not found.").arg(entryPath) << Qt::endl;
        return EXIT_FAILURE;
    }

    QString entryTitle = entry->title();
    bool recycled = true;
    auto* recycleBin = database->metadata()->recycleBin();
    if (!database->metadata()->recycleBinEnabled() || (recycleBin && recycleBin->findEntryByUuid(entry->uuid()))) {
        delete entry;
        recycled = false;
    } else {
        database->recycleEntry(entry);
    }

    QString errorMessage;
    if (!database->save(Database::Atomic, {}, &errorMessage)) {
        err << QObject::tr("Unable to save database to file: %1").arg(errorMessage) << Qt::endl;
        return EXIT_FAILURE;
    }

    if (recycled) {
        out << QObject::tr("Successfully recycled entry %1.").arg(entryTitle) << Qt::endl;
    } else {
        out << QObject::tr("Successfully deleted entry %1.").arg(entryTitle) << Qt::endl;
    }

    return EXIT_SUCCESS;
}
