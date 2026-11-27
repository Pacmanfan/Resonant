#include "ScriptEditor.h"

#include "DocumentController.h"
#include "PythonSession.h"
#include "ResourcePaths.h"
#include "commands/SceneCommands.h"
#include "project/Project.h"
#include "ui_ScriptEditor.h"

#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QJsonArray>
#include <QMessageBox>
#include <QStandardPaths>
#include <QTextBlock>

#include <nlohmann/json.hpp>

namespace {
constexpr const char* kScriptsExtension = "scripts";

QStringList pythonFilesIn(const QString& directory)
{
    QStringList out;
    const QDir d(directory);
    for (const QString& name : d.entryList({QStringLiteral("*.py")}, QDir::Files, QDir::Name))
        out.append(d.absoluteFilePath(name));
    return out;
}
}  // namespace

ScriptEditor::ScriptEditor(QWidget* parent) : QMainWindow(parent), ui_(std::make_unique<Ui::ScriptEditor>())
{
    ui_->setupUi(this);
    ui_->splitter->setStretchFactor(0, 3);
    ui_->splitter->setStretchFactor(1, 1);
    setProperty("helpTopic", QStringLiteral("py.editor"));
    connect(ui_->actionNew, &QAction::triggered, this, [this] { newScript(); });
    connect(ui_->actionOpen, &QAction::triggered, this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, tr("Open Script"), userScriptDirectory(), tr("Python scripts (*.py)"));
        if (!path.isEmpty())
            openFile(path);
    });
    connect(ui_->actionSave, &QAction::triggered, this, [this] {
        if (file_.isEmpty() || file_.startsWith(QLatin1Char(':'))) {
            ui_->actionSaveAs->trigger();
            return;
        }
        saveFile(file_);
    });
    connect(ui_->actionSaveAs, &QAction::triggered, this, [this] {
        QDir().mkpath(userScriptDirectory());
        const QString path = QFileDialog::getSaveFileName(this, tr("Save Script"), userScriptDirectory(), tr("Python scripts (*.py)"));
        if (!path.isEmpty())
            saveFile(path.endsWith(QLatin1String(".py")) ? path : path + QStringLiteral(".py"));
    });
    connect(ui_->actionAttachToProject, &QAction::triggered, this, [this] {
        bool ok = false;
        const QString name = QInputDialog::getText(this, tr("Attach to Project"), tr("Script name:"), QLineEdit::Normal,
                                                   file_.isEmpty() ? tr("script") : QFileInfo(file_).completeBaseName(), &ok);
        if (ok && !name.trimmed().isEmpty())
            attachToProject(name.trimmed());
    });
    connect(ui_->actionClose, &QAction::triggered, this, &QWidget::close);
    connect(ui_->actionRun, &QAction::triggered, this, &ScriptEditor::run);
    connect(ui_->actionRunSelection, &QAction::triggered, this, &ScriptEditor::runSelection);
    connect(ui_->actionStop, &QAction::triggered, this, &ScriptEditor::stop);
    connect(ui_->actionScriptingHelp, &QAction::triggered, this, [this] { emit helpRequested(QStringLiteral("py.editor")); });
    connect(ui_->problemsList, &QListWidget::itemActivated, this, [this](QListWidgetItem* item) {
        const int line = item->data(Qt::UserRole).toInt();
        if (line > 0)
            ui_->editor->goToLine(line);
    });
    connect(ui_->editor, &CodeEditor::completionRequested, this, [this](const QString& prefix) {
        if (!session_ || !session_->isReady() || prefix.isEmpty())
            return;
        completionPrefix_ = prefix;
        pendingCompletion_ = session_->request({{QStringLiteral("op"), QStringLiteral("complete")}, {QStringLiteral("text"), prefix}});
    });
    connect(ui_->editor->document(), &QTextDocument::modificationChanged, this, &ScriptEditor::updateTitle);
    fillMenus();
    newScript();
}

