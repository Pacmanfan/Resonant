#include "PythonConsole.h"

#include "PythonSession.h"
#include "ui_PythonConsole.h"

#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonArray>
#include <QScrollBar>
#include <QTextCharFormat>

PythonConsole::PythonConsole(QWidget* parent) : QWidget(parent), ui_(std::make_unique<Ui::PythonConsole>())
{
    ui_->setupUi(this);
    ui_->inputEdit->setLineNumbersVisible(false);
    ui_->inputEdit->setSubmitOnEnter(true);
    connect(ui_->inputEdit, &CodeEditor::submitted, this, &PythonConsole::submitInput);
    connect(ui_->inputEdit, &CodeEditor::completionRequested, this, &PythonConsole::requestCompletion);
    connect(ui_->inputEdit, &CodeEditor::historyUp, this, [this] { recall(-1); });
    connect(ui_->inputEdit, &CodeEditor::historyDown, this, [this] { recall(+1); });
    connect(ui_->runButton, &QPushButton::clicked, this, &PythonConsole::submitInput);
    connect(ui_->stopButton, &QPushButton::clicked, this, [this] {
        if (session_)
            session_->interrupt();
    });
    connect(ui_->clearButton, &QPushButton::clicked, this, &PythonConsole::clear);
    connect(ui_->sendProjectButton, &QPushButton::clicked, this, [this] { sendProject(true); });
    connect(ui_->restartButton, &QPushButton::clicked, this, [this] {
        if (!session_)
            return;
        appendOutput(QStringLiteral("--- restarting Python ---\n"), QStringLiteral("info"));
        projectSent_ = false;
        session_->restart();
        ensureStarted();
    });
    connect(ui_->runFileButton, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, tr("Run Python Script"), QString(), tr("Python scripts (*.py)"));
        if (!path.isEmpty())
            runFile(path);
    });
    updateState();
}

PythonConsole::~PythonConsole() = default;

void PythonConsole::setSession(PythonSession* session)
{
    session_ = session;
    connect(session_, &PythonSession::readyChanged, this, [this](bool ready) {
        if (ready) {
            const QJsonObject info = session_->readyInfo();
            if (!info.value(QStringLiteral("api")).isNull())
                appendOutput(QStringLiteral("Python ready: resonant API %1, core %2. `project` is a copy of the open project; "
                                            "app.apply(project, \"what\") applies your changes as one undoable step.\n")
                                 .arg(info.value(QStringLiteral("api")).toString(), info.value(QStringLiteral("core")).toString()),
                             QStringLiteral("info"));
            if (!projectSent_)
                sendProject(false);
        }
        updateState();
    });
    connect(session_, &PythonSession::busyChanged, this, &PythonConsole::updateState);
    connect(session_, &PythonSession::crashed, this, [this](const QString& reason) {
        appendOutput(reason + QLatin1Char('\n'), QStringLiteral("error"));
        projectSent_ = false;
        updateState();
    });
    connect(session_, &PythonSession::output, this, [this](int id, const QString& stream, const QString& text) {
        if (id == pendingExec_ && id != 0)
            echoPending();
        if (id == pendingExec_ || id == -1)
            appendOutput(text, stream);
    });
    connect(session_, &PythonSession::received, this, &PythonConsole::onReceived);
    updateState();
}

void PythonConsole::ensureStarted()
{
    if (!session_)
        return;
    if (!session_->isRunning() && !session_->start())
        appendOutput(session_->startupError() + QLatin1Char('\n'), QStringLiteral("error"));
    updateState();
}

void PythonConsole::updateState()
{
    const bool running = session_ && session_->isRunning();
    const bool ready = session_ && session_->isReady();
    const bool busy = session_ && session_->isBusy() && pendingExec_ != 0;
    ui_->statusLabel->setText(!session_  ? tr("Python: unavailable")
                              : !running ? tr("Python: not started (type to start)")
                              : !ready   ? tr("Python: starting...")
                              : busy     ? tr("Python: running... (Stop interrupts)")
                                         : tr("Python: ready"));
    ui_->stopButton->setEnabled(busy);
    ui_->runButton->setEnabled(!busy);
    ui_->inputEdit->setReadOnly(busy);
    ui_->sendProjectButton->setEnabled(running && !busy);
}

bool PythonConsole::isBusy() const
{
    return pendingExec_ != 0;
}

