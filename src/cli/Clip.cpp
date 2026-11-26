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

#include "Clip.h"

#include "Utils.h"
#include "core/EntrySearcher.h"
#include "core/Group.h"
#include "core/Tools.h"

#include <QCommandLineParser>
#include <QDeadlineTimer>

#ifdef Q_OS_UNIX
#include <cerrno>
#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace
{
    bool acceptsEnterToClear(const Group* workingGroup)
    {
#ifdef Q_OS_UNIX
        termios settings{};
        return workingGroup && tcgetattr(STDIN_FILENO, &settings) == 0 && (settings.c_lflag & ICANON);
#else
        Q_UNUSED(workingGroup);
        return false;
#endif
    }

    bool waitForClearKey(bool& acceptsEnter)
    {
#ifdef Q_OS_UNIX
        if (acceptsEnter) {
            const QDeadlineTimer tick(1000);
            pollfd input{STDIN_FILENO, POLLIN, 0};
            int ready;
            do {
                ready = poll(&input, 1, static_cast<int>(tick.remainingTime()));
            } while (ready < 0 && errno == EINTR && !tick.hasExpired());

            if (ready == 0) {
                return false;
            }

            // In canonical mode, a bare Enter is one available byte. Leave any
            // queued command intact so the next prompt can read it.
            int pending = 0;
            char key = 0;
            if (ready > 0 && (input.revents & POLLIN) && ioctl(STDIN_FILENO, FIONREAD, &pending) == 0 && pending == 1
                && read(STDIN_FILENO, &key, 1) == 1 && (key == '\n' || key == '\r')) {
                return true;
            }

            acceptsEnter = false;
            Tools::sleep(static_cast<int>(tick.remainingTime()));
            return false;
        }
#else
        Q_UNUSED(acceptsEnter);
#endif
        Tools::sleep(1000);
        return false;
    }
} // namespace

const QCommandLineOption Clip::AttributeOption = QCommandLineOption(
    QStringList() << "a" << "attribute",
    QObject::tr("Copy the given attribute to the clipboard. Defaults to \"password\" if not specified.",
                "Don't translate \"password\", it refers to the attribute."),
    "attr",
    "password");

const QCommandLineOption Clip::TotpOption =
    QCommandLineOption(QStringList() << "t" << "totp",
                       QObject::tr("Copy the current TOTP to the clipboard (equivalent to \"-a totp\")."));

const QCommandLineOption Clip::BestMatchOption =
    QCommandLineOption(QStringList() << "b" << "best-match",
                       QObject::tr("Must match only one entry, otherwise a list of possible matches is shown."));

Clip::Clip()
{
    name = QString("clip");
    description = QObject::tr("Copy an entry's attribute to the clipboard.");
    options.append(Clip::AttributeOption);
    options.append(Clip::TotpOption);
    options.append(Clip::BestMatchOption);
    positionalArguments.append(
        {QString("entry"), QObject::tr("Path of the entry to clip.", "clip = copy to clipboard"), QString("")});
    optionalArguments.append(
        {QString("timeout"),
         QObject::tr("Timeout before clearing the clipboard (default is %1 seconds, set to 0 for unlimited).")
             .arg(DefaultTimeout),
         QString("[timeout]")});
}