ScriptEditor::~ScriptEditor() = default;

QString ScriptEditor::userScriptDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/scripts");
}

QStringList ScriptEditor::templateFiles() const
{
    const ResourcePaths p = ResourcePaths::locate();
    return p.pythonPackage.isEmpty() ? QStringList() : pythonFilesIn(p.pythonPackage + QStringLiteral("/templates"));
}

QStringList ScriptEditor::exampleFiles() const
{
    const ResourcePaths p = ResourcePaths::locate();
    return p.pythonPackage.isEmpty() ? QStringList() : pythonFilesIn(p.pythonPackage + QStringLiteral("/examples"));
}

void ScriptEditor::fillMenus()
{
    ui_->menuTemplates->clear();
    for (const QString& file : templateFiles())
        ui_->menuTemplates->addAction(QFileInfo(file).completeBaseName(), this, [this, file] {
            QFile f(file);
            if (f.open(QIODevice::ReadOnly | QIODevice::Text))
                newScript(QString::fromUtf8(f.readAll()));
        });
    if (ui_->menuTemplates->isEmpty())
        ui_->menuTemplates->addAction(tr("(no templates found)"))->setEnabled(false);
    ui_->menuExamples->clear();
    for (const QString& file : exampleFiles())
        ui_->menuExamples->addAction(QFileInfo(file).fileName(), this, [this, file] {
            // Examples are opened as new, unsaved scripts: the bundled files stay unchanged.
            QFile f(file);
            if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                newScript(QString::fromUtf8(f.readAll()));
                runName_ = QFileInfo(file).fileName();
                statusBar()->showMessage(tr("Example %1 (save it to keep your changes)").arg(QFileInfo(file).fileName()));
            }
        });
    if (ui_->menuExamples->isEmpty())
        ui_->menuExamples->addAction(tr("(no examples found)"))->setEnabled(false);
    connect(ui_->menuProjectScripts, &QMenu::aboutToShow, this, [this] {
        ui_->menuProjectScripts->clear();
        for (const QString& name : projectScripts())
            ui_->menuProjectScripts->addAction(name, this, [this, name] { openProjectScript(name); });
        if (ui_->menuProjectScripts->isEmpty())
            ui_->menuProjectScripts->addAction(tr("(the project has no scripts)"))->setEnabled(false);
    }, Qt::UniqueConnection);
}

void ScriptEditor::setSession(PythonSession* session)
{
    session_ = session;
    connect(session_, &PythonSession::received, this, &ScriptEditor::onReceived);
    connect(session_, &PythonSession::output, this, [this](int id, const QString& stream, const QString& text) {
        if (id == running_ && id != 0)
            appendOutput(text, stream == QLatin1String("stderr"));
    });
    connect(session_, &PythonSession::busyChanged, this, &ScriptEditor::updateActions);
    connect(session_, &PythonSession::readyChanged, this, &ScriptEditor::updateActions);
    updateActions();
}

void ScriptEditor::setDocument(DocumentController* document)
{
    document_ = document;
}

QString ScriptEditor::text() const
{
    return ui_->editor->toPlainText();
}

void ScriptEditor::setText(const QString& text)
{
    ui_->editor->setPlainText(text);
}

void ScriptEditor::newScript(const QString& text)
{
    ui_->editor->setPlainText(text.isEmpty() ? QStringLiteral("# A Resonant script. `project` is a copy of the open project;\n"
                                                              "# app.apply(project, \"what changed\") applies your changes (undoable).\n"
                                                              "import resonant\n\n")
                                             : text);
    ui_->editor->document()->setModified(false);
    ui_->editor->setErrorLine(0);
    file_.clear();
    runName_.clear();
    updateTitle();
}

bool ScriptEditor::openFile(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Open Script"), tr("Cannot read %1").arg(path));
        return false;
    }
    ui_->editor->setPlainText(QString::fromUtf8(f.readAll()));
    ui_->editor->document()->setModified(false);
    ui_->editor->setErrorLine(0);
    file_ = path;
    runName_ = QFileInfo(path).fileName();
    updateTitle();
    return true;
}

