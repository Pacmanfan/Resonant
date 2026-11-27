// A plain-text code editor for Python: line numbers (optional), current-line
// and error-line highlighting, automatic indentation, Tab / Shift+Tab
// indenting, and completion from a provider (the Python worker's names).
#pragma once

#include <QPlainTextEdit>

class QCompleter;
class QStringListModel;
class PythonHighlighter;

class CodeEditor : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit CodeEditor(QWidget* parent = nullptr);

    void setLineNumbersVisible(bool visible);
    bool lineNumbersVisible() const { return lineNumbers_; }
    // Marks a line (1-based) as the place of an error; 0 clears it.
    void setErrorLine(int line);
    int errorLine() const { return errorLine_; }
    void goToLine(int line);
    // The word before the cursor (with dots: "resonant.sce").
    QString completionPrefix() const;
    // Shows completions for the prefix they were requested for.
    void showCompletions(const QStringList& items, const QString& prefix);
    // Plain Enter submits instead of breaking the line (the console's input).
    void setSubmitOnEnter(bool submit) { submitOnEnter_ = submit; }

    int lineNumberAreaWidth() const;
    void paintLineNumbers(class QPaintEvent* event);

signals:
    void completionRequested(const QString& prefix);
    void submitted();         // Enter in a submit-on-enter editor
    void historyUp();         // Up on the first line
    void historyDown();       // Down on the last line

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void updateMargins();
    void updateLineNumberArea(const QRect& rect, int dy);
    void highlightLines();
    void insertCompletion(const QString& completion);
    void indentSelection(bool indent);

    QWidget* lineNumberArea_ = nullptr;
    PythonHighlighter* highlighter_ = nullptr;
    QCompleter* completer_ = nullptr;
    QStringListModel* completions_ = nullptr;
    QString completionPrefix_;
    bool lineNumbers_ = true;
    bool submitOnEnter_ = false;
    int errorLine_ = 0;
};
