#include "LineReader.h"
#ifdef USE_READLINE
#include <QRegularExpression>

#include <cstdlib>
#include <readline/history.h>
#include <readline/readline.h>

// The macOS SDK also provides libedit's smaller Readline compatibility API.
#ifdef RL_VERSION_MAJOR
#include <QScopedValueRollback>

#include <cstdio>

namespace
{
    struct ReadlineCompletion
    {
        const LineReader::Complete& complete;
        LineReader::Completion result{};
        int start = 0;
        int end = 0;
        qsizetype selected = 0;
        bool cycle = false;
    };

    ReadlineCompletion* completion = nullptr;

    QByteArray quoteArgument(const QString& argument, bool unfinished)
    {
        auto escaped = argument.toUtf8();
        escaped.replace("\\", "\\\\").replace("\"", "\\\"");
        if (unfinished) {
            escaped.replace(" ", "\\ ").replace("\t", "\\\t");
            if (escaped.startsWith('\'')) {
                escaped.prepend('\\');
            }
        } else if (escaped.startsWith('\'') || escaped.contains(' ') || escaped.contains('\t')) {
            escaped.prepend('"').append('"');
        }
        return escaped;
    }

    void insertMatch(ReadlineCompletion& state, const QString& match, bool complete = true)
    {
        const bool unfinished = !complete || (state.result.continuePath && match.endsWith('/'));
        auto replacement = quoteArgument(match, unfinished);
        if (state.end == rl_end && !unfinished) {
            replacement.append(' ');
        }
        rl_begin_undo_group();
        rl_delete_text(state.start, state.end);
        rl_point = state.start;
        rl_insert_text(replacement.constData());
        rl_end_undo_group();
        state.end = rl_point;
    }

    int completeLine(int, int)
    {
        auto& state = *completion;
        if (rl_last_func == completeLine && state.cycle) {
            insertMatch(state, state.result.matches.at(state.selected));
            state.selected = (state.selected + 1) % state.result.matches.size();
            return 0;
        }

        const auto line = QString::fromUtf8(rl_line_buffer, rl_end);
        const auto cursor = QString::fromUtf8(rl_line_buffer, rl_point).size();
        state.result = state.complete(line, cursor);
        state.cycle = false;
        const auto& matches = state.result.matches;
        if (matches.isEmpty()) {
            rl_ding();
            return 0;
        }

        state.start = static_cast<int>(line.left(state.result.start).toUtf8().size());
        state.end = static_cast<int>(line.left(state.result.end).toUtf8().size());
        state.selected = 0;

        if (matches.size() == 1) {
            insertMatch(state, matches.first());
            return 0;
        }

        auto common = matches.first();
        for (const auto& match : matches) {
            while (!match.startsWith(common)) {
                common.chop(1);
            }
        }
        if (!common.isEmpty() && common.at(common.size() - 1).isHighSurrogate()) {
            common.chop(1);
        }
        if (common.size() > state.result.prefix.size() && common.startsWith(state.result.prefix, Qt::CaseInsensitive)) {
            insertMatch(state, common, false);
            return 0;
        }

        state.cycle = true;
        rl_crlf();
        constexpr qsizetype maxDisplayed = 20;
        for (qsizetype i = 0; i < qMin(matches.size(), maxDisplayed); ++i) {
            std::fprintf(rl_outstream, "  %s\n", matches.at(i).toUtf8().constData());
        }
        if (matches.size() > maxDisplayed) {
            std::fprintf(rl_outstream, "  ... (%lld)\n", static_cast<long long>(matches.size()));
        }
        rl_on_new_line();
        rl_redisplay();
        return 0;
    }
} // namespace
#endif
#else
#include "TextStream.h"
#endif

QString LineReader::readLine(const QString& prompt, const Complete& complete)
{
#ifdef USE_READLINE
#ifdef RL_VERSION_MAJOR
    rl_initialize();
    rl_bind_key('\t', completeLine);
    ReadlineCompletion state{complete};
    const QScopedValueRollback<ReadlineCompletion*> activeCompletion(completion, &state);
#else
    Q_UNUSED(complete);
#endif
    // Readline must exclude ANSI styling from the prompt's display width.
    static const QRegularExpression ansiStyle("(\\x1b\\[[0-9;]*m)");
    auto displayPrompt = prompt;
    displayPrompt.replace(ansiStyle, QStringLiteral("\x01\\1\x02"));
    char* result = readline(displayPrompt.toUtf8().constData());
    if (!result) {
        m_finished = true;
        return {};
    }
    add_history(result);
    const auto line = QString::fromUtf8(result);
    std::free(result);
    return line;
#else
    Q_UNUSED(complete);
    static TextStream outStream(stdout, QIODevice::WriteOnly), inStream(stdin, QIODevice::ReadOnly);
    outStream << prompt;
    outStream.flush();
    QString result = inStream.readLine();
    if (result.isNull()) {
        m_finished = true;
    }
    return result;
#endif
}

bool LineReader::isFinished() const
{
    return m_finished;
}
