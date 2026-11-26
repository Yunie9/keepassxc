#include "InteractiveCompletion.h"

#include <QDir>

#include "Command.h"
#include "DatabaseCommand.h"
#include "Utils.h"
#include "core/Entry.h"
#include "core/EntryAttributes.h"
#include "core/Group.h"

#include <algorithm>

namespace
{
    QStringList fileMatches(const QString& prefix)
    {
        const auto directory = prefix.left(prefix.lastIndexOf('/') + 1);
        const auto filename = prefix.mid(directory.size());
        auto filters = QDir::AllEntries | QDir::NoDotAndDotDot;
        if (filename.startsWith('.')) {
            filters |= QDir::Hidden;
        }

        QStringList matches;
        for (const auto& file : QDir(directory.isEmpty() ? "." : directory).entryInfoList(filters)) {
            if (file.fileName().startsWith(filename)) {
                matches.append(directory + file.fileName() + (file.isDir() ? "/" : ""));
            }
        }
        return matches;
    }

    QStringList databaseMatches(Group& workingGroup, const QString& prefix, bool includeEntries)
    {
        const auto separator = prefix.lastIndexOf('/') + 1;
        const auto directory = prefix.left(separator);
        const auto name = prefix.mid(separator);
        auto* group = workingGroup.database()->rootGroup()->findGroupByPath(
            Utils::absoluteDatabasePath(workingGroup, directory));
        if (!group) {
            return {};
        }

        QStringList matches;
        const auto addMatch = [&](const QString& candidate, bool isDirectory) {
            if (candidate.startsWith(name, Qt::CaseInsensitive)) {
                auto path = directory + candidate;
                if (path.startsWith('-')) {
                    path.prepend("./");
                }
                matches.append(path + (isDirectory ? "/" : ""));
            }
        };
        if (name.startsWith('.')) {
            addMatch(".", true);
            addMatch("..", true);
        }
        for (const auto* child : group->children()) {
            addMatch(child->name(), true);
        }
        if (includeEntries) {
            for (const auto* entry : group->entries()) {
                addMatch(entry->title(), false);
            }
        }
        return matches;
    }

    QString valueOption(const QList<QCommandLineOption>& options, const QString& word)
    {
        for (const auto& option : options) {
            if (option.valueName().isEmpty()) {
                continue;
            }
            if (option.names().contains(word)) {
                return option.names().last();
            }
        }
        return {};
    }

    bool printable(const QString& match)
    {
        return std::none_of(match.begin(), match.end(), [](QChar c) {
            return c.category() == QChar::Other_Control || c.category() == QChar::Separator_Line
                   || c.category() == QChar::Separator_Paragraph;
        });
    }
} // namespace

LineReader::Completion completeInteractiveCommand(const QString& line, qsizetype cursor, Group* workingGroup)
{
    LineReader::Completion result{cursor, cursor, {}};
    QStringList words;
    for (const auto& token : Utils::tokenizeCommand(line)) {
        if (token.end < cursor) {
            if (!token.value.isEmpty()) {
                words.append(token.value);
            }
        } else {
            if (token.start <= cursor) {
                result.start = token.start;
                result.end = token.end;
            }
            break;
        }
    }
    const auto prefix = Utils::splitCommandString(line.mid(result.start, cursor - result.start)).value(0);
    result.prefix = prefix;
    if (words.isEmpty()) {
        for (const auto& command : Commands::getCommands()) {
            if (command->name.startsWith(prefix)) {
                result.matches.append(command->name);
            }
        }
        return result;
    }

    const auto command = Commands::getCommand(words.takeFirst());
    if (!command) {
        return result;
    }

    auto options = command->options;
    options.append(Command::HelpOption);
    const bool usesOpenDatabase =
        workingGroup && command->name != "open" && dynamic_cast<const DatabaseCommand*>(command.data());
    if (usesOpenDatabase) {
        options.removeIf([](const auto& option) {
            return option.names().contains("key-file") || option.names().contains("yubikey")
                   || option.names().contains("no-password");
        });
    }

    qsizetype position = usesOpenDatabase ? 1 : 0;
    bool endOfOptions = false;
    QString optionValue;
    QString entryPath;
    for (const auto& word : words) {
        if (!optionValue.isEmpty()) {
            optionValue.clear();
        } else if (!endOfOptions && word == "--") {
            endOfOptions = true;
        } else if (!endOfOptions && word.startsWith("--")) {
            if (!word.contains('=')) {
                optionValue = valueOption(options, word.mid(2));
            }
        } else if (!endOfOptions && word.startsWith('-') && word.size() > 1) {
            for (qsizetype i = 1; i < word.size(); ++i) {
                const auto name = valueOption(options, word.mid(i, 1));
                if (!name.isEmpty()) {
                    if (i == word.size() - 1) {
                        optionValue = name;
                    }
                    break;
                }
            }
        } else {
            if (usesOpenDatabase && position == 1 && command->name == "show") {
                entryPath = word;
            }
            ++position;
        }
    }

    QString valuePrefix = prefix;
    QString optionPrefix;
    if (!endOfOptions && optionValue.isEmpty() && prefix.startsWith("--") && prefix.contains('=')) {
        const auto equals = prefix.indexOf('=');
        optionValue = valueOption(options, prefix.mid(2, equals - 2));
        optionPrefix = prefix.left(equals + 1);
        valuePrefix = prefix.mid(equals + 1);
    }

    if (optionValue == "attribute" || optionValue == "attributes" || optionValue == "copy-attribute") {
        auto fields = EntryAttributes::DefaultAttributes + Utils::EntryFieldNames;
        if (optionValue != "attributes") {
            fields.append("totp");
        }
        if (optionValue == "copy-attribute" && !entryPath.isEmpty()) {
            const auto path = Utils::absoluteDatabasePath(*workingGroup, entryPath);
            const auto* entry = workingGroup->database()->rootGroup()->findEntryByPath(path);
            if (entry) {
                fields.append(entry->attributes()->keys());
            }
        }
        for (const auto& field : fields) {
            if (field.startsWith(valuePrefix, Qt::CaseInsensitive)) {
                result.matches.append(field);
            }
        }
    } else if (optionValue.endsWith("key-file") || optionValue == "key-file-from") {
        result.continuePath = true;
        result.matches = fileMatches(valuePrefix);
    } else if (!optionValue.isEmpty()) {
        return result;
    } else if (!endOfOptions && prefix.startsWith('-')) {
        for (const auto& option : options) {
            for (const auto& name : option.names()) {
                const auto flag = (name.size() == 1 ? "-" : "--") + name;
                if (flag.startsWith(prefix)) {
                    result.matches.append(flag);
                }
            }
        }
    } else {
        const auto arguments = command->positionalArguments + command->optionalArguments;
        if (position < arguments.size()) {
            const auto& type = arguments.at(position).name;
            if (workingGroup && (type == "entry" || type == "group" || type == "path" || type == "destination")) {
                result.continuePath = true;
                const bool includeEntries = type == "path" || (type == "entry" && command->name != "add");
                result.matches = databaseMatches(*workingGroup, prefix, includeEntries);
            } else if (type == "database" || type == "database2" || type == "import-file" || type == "export-file") {
                result.continuePath = true;
                result.matches = fileMatches(prefix);
            }
        }
    }

    result.matches.removeIf([](const auto& match) { return !printable(match); });
    for (auto& match : result.matches) {
        match.prepend(optionPrefix);
    }
    result.matches.removeDuplicates();
    result.matches.sort(Qt::CaseInsensitive);
    return result;
}