void PythonConsole::appendOutput(const QString& text, const QString& kind)
{
    QTextCharFormat format;
    if (kind == QLatin1String("stderr") || kind == QLatin1String("error"))
        format.setForeground(QColor(0xa1, 0x2a, 0x2a));
    else if (kind == QLatin1String("info"))
        format.setForeground(QColor(0x5a, 0x67, 0x78));
    else if (kind == QLatin1String("echo"))
        format.setForeground(QColor(0x1f, 0x4f, 0xb0));
    QTextCursor c(ui_->outputView->document());
    c.movePosition(QTextCursor::End);
    c.insertText(text, format);
    ui_->outputView->verticalScrollBar()->setValue(ui_->outputView->verticalScrollBar()->maximum());
}

QString PythonConsole::outputText() const
{
    return ui_->outputView->toPlainText();
}

QString PythonConsole::inputText() const
{
    return ui_->inputEdit->toPlainText();
}

void PythonConsole::clear()
{
    ui_->outputView->clear();
}

void PythonConsole::sendProject(bool announce)
{
    if (!session_ || !projectProvider_)
        return;
    ensureStarted();
    if (session_->setProject(projectProvider_()) != 0) {
        projectSent_ = true;
        if (announce)
            appendOutput(QStringLiteral("(project: a fresh copy of the open project)\n"), QStringLiteral("info"));
    }
}

bool PythonConsole::submit(const QString& code)
{
    ui_->inputEdit->setPlainText(code);
    submitInput();
    return session_ && session_->isRunning();
}

void PythonConsole::submitInput()
{
    if (!session_ || pendingExec_ != 0)
        return;
    ensureStarted();
    if (!session_->isRunning())
        return;
    QString source = ui_->inputEdit->toPlainText();
    const QString trimmed = source.trimmed();
    if (trimmed.isEmpty() && !source.contains(QLatin1Char('\n')))
        return;
    // `name?` or `?name`: documentation of a name.
    if (!trimmed.contains(QLatin1Char('\n')) && (trimmed.endsWith(QLatin1Char('?')) || trimmed.startsWith(QLatin1Char('?')))) {
        QString expression = trimmed;
        expression.remove(QLatin1Char('?'));
        appendOutput(QStringLiteral(">>> ") + trimmed + QLatin1Char('\n'), QStringLiteral("echo"));
        history_.append(trimmed);
        historyPosition_ = static_cast<int>(history_.size());
        ui_->inputEdit->clear();
        pendingSource_.clear();
        pendingExec_ = session_->request({{QStringLiteral("op"), QStringLiteral("inspect")}, {QStringLiteral("expression"), expression}});
        updateState();
        return;
    }
    QString mode = QStringLiteral("single");
    if (source.contains(QLatin1Char('\n'))) {
        // Several lines. An indented, non-empty last line continues a block (a
        // new line with the same indentation); otherwise the lines run together.
        const QString last = source.section(QLatin1Char('\n'), -1);
        if (!last.trimmed().isEmpty() && (last.startsWith(QLatin1Char(' ')) || last.startsWith(QLatin1Char('\t')))) {
            QString indent;
            for (QChar ch : last) {
                if (ch != QLatin1Char(' ') && ch != QLatin1Char('\t'))
                    break;
                indent += ch;
            }
            if (last.trimmed().endsWith(QLatin1Char(':')))
                indent += QStringLiteral("    ");
            ui_->inputEdit->setPlainText(source + QLatin1Char('\n') + indent);
            ui_->inputEdit->moveCursor(QTextCursor::End);
            ui_->promptLabel->setText(QStringLiteral("..."));
            return;
        }
        if (!source.endsWith(QLatin1Char('\n')))
            source += QLatin1Char('\n');
        mode = QStringLiteral("block");
    }
    pendingSource_ = source;
    pendingExec_ = session_->execute(source, mode, QStringLiteral("<console>"));
    updateState();
}

