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

#include "Show.h"

#include "Clip.h"
#include "Utils.h"
#include "core/Group.h"
#include "core/Tools.h"

#include <QCommandLineParser>

namespace
{
    const QCommandLineOption CopyOption({"c", "copy"}, QObject::tr("Copy the password to the clipboard."));
    const QCommandLineOption CopyAttributeOption(
        QStringLiteral("copy-attribute"),
        QObject::tr("Copy the given attribute to the clipboard (use totp for TOTP)."),
        "attribute");
    const QCommandLineOption TimeoutOption(
        QStringLiteral("timeout"),
        QObject::tr("Clipboard clear timeout in seconds (default: %1, 0 for unlimited).").arg(Clip::DefaultTimeout),
        "seconds");
} // namespace

const QCommandLineOption Show::TotpOption =
    QCommandLineOption(QStringList() << "t" << "totp", QObject::tr("Show the entry's current TOTP."));

const QCommandLineOption Show::ProtectedAttributesOption =
    QCommandLineOption(QStringList() << "s" << "show-protected",
                       QObject::tr("Show the protected attributes in clear text."));

const QCommandLineOption Show::AllAttributesOption =
    QCommandLineOption(QStringList() << "all", QObject::tr("Show all the attributes of the entry."));

const QCommandLineOption Show::AttachmentsOption =
    QCommandLineOption(QStringList() << "show-attachments", QObject::tr("Show the attachments of the entry."));

const QCommandLineOption Show::AttributesOption = QCommandLineOption(
    QStringList() << "a" << "attributes",
    QObject::tr(
        "Names of the attributes to show. "
        "This option can be specified more than once, with each attribute shown one-per-line in the given order. "
        "If no attributes are specified, a summary of the default attributes is given."),
    QObject::tr("attribute"));

Show::Show()
{
    name = QString("show");
    description = QObject::tr("Show an entry's information.");
    options.append(Show::TotpOption);
    options.append(Show::AttributesOption);
    options.append(Show::ProtectedAttributesOption);
    options.append(Show::AllAttributesOption);
    options.append(Show::AttachmentsOption);
    options.append(CopyOption);
    options.append(CopyAttributeOption);
    options.append(TimeoutOption);
    positionalArguments.append({QString("entry"), QObject::tr("Name of the entry to show."), QString("")});
}