bool ScriptEditor::saveFile(const QString& path)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QMessageBox::warning(this, tr("Save Script"), tr("Cannot write %1").arg(path));
        return false;
    }
    f.write(text().toUtf8());
    file_ = path;
    runName_ = QFileInfo(path).fileName();
    ui_->editor->document()->setModified(false);
    updateTitle();
    statusBar()->showMessage(tr("Saved %1").arg(QFileInfo(path).fileName()), 4000);
    return true;
}

void ScriptEditor::updateTitle()
{
    const QString name = file_.isEmpty() ? (runName_.isEmpty() ? tr("Untitled script") : runName_) : QFileInfo(file_).fileName();
    setWindowTitle(QStringLiteral("%1%2 - Script Editor").arg(name, ui_->editor->document()->isModified() ? QStringLiteral(" *") : QString()));
}

void ScriptEditor::updateActions()
{
    ui_->actionStop->setEnabled(running_ != 0);
    ui_->actionRun->setEnabled(running_ == 0);
    ui_->actionRunSelection->setEnabled(running_ == 0);
}

void ScriptEditor::appendOutput(const QString& text, bool error)
{
    QTextCharFormat format;
    if (error)
        format.setForeground(QColor(0xa1, 0x2a, 0x2a));
    QTextCursor c(ui_->outputView->document());
    c.movePosition(QTextCursor::End);
    c.insertText(text, format);
    ui_->outputView->ensureCursorVisible();
}

bool ScriptEditor::execute(const QString& code, const QString& filename, int lineOffset)
{
    if (!session_ || running_ != 0)
        return false;
    if (!session_->isRunning() && !(starter_ ? starter_() : session_->start())) {
        appendOutput(session_->startupError() + QLatin1Char('\n'), true);
        return false;
    }
    ui_->problemsList->clear();
    ui_->editor->setErrorLine(0);
    ui_->bottomTabs->setCurrentWidget(ui_->outputTab);
    appendOutput(tr("--- running %1 ---\n").arg(filename));
    lineOffset_ = lineOffset;
    running_ = session_->execute(code, QStringLiteral("exec"), filename);
    updateActions();
    return running_ != 0;
}

bool ScriptEditor::run()
{
    return execute(text(), runName_.isEmpty() ? QStringLiteral("<script>") : runName_, 0);
}

bool ScriptEditor::runSelection()
{
    QTextCursor c = ui_->editor->textCursor();
    const QTextDocument* doc = ui_->editor->document();
    const int first = doc->findBlock(c.selectionStart()).blockNumber();
    const int last = doc->findBlock(c.selectionEnd()).blockNumber();
    QStringList lines;
    for (int b = first; b <= last; ++b)
        lines.append(doc->findBlockByNumber(b).text());
    // Whole lines, de-indented as a block, so a selection inside a function runs.
    int common = INT_MAX;
    for (const QString& l : lines)
        if (!l.trimmed().isEmpty()) {
            int k = 0;
            while (k < l.size() && l[k] == QLatin1Char(' '))
                ++k;
            common = std::min(common, k);
        }
    if (common == INT_MAX)
        return false;
    for (QString& l : lines)
        l = l.mid(std::min(common, static_cast<int>(l.size())));
    return execute(lines.join(QLatin1Char('\n')) + QLatin1Char('\n'), QStringLiteral("<selection>"), first);
}

void ScriptEditor::stop()
{
    if (session_ && running_ != 0)
        session_->interrupt();
}