int Clip::executeWithDatabase(QSharedPointer<Database> database, QSharedPointer<QCommandLineParser> parser)
{
    auto& out = parser->isSet(Command::QuietOption) ? Utils::DEVNULL : Utils::STDOUT;
    auto& err = Utils::STDERR;

    const QStringList args = parser->positionalArguments();

    auto timeout = DefaultTimeout;
    if (args.size() == 3) {
        bool ok;
        timeout = args.at(2).toInt(&ok);
        if (!ok) {
            err << QObject::tr("Invalid timeout value %1.").arg(args.at(2)) << Qt::endl;
            return EXIT_FAILURE;
        }
    }

    const auto& searchTerm = args.at(1);
    auto entryPath = databasePath(searchTerm);
    if (parser->isSet(Clip::BestMatchOption) && (!workingGroup || !searchTerm.contains('/'))) {
        EntrySearcher searcher;
        const auto* group = workingGroup ? workingGroup : database->rootGroup();
        const auto results = searcher.search(QString("title:%1").arg(searchTerm), group, true);
        if (results.count() > 1) {
            err << QObject::tr("Multiple entries matching:") << Qt::endl;
            for (const Entry* result : results) {
                err << result->path().prepend('/') << Qt::endl;
            }
            return EXIT_FAILURE;
        } else {
            if (!results.isEmpty()) {
                entryPath = results[0]->path().prepend('/');
            }
            out << QObject::tr("Using matching entry: %1").arg(entryPath) << Qt::endl;
        }
    }

    auto* entry = database->rootGroup()->findEntryByPath(entryPath);
    if (!entry) {
        err << QObject::tr("Entry %1 not found.").arg(entryPath) << Qt::endl;
        return EXIT_FAILURE;
    }

    if (parser->isSet(AttributeOption) && parser->isSet(TotpOption)) {
        err << QObject::tr("ERROR: Please specify one of --attribute or --totp, not both.") << Qt::endl;
        return EXIT_FAILURE;
    }

    const auto attribute = parser->isSet(TotpOption) ? QStringLiteral("totp") : parser->value(AttributeOption);
    return copyAttribute(*entry, entryPath, attribute, timeout, workingGroup, parser->isSet(Command::QuietOption));
}

int Clip::copyAttribute(const Entry& entry,
                        const QString& entryPath,
                        QString selectedAttribute,
                        int timeout,
                        const Group* workingGroup,
                        bool quiet)
{
    auto& out = quiet ? Utils::DEVNULL : Utils::STDOUT;
    auto& err = Utils::STDERR;
    const bool colors = workingGroup && Utils::terminalColorsEnabled();
    QString value;
    bool found = false;
    if (selectedAttribute == "totp") {
        if (!entry.hasTotp()) {
            err << QObject::tr("Entry with path %1 has no TOTP set up.").arg(entryPath) << Qt::endl;
            return EXIT_FAILURE;
        }

        found = true;
        value = entry.totp();
    } else if (Utils::EntryFieldNames.contains(selectedAttribute)) {
        value = Utils::getTopLevelField(&entry, selectedAttribute);
        found = true;
    } else {
        QStringList attrs = Utils::findAttributes(*entry.attributes(), selectedAttribute);
        if (attrs.size() > 1) {
            err << QObject::tr("ERROR: attribute %1 is ambiguous, it matches %2.")
                       .arg(selectedAttribute, QLocale().createSeparatedList(attrs))
                << Qt::endl;
            return EXIT_FAILURE;
        } else if (attrs.size() == 1) {
            found = true;
            selectedAttribute = attrs[0];
            value = entry.attributes()->value(selectedAttribute);
        }
    }

    if (!found) {
        out << QObject::tr("Attribute \"%1\" not found.").arg(selectedAttribute) << Qt::endl;
        return EXIT_FAILURE;
    }

    int exitCode = Utils::clipText(value);
    if (exitCode != EXIT_SUCCESS) {
        return exitCode;
    }

    out << Utils::colorize(QObject::tr("Entry's \"%1\" attribute copied to the clipboard!").arg(selectedAttribute),
                          Utils::TerminalColor::BrightGreen,
                          colors)
        << Qt::endl;

    if (timeout <= 0) {
        return exitCode;
    }

    bool acceptsEnter = acceptsEnterToClear(workingGroup);
    QString lastLine;
    while (timeout > 0) {
        out << '\r' << QString(lastLine.size(), ' ') << '\r';
        lastLine = QObject::tr("Clearing the clipboard in %1 second(s)...", "", timeout).arg(timeout);
        if (acceptsEnter) {
            lastLine += ' ' + QObject::tr("Press Enter to clear now.");
        }
        out << Utils::colorize(lastLine, Utils::TerminalColor::Yellow, colors) << Qt::flush;
        if (waitForClearKey(acceptsEnter)) {
            break;
        }
        --timeout;
    }
    exitCode = Utils::clipText("");
    out << '\r' << QString(lastLine.size(), ' ') << '\r';
    if (exitCode == EXIT_SUCCESS) {
        out << Utils::colorize(QObject::tr("Clipboard cleared!"), Utils::TerminalColor::BrightGreen, colors);
    }
    out << Qt::endl;

    return exitCode;
}