int Show::executeWithDatabase(QSharedPointer<Database> database, QSharedPointer<QCommandLineParser> parser)
{
    auto& out = Utils::STDOUT;
    auto& err = Utils::STDERR;
    const bool colors = workingGroup && Utils::terminalColorsEnabled();
    const auto fieldLabel = [colors](const QString& name) {
        return Utils::colorize(name + ':', Utils::TerminalColor::Cyan, colors) + ' ';
    };

    const bool copyAttribute = parser->isSet(CopyAttributeOption);
    const bool copy = parser->isSet(CopyOption) || copyAttribute;
    auto timeout = Clip::DefaultTimeout;
    if (parser->isSet(TimeoutOption)) {
        if (!copy) {
            err << QObject::tr("--timeout requires --copy or --copy-attribute.") << Qt::endl;
            return EXIT_FAILURE;
        }
        const auto timeoutValue = parser->value(TimeoutOption);
        bool ok;
        timeout = timeoutValue.toInt(&ok);
        if (!ok) {
            err << QObject::tr("Invalid timeout value %1.").arg(timeoutValue) << Qt::endl;
            return EXIT_FAILURE;
        }
    }

    const QStringList args = parser->positionalArguments();
    const auto entryPath = databasePath(args.at(1));
    bool showTotp = parser->isSet(Show::TotpOption);
    bool showProtectedAttributes = parser->isSet(Show::ProtectedAttributesOption);
    bool showAllAttributes = parser->isSet(Show::AllAttributesOption);
    QStringList attributes = parser->values(Show::AttributesOption);

    Entry* entry = database->rootGroup()->findEntryByPath(entryPath);
    if (!entry) {
        err << QObject::tr("Could not find entry with path %1.").arg(entryPath) << Qt::endl;
        return EXIT_FAILURE;
    }

    if (showTotp && !entry->hasTotp()) {
        err << QObject::tr("Entry with path %1 has no TOTP set up.").arg(entryPath) << Qt::endl;
        return EXIT_FAILURE;
    }

    bool attributesWereSpecified = true;
    if (showAllAttributes) {
        attributesWereSpecified = false;
        attributes = EntryAttributes::DefaultAttributes;
        for (QString fieldName : Utils::EntryFieldNames) {
            attributes.append(fieldName);
        }
        // Adding the custom attributes after the default attributes so that
        // the default attributes are always shown first.
        for (QString attributeName : entry->attributes()->keys()) {
            if (EntryAttributes::DefaultAttributes.contains(attributeName)) {
                continue;
            }
            attributes.append(attributeName);
        }
    } else if (attributes.isEmpty() && !showTotp) {
        // If no attributes are specified, output the default attribute set.
        attributesWereSpecified = false;
        attributes = EntryAttributes::DefaultAttributes;
        for (QString fieldName : Utils::EntryFieldNames) {
            attributes.append(fieldName);
        }
    }

    // Iterate over the attributes and output them line-by-line.
    bool encounteredError = false;
    for (const QString& attributeName : asConst(attributes)) {
        if (Utils::EntryFieldNames.contains(attributeName)) {
            if (!attributesWereSpecified) {
                out << fieldLabel(attributeName);
            }
            out << Utils::getTopLevelField(entry, attributeName) << Qt::endl;
            continue;
        }

        QStringList attrs = Utils::findAttributes(*entry->attributes(), attributeName);
        if (attrs.isEmpty()) {
            encounteredError = true;
            err << QObject::tr("ERROR: unknown attribute %1.").arg(attributeName) << Qt::endl;
            continue;
        } else if (attrs.size() > 1) {
            encounteredError = true;
            err << QObject::tr("ERROR: attribute %1 is ambiguous, it matches %2.")
                       .arg(attributeName, QLocale().createSeparatedList(attrs))
                << Qt::endl;
            continue;
        }
        QString canonicalName = attrs[0];
        if (!attributesWereSpecified) {
            out << fieldLabel(canonicalName);
        }
        if (entry->attributes()->isProtected(canonicalName) && !attributesWereSpecified && !showProtectedAttributes) {
            out << Utils::colorize("PROTECTED", Utils::TerminalColor::Yellow, colors) << Qt::endl;
        } else {
            out << entry->resolveMultiplePlaceholders(entry->attributes()->value(canonicalName)) << Qt::endl;
        }
    }

    if (parser->isSet(Show::AttachmentsOption)) {
        // Separate attachment output from attributes output via a newline.
        out << Qt::endl;

        EntryAttachments* attachments = entry->attachments();
        if (attachments->isEmpty()) {
            out << QObject::tr("No attachments present.") << Qt::endl;
        } else {
            out << Utils::colorize(QObject::tr("Attachments:"), Utils::TerminalColor::Cyan, colors) << Qt::endl;

            // Iterate over the attachments and output their names and size line-by-line, indented.
            for (const QString& attachmentName : attachments->keys()) {
                // TODO: use QLocale::formattedDataSize when >= Qt 5.10
                QString attachmentSize = Tools::humanReadableFileSize(attachments->value(attachmentName).size(), 1);
                out << "  " << attachmentName << " (" << attachmentSize << ")" << Qt::endl;
            }
        }
    }

    if (showTotp) {
        out << entry->totp() << Qt::endl;
    }

    if (encounteredError) {
        return EXIT_FAILURE;
    }
    if (copy) {
        const auto attribute = copyAttribute ? parser->value(CopyAttributeOption) : QStringLiteral("password");
        return Clip::copyAttribute(
            *entry, entryPath, attribute, timeout, workingGroup, parser->isSet(Command::QuietOption));
    }
    return EXIT_SUCCESS;
}
