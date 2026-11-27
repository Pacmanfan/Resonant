#include "PythonHighlighter.h"

#include <QColor>
#include <QFont>

PythonHighlighter::PythonHighlighter(QTextDocument* document) : QSyntaxHighlighter(document)
{
    const auto format = [](const QColor& color, bool bold = false, bool italic = false) {
        QTextCharFormat f;
        f.setForeground(color);
        if (bold)
            f.setFontWeight(QFont::Bold);
        f.setFontItalic(italic);
        return f;
    };
    const QStringList keywords = {
        QStringLiteral("and"),    QStringLiteral("as"),     QStringLiteral("assert"), QStringLiteral("async"),  QStringLiteral("await"),
        QStringLiteral("break"),  QStringLiteral("class"),  QStringLiteral("continue"), QStringLiteral("def"),  QStringLiteral("del"),
        QStringLiteral("elif"),   QStringLiteral("else"),   QStringLiteral("except"), QStringLiteral("finally"), QStringLiteral("for"),
        QStringLiteral("from"),   QStringLiteral("global"), QStringLiteral("if"),     QStringLiteral("import"), QStringLiteral("in"),
        QStringLiteral("is"),     QStringLiteral("lambda"), QStringLiteral("nonlocal"), QStringLiteral("not"),  QStringLiteral("or"),
        QStringLiteral("pass"),   QStringLiteral("raise"),  QStringLiteral("return"), QStringLiteral("try"),    QStringLiteral("while"),
        QStringLiteral("with"),   QStringLiteral("yield"),  QStringLiteral("None"),   QStringLiteral("True"),   QStringLiteral("False")};
    const QStringList builtins = {QStringLiteral("print"), QStringLiteral("len"),   QStringLiteral("range"), QStringLiteral("abs"),
                                  QStringLiteral("min"),   QStringLiteral("max"),   QStringLiteral("sum"),   QStringLiteral("list"),
                                  QStringLiteral("dict"),  QStringLiteral("float"), QStringLiteral("int"),   QStringLiteral("str"),
                                  QStringLiteral("round"), QStringLiteral("zip"),   QStringLiteral("enumerate"), QStringLiteral("help"),
                                  QStringLiteral("dir"),   QStringLiteral("type"),  QStringLiteral("isinstance"), QStringLiteral("open")};
    rules_.push_back({QRegularExpression(QStringLiteral("\\b(%1)\\b").arg(keywords.join(QLatin1Char('|')))), format(QColor(0x1f, 0x4f, 0xb0), true)});
    rules_.push_back({QRegularExpression(QStringLiteral("\\b(%1)\\b").arg(builtins.join(QLatin1Char('|')))), format(QColor(0x6b, 0x3f, 0xa0))});
    rules_.push_back({QRegularExpression(QStringLiteral("\\b(?:def|class)\\s+([A-Za-z_][A-Za-z0-9_]*)")), format(QColor(0x0b, 0x6e, 0x6e), true), 1});
    rules_.push_back({QRegularExpression(QStringLiteral("\\b[0-9]+(?:\\.[0-9]*)?(?:[eE][-+]?[0-9]+)?j?\\b")), format(QColor(0x9a, 0x4b, 0x00))});
    rules_.push_back({QRegularExpression(QStringLiteral("^\\s*@[A-Za-z_][A-Za-z0-9_.]*")), format(QColor(0x8a, 0x5a, 0x00))});
    rules_.push_back({QRegularExpression(QStringLiteral("\\bresonant\\b")), format(QColor(0x13, 0x40, 0x6b), true)});
    string_ = format(QColor(0x1b, 0x6e, 0x2e));
    comment_ = format(QColor(0x6b, 0x77, 0x85), false, true);
}

void PythonHighlighter::highlightBlock(const QString& text)
{
    for (const Rule& rule : rules_) {
        auto it = rule.pattern.globalMatch(text);
        while (it.hasNext()) {
            const auto m = it.next();
            setFormat(static_cast<int>(m.capturedStart(rule.group)), static_cast<int>(m.capturedLength(rule.group)), rule.format);
        }
    }
    // Strings and comments last (they win over keywords inside them); a
    // triple-quoted string may span blocks (state 1 = inside ''' or """).
    int i = 0;
    const int n = static_cast<int>(text.size());
    if (previousBlockState() == 1) {
        const int end = text.indexOf(QStringLiteral("\"\"\""));
        const int end2 = text.indexOf(QStringLiteral("'''"));
        const int close = end >= 0 && (end2 < 0 || end < end2) ? end : end2;
        if (close < 0) {
            setFormat(0, n, string_);
            setCurrentBlockState(1);
            return;
        }
        setFormat(0, close + 3, string_);
        i = close + 3;
    }
    setCurrentBlockState(0);
    while (i < n) {
        const QChar c = text[i];
        if (c == QLatin1Char('#')) {
            setFormat(i, n - i, comment_);
            return;
        }
        if (c == QLatin1Char('"') || c == QLatin1Char('\'')) {
            const QString triple(3, c);
            if (text.mid(i, 3) == triple) {
                const int close = text.indexOf(triple, i + 3);
                if (close < 0) {
                    setFormat(i, n - i, string_);
                    setCurrentBlockState(1);
                    return;
                }
                setFormat(i, close + 3 - i, string_);
                i = close + 3;
                continue;
            }
            int j = i + 1;
            while (j < n && text[j] != c) {
                if (text[j] == QLatin1Char('\\'))
                    ++j;
                ++j;
            }
            setFormat(i, std::min(j + 1, n) - i, string_);
            i = j + 1;
            continue;
        }
        ++i;
    }
}
