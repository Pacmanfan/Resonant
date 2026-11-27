#include "CodeEditor.h"

#include "PythonHighlighter.h"

#include <QAbstractItemView>
#include <QCompleter>
#include <QFontDatabase>
#include <QKeyEvent>
#include <QPainter>
#include <QScrollBar>
#include <QStringListModel>
#include <QTextBlock>

namespace {
class LineNumberArea : public QWidget {
public:
    explicit LineNumberArea(CodeEditor* editor) : QWidget(editor), editor_(editor) {}
    QSize sizeHint() const override { return {editor_->lineNumberAreaWidth(), 0}; }

protected:
    void paintEvent(QPaintEvent* event) override { editor_->paintLineNumbers(event); }

private:
    CodeEditor* editor_;
};
}  // namespace

CodeEditor::CodeEditor(QWidget* parent) : QPlainTextEdit(parent)
{
    setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    setTabStopDistance(fontMetrics().horizontalAdvance(QLatin1Char(' ')) * 4);
    setLineWrapMode(QPlainTextEdit::NoWrap);
    lineNumberArea_ = new LineNumberArea(this);
    highlighter_ = new PythonHighlighter(document());
    completions_ = new QStringListModel(this);
    completer_ = new QCompleter(completions_, this);
    completer_->setWidget(this);
    completer_->setCompletionMode(QCompleter::PopupCompletion);
    completer_->setCaseSensitivity(Qt::CaseSensitive);
    connect(completer_, qOverload<const QString&>(&QCompleter::activated), this, &CodeEditor::insertCompletion);
    connect(this, &QPlainTextEdit::blockCountChanged, this, &CodeEditor::updateMargins);
    connect(this, &QPlainTextEdit::updateRequest, this, &CodeEditor::updateLineNumberArea);
    connect(this, &QPlainTextEdit::cursorPositionChanged, this, &CodeEditor::highlightLines);
    updateMargins();
    highlightLines();
}

void CodeEditor::setLineNumbersVisible(bool visible)
{
    lineNumbers_ = visible;
    lineNumberArea_->setVisible(visible);
    updateMargins();
}

int CodeEditor::lineNumberAreaWidth() const
{
    if (!lineNumbers_)
        return 0;
    int digits = 1;
    for (int max = std::max(1, blockCount()); max >= 10; max /= 10)
        ++digits;
    return 8 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * std::max(3, digits);
}

void CodeEditor::updateMargins()
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void CodeEditor::updateLineNumberArea(const QRect& rect, int dy)
{
    if (dy)
        lineNumberArea_->scroll(0, dy);
    else
        lineNumberArea_->update(0, rect.y(), lineNumberArea_->width(), rect.height());
    if (rect.contains(viewport()->rect()))
        updateMargins();
}

void CodeEditor::resizeEvent(QResizeEvent* event)
{
    QPlainTextEdit::resizeEvent(event);
    const QRect r = contentsRect();
    lineNumberArea_->setGeometry(QRect(r.left(), r.top(), lineNumberAreaWidth(), r.height()));
}

void CodeEditor::paintLineNumbers(QPaintEvent* event)
{
    QPainter painter(lineNumberArea_);
    painter.fillRect(event->rect(), QColor(0xf0, 0xf2, 0xf5));
    QTextBlock block = firstVisibleBlock();
    int number = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());
    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            painter.setPen(number + 1 == errorLine_ ? QColor(0xa1, 0x2a, 0x2a) : QColor(0x7a, 0x86, 0x94));
            painter.drawText(0, top, lineNumberArea_->width() - 4, fontMetrics().height(), Qt::AlignRight, QString::number(number + 1));
        }
        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++number;
    }
}

void CodeEditor::highlightLines()
{
    QList<QTextEdit::ExtraSelection> selections;
    if (!isReadOnly()) {
        QTextEdit::ExtraSelection current;
        current.format.setBackground(QColor(0xf4, 0xf7, 0xfb));
        current.format.setProperty(QTextFormat::FullWidthSelection, true);
        current.cursor = textCursor();
        current.cursor.clearSelection();
        selections.append(current);
    }
    if (errorLine_ > 0) {
        const QTextBlock block = document()->findBlockByNumber(errorLine_ - 1);
        if (block.isValid()) {
            QTextEdit::ExtraSelection error;
            error.format.setBackground(QColor(0xfb, 0xe3, 0xe3));
            error.format.setProperty(QTextFormat::FullWidthSelection, true);
            error.cursor = QTextCursor(block);
            selections.append(error);
        }
    }
    setExtraSelections(selections);
}

void CodeEditor::setErrorLine(int line)
{
    errorLine_ = line;
    highlightLines();
    lineNumberArea_->update();
}

