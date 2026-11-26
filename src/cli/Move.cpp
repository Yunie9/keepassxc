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

#include "Move.h"

#include "Utils.h"
#include "core/Global.h"
#include "core/Group.h"
#include "core/Metadata.h"

#include <QCommandLineParser>

Move::Move(bool interactive)
{
    name = QString("mv");
    if (interactive) {
        description = QObject::tr("Move or rename an entry or group.");
        positionalArguments.append({"path", QObject::tr("Path of the entry or group to move."), ""});
        positionalArguments.append({"destination", QObject::tr("Destination path."), ""});
    } else {
        description = QObject::tr("Moves an entry to a new group.");
        positionalArguments.append({"entry", QObject::tr("Path of the entry to move."), ""});
        positionalArguments.append({"group", QObject::tr("Path of the destination group."), ""});
    }
}

Move::~Move() = default;

int Move::executeWithDatabase(QSharedPointer<Database> database, QSharedPointer<QCommandLineParser> parser)
{
    auto& out = Utils::STDOUT;
    auto& err = Utils::STDERR;

    const QStringList args = parser->positionalArguments();
    if (workingGroup) {
        return moveInSession(*database, args.at(1), args.at(2));
    }
    const QString& entryPath = args.at(1);
    const QString& destinationPath = args.at(2);

    Entry* entry = database->rootGroup()->findEntryByPath(entryPath);
    if (!entry) {
        err << QObject::tr("Could not find entry with path %1.").arg(entryPath) << Qt::endl;
        return EXIT_FAILURE;
    }

    Group* destinationGroup = database->rootGroup()->findGroupByPath(destinationPath);
    if (!destinationGroup) {
        err << QObject::tr("Could not find group with path %1.").arg(destinationPath) << Qt::endl;
        return EXIT_FAILURE;
    }

    if (destinationGroup == entry->parent()) {
        err << QObject::tr("Entry is already in group %1.").arg(destinationPath) << Qt::endl;
        return EXIT_FAILURE;
    }

    entry->beginUpdate();
    entry->setGroup(destinationGroup);
    entry->endUpdate();

    QString errorMessage;
    if (!database->save(Database::Atomic, {}, &errorMessage)) {
        err << QObject::tr("Writing the database failed %1.").arg(errorMessage) << Qt::endl;
        return EXIT_FAILURE;
    }

    out << QObject::tr("Successfully moved entry %1 to group %2.").arg(entry->title(), destinationPath) << Qt::endl;
    return EXIT_SUCCESS;
}

int Move::moveInSession(Database& database, const QString& source, const QString& target)
{
    auto& err = Utils::STDERR;
    auto* root = database.rootGroup();
    const auto sourcePath = databasePath(source);
    const auto targetPath = databasePath(target);
    auto* entry = source.endsWith('/') ? nullptr : root->findEntryByPath(sourcePath);
    auto* group = entry ? nullptr : root->findGroupByPath(sourcePath);
    if (!entry && (!group || group == root || group == database.metadata()->recycleBin())) {
        err << QObject::tr("Cannot move %1.").arg(sourcePath) << Qt::endl;
        return EXIT_FAILURE;
    }

    auto* destination = root->findGroupByPath(targetPath);
    auto name = entry ? entry->title() : group->name();
    if (!destination && !target.endsWith('/')) {
        const auto separator = targetPath.lastIndexOf('/');
        destination = root->findGroupByPath(targetPath.left(separator + 1));
        name = targetPath.mid(separator + 1);
    }
    if (!destination) {
        err << QObject::tr("Destination group for %1 not found.").arg(targetPath) << Qt::endl;
        return EXIT_FAILURE;
    }

    for (auto* parent = destination; parent; parent = parent->parentGroup()) {
        if (parent == group) {
            err << QObject::tr("Cannot move a group into itself or one of its descendants.") << Qt::endl;
            return EXIT_FAILURE;
        }
    }

    auto destinationPath = Utils::groupPath(*destination);
    if (!destinationPath.endsWith('/')) {
        destinationPath.append('/');
    }
    destinationPath.append(name);
    if (destinationPath == sourcePath) {
        return EXIT_SUCCESS;
    }
    if (root->findGroupByPath(destinationPath) || root->findEntryByPath(destinationPath)) {
        err << QObject::tr("Destination %1 already exists.").arg(destinationPath) << Qt::endl;
        return EXIT_FAILURE;
    }

    if (entry) {
        entry->beginUpdate();
        entry->setTitle(name);
        entry->setGroup(destination);
        entry->endUpdate();
    } else {
        group->setName(name);
        group->setParent(destination);
    }

    QString errorMessage;
    if (!database.save(Database::Atomic, {}, &errorMessage)) {
        err << QObject::tr("Writing the database failed %1.").arg(errorMessage) << Qt::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
