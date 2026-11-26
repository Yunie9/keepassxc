#ifndef LINEREADER_H
#define LINEREADER_H

#include <QString>
#include <QStringList>

#include <functional>

class LineReader
{
    bool m_finished{false};

public:
    struct Completion
    {
        qsizetype start;
        qsizetype end;
        QStringList matches;
        bool continuePath = false;
        QString prefix{};
    };
    using Complete = std::function<Completion(const QString&, qsizetype)>;

    LineReader() = default;
    LineReader(const LineReader&) = delete;
    ~LineReader() = default;
    LineReader& operator=(const LineReader&) = delete;
    QString readLine(const QString& prompt, const Complete& complete);
    [[nodiscard]] bool isFinished() const;
};

#endif // LINEREADER_H