void PythonConsole::onReceived(int id, const QJsonObject& m)
{
    const QString type = m.value(QStringLiteral("type")).toString();
    if (id == pendingCompletion_ && type == QLatin1String("completions")) {
        QStringList items;
        for (const auto& v : m.value(QStringLiteral("items")).toArray())
            items.append(v.toString());
        ui_->inputEdit->showCompletions(items, completionPrefix_);
        return;
    }
    if (id != pendingExec_ || id == 0)
        return;
    if (type == QLatin1String("incomplete")) {
        // More input is needed: keep the block, continue on a new indented line.
        QString text = pendingSource_;
        const QString last = text.section(QLatin1Char('\n'), -1);
        QString indent;
        for (QChar ch : last) {
            if (ch != QLatin1Char(' '))
                break;
            indent += ch;
        }
        if (last.trimmed().endsWith(QLatin1Char(':')))
            indent += QStringLiteral("    ");
        ui_->inputEdit->setPlainText(text + QLatin1Char('\n') + indent);
        ui_->inputEdit->moveCursor(QTextCursor::End);
        ui_->promptLabel->setText(QStringLiteral("..."));
        pendingSource_.clear();
        pendingExec_ = 0;
        updateState();
        return;
    }
    if (type == QLatin1String("inspect")) {
        appendOutput(QStringLiteral("%1: %2\n").arg(m.value(QStringLiteral("valueType")).toString(), m.value(QStringLiteral("repr")).toString()));
        const QString doc = m.value(QStringLiteral("doc")).toString();
        if (!doc.isEmpty())
            appendOutput(doc + QLatin1Char('\n'), QStringLiteral("info"));
        return;
    }
    if (type == QLatin1String("error")) {
        echoPending();
        const QString tb = m.value(QStringLiteral("traceback")).toString();
        appendOutput(tb.isEmpty() ? QStringLiteral("%1: %2\n").arg(m.value(QStringLiteral("error")).toString(), m.value(QStringLiteral("message")).toString())
                                  : tb,
                     QStringLiteral("error"));
        return;
    }
    if (type == QLatin1String("done")) {
        echoPending();
        if (!pendingSource_.isEmpty()) {
            history_.append(pendingSource_.trimmed());
            historyPosition_ = static_cast<int>(history_.size());
            ui_->inputEdit->clear();
            ui_->promptLabel->setText(QStringLiteral(">>>"));
        }
        pendingExec_ = 0;
        pendingSource_.clear();
        echoed_ = false;
        updateState();
    }
}

void PythonConsole::echoPending()
{
    // The submitted block, shown once, before what it printed.
    if (echoed_ || pendingSource_.isEmpty())
        return;
    echoed_ = true;
    const QStringList lines = pendingSource_.split(QLatin1Char('\n'));
    QString echo;
    for (int k = 0; k < lines.size(); ++k)
        if (k + 1 < lines.size() || !lines[k].isEmpty())
            echo += (k == 0 ? QStringLiteral(">>> ") : QStringLiteral("... ")) + lines[k] + QLatin1Char('\n');
    appendOutput(echo, QStringLiteral("echo"));
}

void PythonConsole::requestCompletion(const QString& prefix)
{
    if (!session_ || !session_->isReady() || prefix.isEmpty())
        return;
    completionPrefix_ = prefix;
    pendingCompletion_ = session_->request({{QStringLiteral("op"), QStringLiteral("complete")}, {QStringLiteral("text"), prefix}});
}

void PythonConsole::recall(int direction)
{
    if (history_.isEmpty())
        return;
    historyPosition_ = std::clamp(historyPosition_ + direction, 0, static_cast<int>(history_.size()));
    ui_->inputEdit->setPlainText(historyPosition_ < history_.size() ? history_[historyPosition_] : QString());
    ui_->inputEdit->moveCursor(QTextCursor::End);
}

bool PythonConsole::runFile(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        appendOutput(tr("Cannot read %1\n").arg(path), QStringLiteral("error"));
        return false;
    }
    ensureStarted();
    if (!session_ || !session_->isRunning() || pendingExec_ != 0)
        return false;
    appendOutput(QStringLiteral(">>> # run %1\n").arg(QFileInfo(path).fileName()), QStringLiteral("echo"));
    pendingSource_.clear();
    pendingExec_ = session_->execute(QString::fromUtf8(f.readAll()), QStringLiteral("exec"), QFileInfo(path).fileName());
    updateState();
    return true;
}

int PythonConsole::runCode(const QString& code, const QString& label)
{
    ensureStarted();
    if (!session_ || !session_->isRunning() || pendingExec_ != 0)
        return 0;
    appendOutput(QStringLiteral(">>> # %1\n").arg(label), QStringLiteral("echo"));
    for (const QString& line : code.split(QLatin1Char('\n')))
        if (!line.trimmed().isEmpty())
            appendOutput(QStringLiteral("... ") + line + QLatin1Char('\n'), QStringLiteral("echo"));
    pendingSource_.clear();
    pendingExec_ = session_->execute(code, QStringLiteral("exec"), label);
    updateState();
    return pendingExec_;
}