void ScriptEditor::onReceived(int id, const QJsonObject& m)
{
    const QString type = m.value(QStringLiteral("type")).toString();
    if (id == pendingCompletion_ && type == QLatin1String("completions")) {
        QStringList items;
        for (const auto& v : m.value(QStringLiteral("items")).toArray())
            items.append(v.toString());
        ui_->editor->showCompletions(items, completionPrefix_);
        return;
    }
    if (id != running_ || id == 0)
        return;
    if (type == QLatin1String("error")) {
        const QString error = m.value(QStringLiteral("error")).toString();
        const QString message = m.value(QStringLiteral("message")).toString();
        const QJsonValue lineValue = m.value(QStringLiteral("line"));
        const int line = lineValue.isDouble() ? lineValue.toInt() + lineOffset_ : 0;
        appendOutput(m.value(QStringLiteral("traceback")).toString().isEmpty() ? error + QStringLiteral(": ") + message + QLatin1Char('\n')
                                                                               : m.value(QStringLiteral("traceback")).toString(),
                     true);
        auto* item = new QListWidgetItem(line > 0 ? tr("Line %1: %2: %3").arg(line).arg(error, message) : tr("%1: %2").arg(error, message));
        item->setData(Qt::UserRole, line);
        ui_->problemsList->addItem(item);
        if (line > 0) {
            ui_->editor->setErrorLine(line);
            ui_->editor->goToLine(line);
        }
        ui_->bottomTabs->setCurrentWidget(ui_->problemsTab);
    } else if (type == QLatin1String("done")) {
        appendOutput(m.value(QStringLiteral("ok")).toBool() ? tr("--- finished ---\n") : tr("--- stopped with an error ---\n"),
                     !m.value(QStringLiteral("ok")).toBool());
        running_ = 0;
        updateActions();
    }
}

QString ScriptEditor::outputText() const
{
    return ui_->outputView->toPlainText();
}

QStringList ScriptEditor::problems() const
{
    QStringList out;
    for (int k = 0; k < ui_->problemsList->count(); ++k)
        out.append(ui_->problemsList->item(k)->text());
    return out;
}

int ScriptEditor::errorLine() const
{
    return ui_->editor->errorLine();
}

QStringList ScriptEditor::projectScripts() const
{
    QStringList out;
    if (!document_)
        return out;
    const nlohmann::json& ext = document_->project().extensions;
    if (ext.contains(kScriptsExtension) && ext[kScriptsExtension].is_array())
        for (const auto& s : ext[kScriptsExtension])
            out.append(QString::fromStdString(s.value("name", std::string())));
    return out;
}

bool ScriptEditor::attachToProject(const QString& name)
{
    if (!document_)
        return false;
    nlohmann::json scripts = nlohmann::json::array();
    const nlohmann::json& ext = document_->project().extensions;
    if (ext.contains(kScriptsExtension) && ext[kScriptsExtension].is_array())
        scripts = ext[kScriptsExtension];
    bool replaced = false;
    for (auto& s : scripts)
        if (s.value("name", std::string()) == name.toStdString()) {
            s["code"] = text().toStdString();
            replaced = true;
        }
    if (!replaced)
        scripts.push_back({{"name", name.toStdString()}, {"code", text().toStdString()}});
    const auto status = document_->pushCommand(std::make_unique<resonant::SetProjectExtensionCommand>(
        document_->project(), kScriptsExtension, scripts.dump(), "Attach script " + name.toStdString()));
    if (!status) {
        QMessageBox::warning(this, tr("Attach to Project"), QString::fromStdString(status.message()));
        return false;
    }
    statusBar()->showMessage(tr("The script '%1' is stored in the project (save the project to keep it).").arg(name), 6000);
    return true;
}

bool ScriptEditor::openProjectScript(const QString& name)
{
    if (!document_)
        return false;
    const nlohmann::json& ext = document_->project().extensions;
    if (!ext.contains(kScriptsExtension) || !ext[kScriptsExtension].is_array())
        return false;
    for (const auto& s : ext[kScriptsExtension])
        if (s.value("name", std::string()) == name.toStdString()) {
            newScript(QString::fromStdString(s.value("code", std::string())));
            runName_ = name + QStringLiteral(".py");
            updateTitle();
            return true;
        }
    return false;
}