void CodeEditor::goToLine(int line)
{
    const QTextBlock block = document()->findBlockByNumber(std::max(0, line - 1));
    if (!block.isValid())
        return;
    QTextCursor c(block);
    setTextCursor(c);
    centerCursor();
    setFocus();
}

QString CodeEditor::completionPrefix() const
{
    QTextCursor c = textCursor();
    const QString line = c.block().text().left(c.positionInBlock());
    int start = static_cast<int>(line.size());
    while (start > 0 && (line[start - 1].isLetterOrNumber() || line[start - 1] == QLatin1Char('_') || line[start - 1] == QLatin1Char('.')))
        --start;
    return line.mid(start);
}

void CodeEditor::showCompletions(const QStringList& items, const QString& prefix)
{
    if (prefix != completionPrefix() || items.isEmpty())
        return;
    if (items.size() == 1) {
        completionPrefix_ = prefix;
        insertCompletion(items.front());
        return;
    }
    completionPrefix_ = prefix;
    completions_->setStringList(items);
    completer_->setCompletionPrefix(prefix);
    QRect r = cursorRect();
    r.setWidth(completer_->popup()->sizeHintForColumn(0) + completer_->popup()->verticalScrollBar()->sizeHint().width() + 12);
    completer_->complete(r);
}

void CodeEditor::insertCompletion(const QString& completion)
{
    QTextCursor c = textCursor();
    c.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, static_cast<int>(completionPrefix_.size()));
    QString text = completion;
    if (text.endsWith(QLatin1Char('(')) && !text.isEmpty())
        text.chop(1);  // rlcompleter marks callables with "("; the user types it
    c.insertText(text);
    setTextCursor(c);
}

void CodeEditor::indentSelection(bool indent)
{
    QTextCursor c = textCursor();
    const int start = document()->findBlock(c.selectionStart()).blockNumber();
    const int end = document()->findBlock(c.selectionEnd()).blockNumber();
    c.beginEditBlock();
    for (int b = start; b <= end; ++b) {
        QTextCursor line(document()->findBlockByNumber(b));
        if (indent)
            line.insertText(QStringLiteral("    "));
        else {
            const QString text = line.block().text();
            int spaces = 0;
            while (spaces < 4 && spaces < text.size() && text[spaces] == QLatin1Char(' '))
                ++spaces;
            line.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, spaces);
            line.removeSelectedText();
        }
    }
    c.endEditBlock();
}

void CodeEditor::keyPressEvent(QKeyEvent* event)
{
    if (completer_->popup()->isVisible()) {
        switch (event->key()) {
        case Qt::Key_Enter:
        case Qt::Key_Return:
        case Qt::Key_Escape:
        case Qt::Key_Tab:
        case Qt::Key_Backtab:
            event->ignore();
            return;
        default:
            break;
        }
    }
    const bool shift = event->modifiers() & Qt::ShiftModifier;
    const bool ctrl = event->modifiers() & Qt::ControlModifier;
    if (event->key() == Qt::Key_Space && ctrl) {
        emit completionRequested(completionPrefix());
        return;
    }
    if (event->key() == Qt::Key_Tab && !ctrl) {
        const QString prefix = completionPrefix();
        if (!textCursor().hasSelection() && !prefix.isEmpty()) {
            emit completionRequested(prefix);
            return;
        }
        if (textCursor().hasSelection()) {
            indentSelection(true);
            return;
        }
        insertPlainText(QStringLiteral("    "));
        return;
    }
    if (event->key() == Qt::Key_Backtab) {
        indentSelection(false);
        return;
    }
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && !ctrl) {
        if (submitOnEnter_ && !shift) {
            emit submitted();
            return;
        }
        // Keep the indentation of the line; one level more after a colon.
        const QString line = textCursor().block().text().left(textCursor().positionInBlock());
        QString indent;
        for (QChar ch : line) {
            if (ch != QLatin1Char(' ') && ch != QLatin1Char('\t'))
                break;
            indent += ch;
        }
        if (line.trimmed().endsWith(QLatin1Char(':')))
            indent += QStringLiteral("    ");
        textCursor().insertText(QStringLiteral("\n") + indent);
        ensureCursorVisible();
        return;
    }
    if (event->key() == Qt::Key_Up && !textCursor().block().previous().isValid() && submitOnEnter_) {
        emit historyUp();
        return;
    }
    if (event->key() == Qt::Key_Down && !textCursor().block().next().isValid() && submitOnEnter_) {
        emit historyDown();
        return;
    }
    QPlainTextEdit::keyPressEvent(event);
    if (completer_->popup()->isVisible()) {
        const QString prefix = completionPrefix();
        if (prefix.isEmpty() || !prefix.startsWith(completionPrefix_.section(QLatin1Char('.'), 0, -2)))
            completer_->popup()->hide();
        else {
            completionPrefix_ = prefix;
            completer_->setCompletionPrefix(prefix);
        }
    }
}
