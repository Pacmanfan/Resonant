#include "HelpWindow.h"

#include <algorithm>

#include "ui_HelpImageViewer.h"
#include "ui_HelpWindow.h"

#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QShortcut>
#include <QTextBlock>
#include <QTextDocument>
#include <QUrl>

using resonant::help::HelpIndex;

namespace {
// A small dialog that shows a screenshot full size (Fit, 100 %, zoom).
class ImageViewer : public QDialog {
public:
    ImageViewer(const QString& file, const QString& caption, QWidget* parent) : QDialog(parent), ui_(std::make_unique<Ui::HelpImageViewer>())
    {
        ui_->setupUi(this);
        setAttribute(Qt::WA_DeleteOnClose);
        pixmap_ = QPixmap(file);
        ui_->captionLabel->setText(caption);
        connect(ui_->fitButton, &QPushButton::clicked, this, [this] { fit(); });
        connect(ui_->actualButton, &QPushButton::clicked, this, [this] { setScale(1.0); });
        connect(ui_->zoomInButton, &QPushButton::clicked, this, [this] { setScale(scale_ * 1.25); });
        connect(ui_->zoomOutButton, &QPushButton::clicked, this, [this] { setScale(scale_ / 1.25); });
        setScale(1.0);
    }
    bool valid() const { return !pixmap_.isNull(); }

private:
    void fit()
    {
        if (pixmap_.isNull())
            return;
        const QSize area = ui_->scrollArea->viewport()->size();
        setScale(std::min(area.width() / double(pixmap_.width()), area.height() / double(pixmap_.height())));
    }
    void setScale(double s)
    {
        scale_ = std::clamp(s, 0.1, 6.0);
        if (pixmap_.isNull())
            return;
        const QPixmap scaled = pixmap_.scaled(pixmap_.size() * scale_, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        ui_->imageLabel->setPixmap(scaled);
        ui_->imageLabel->resize(scaled.size());
    }
    std::unique_ptr<Ui::HelpImageViewer> ui_;
    QPixmap pixmap_;
    double scale_ = 1.0;
};
}  // namespace

HelpWindow::HelpWindow(QWidget* parent) : QWidget(parent, Qt::Window), ui_(std::make_unique<Ui::HelpWindow>())
{
    ui_->setupUi(this);
    ui_->splitter->setStretchFactor(1, 3);
    ui_->splitter->setSizes({280, 820});
    connect(ui_->contentsTree, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem* item) {
        const QString id = item->data(0, Qt::UserRole).toString();
        if (!id.isEmpty())
            showTopic(id);
    });
    connect(ui_->contentsTree, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item) {
        const QString id = item->data(0, Qt::UserRole).toString();
        if (!id.isEmpty() && id != current_)
            showTopic(id);
    });
    connect(ui_->searchEdit, &QLineEdit::textChanged, this, [this](const QString& text) { search(text); });
    connect(ui_->searchEdit, &QLineEdit::returnPressed, this, [this] {
        if (!hits_.empty())
            openResult(0);
    });
    connect(ui_->resultsList, &QListWidget::itemActivated, this, [this](QListWidgetItem* item) { openResult(ui_->resultsList->row(item)); });
    connect(ui_->resultsList, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) { openResult(ui_->resultsList->row(item)); });
    connect(ui_->articleView, &QTextBrowser::anchorClicked, this, &HelpWindow::linkActivated);
    connect(ui_->backButton, &QToolButton::clicked, this, &HelpWindow::back);
    connect(ui_->forwardButton, &QToolButton::clicked, this, &HelpWindow::forward);
    connect(ui_->homeButton, &QToolButton::clicked, this, &HelpWindow::home);
    connect(ui_->previousButton, &QToolButton::clicked, this, &HelpWindow::previous);
    connect(ui_->nextButton, &QToolButton::clicked, this, &HelpWindow::next);
    connect(ui_->tocCombo, qOverload<int>(&QComboBox::activated), this, [this](int k) { showSection(k); });
    connect(ui_->breadcrumbLabel, &QLabel::linkActivated, this, [this](const QString& link) { showTopic(link); });
    new QShortcut(QKeySequence::Back, this, [this] { back(); });
    new QShortcut(QKeySequence::Forward, this, [this] { forward(); });
    new QShortcut(QKeySequence::Find, this, [this] {
        ui_->navTabs->setCurrentWidget(ui_->searchTab);
        ui_->searchEdit->setFocus();
        ui_->searchEdit->selectAll();
    });
    updateNavigation();
}

HelpWindow::~HelpWindow() = default;

bool HelpWindow::load(const QString& directory)
{
    index_.reset();
    loadError_.clear();
    if (directory.isEmpty()) {
        loadError_ = tr("The help was not found. Build the 'help' target (it generates the help from help/src), or set RESONANT_HELP_DIR.");
    } else {
        auto loaded = HelpIndex::load(directory.toStdString());
        if (loaded)
            index_ = loaded.take();
        else
            loadError_ = QString::fromStdString(loaded.message());
    }
    if (!index_) {
        ui_->articleView->setHtml(QStringLiteral("<h1>Help unavailable</h1><p>%1</p>").arg(loadError_.toHtmlEscaped()));
        ui_->statusLabel->setText(loadError_);
        return false;
    }
    ui_->articleView->setSearchPaths({QString::fromStdString(index_->htmlDirectory().string())});
    ui_->statusLabel->setText(tr("Resonant %1 help, built %2: %3 topics. Offline.")
                                  .arg(QString::fromStdString(index_->applicationVersion()), QString::fromStdString(index_->built()))
                                  .arg(index_->topics().size()));
    fillContents();
    history_.clear();
    position_ = -1;
    return true;
}

void HelpWindow::fillContents()
{
    ui_->contentsTree->clear();
    auto* homeItem = new QTreeWidgetItem(ui_->contentsTree, {tr("Resonant Help")});
    homeItem->setData(0, Qt::UserRole, QString::fromStdString(index_->home()));
    for (const auto& c : index_->categories()) {
        auto* cat = new QTreeWidgetItem(ui_->contentsTree, {QString::fromStdString(c.title)});
        cat->setToolTip(0, QString::fromStdString(c.summary));
        for (const std::string& id : c.topics)
            if (const auto* t = index_->find(id)) {
                QString label = QString::fromStdString(t->title);
                if (t->status != "available")
                    label += QStringLiteral(" (%1)").arg(QString::fromStdString(t->status));
                auto* item = new QTreeWidgetItem(cat, {label});
                item->setData(0, Qt::UserRole, QString::fromStdString(id));
                item->setToolTip(0, QString::fromStdString(t->summary));
            }
    }
}

bool HelpWindow::showTopic(const QString& id, const QString& anchor)
{
    if (!index_)
        return false;
    if (!index_->find(id.toStdString())) {
        showMissing(id);
        return false;
    }
    display(id, anchor, true);
    return true;
}

void HelpWindow::display(const QString& id, const QString& anchor, bool record)
{
    const auto* t = index_->find(id.toStdString());
    const QString file = QString::fromStdString(index_->pathOf(id.toStdString()).string());
    ui_->articleView->setSource(QUrl::fromLocalFile(file));
    if (!anchor.isEmpty())
        ui_->articleView->scrollToAnchor(anchor);
    current_ = id;
    if (record) {
        while (history_.size() > position_ + 1)
            history_.removeLast();
        history_.append(id);
        position_ = static_cast<int>(history_.size()) - 1;
    }
    // Breadcrumbs (the category opens its first topic), sections.
    QStringList crumbs = {QStringLiteral("<a href=\"%1\">Help</a>").arg(QString::fromStdString(index_->home()))};
    if (id.toStdString() != index_->home()) {
        crumbs.append(QString::fromStdString(t->category).toHtmlEscaped());
        crumbs.append(QStringLiteral("<b>%1</b>").arg(QString::fromStdString(t->title).toHtmlEscaped()));
    }
    ui_->breadcrumbLabel->setText(crumbs.join(QStringLiteral(" › ")));
    ui_->tocCombo->clear();
    ui_->tocCombo->addItem(tr("(top)"), QString());
    for (const auto& h : t->headings)
        ui_->tocCombo->addItem(QString(std::max(0, h.level - 2) * 2, QLatin1Char(' ')) + QString::fromStdString(h.text), QString::fromStdString(h.anchor));
    // Select the topic in the contents.
    const auto items = ui_->contentsTree->findItems(QStringLiteral("*"), Qt::MatchWildcard | Qt::MatchRecursive);
    for (QTreeWidgetItem* item : items)
        if (item->data(0, Qt::UserRole).toString() == id) {
            ui_->contentsTree->setCurrentItem(item);
            break;
        }
    setWindowTitle(QStringLiteral("%1 - Resonant Help").arg(QString::fromStdString(t->title)));
    updateNavigation();
    emit topicShown(id);
}

void HelpWindow::showMissing(const QString& id)
{
    current_.clear();
    QString html = QStringLiteral("<h1>Topic not found</h1><p>The help has no topic <code>%1</code>. It may have been renamed, or this "
                                  "part of Resonant is not documented yet.</p>")
                       .arg(id.toHtmlEscaped());
    QString words = id;
    words.replace(QRegularExpression(QStringLiteral("[.\\-_]")), QStringLiteral(" "));
    const auto hits = index_->search(words.toStdString(), 8);
    if (!hits.empty()) {
        html += QStringLiteral("<p>Perhaps one of these:</p><ul>");
        for (const auto& h : hits)
            if (const auto* t = index_->find(h.topic))
                html += QStringLiteral("<li><a href=\"%1.html\">%2</a></li>").arg(QString::fromStdString(h.topic), QString::fromStdString(t->title).toHtmlEscaped());
        html += QStringLiteral("</ul>");
    }
    html += QStringLiteral("<p><a href=\"%1.html\">Help home</a></p>").arg(QString::fromStdString(index_->home()));
    ui_->articleView->setHtml(html);
    ui_->breadcrumbLabel->setText(QStringLiteral("<a href=\"%1\">Help</a> › not found").arg(QString::fromStdString(index_->home())));
    ui_->tocCombo->clear();
    updateNavigation();
}

void HelpWindow::linkActivated(const QUrl& url)
{
    // A screenshot's link (images/..., or the older zoom: form) opens it full size.
    static const QStringList images = {QStringLiteral(".png"), QStringLiteral(".jpg"), QStringLiteral(".jpeg"), QStringLiteral(".svg"), QStringLiteral(".gif")};
    const bool image = url.scheme().isEmpty() && std::any_of(images.begin(), images.end(), [&url](const QString& e) {
        return url.path().endsWith(e, Qt::CaseInsensitive);
    });
    if (url.scheme() == QLatin1String("zoom") || image) {
        showImage(url.path());
        return;
    }
    const QString name = QFileInfo(url.path()).fileName();
    if (name.endsWith(QLatin1String(".html"))) {
        showTopic(name.chopped(5), url.fragment());
        return;
    }
    if (url.path().isEmpty() && !url.fragment().isEmpty()) {
        ui_->articleView->scrollToAnchor(url.fragment());
        return;
    }
    ui_->statusLabel->setText(tr("The link %1 is not part of the help.").arg(url.toString()));
}

QDialog* HelpWindow::showImage(const QString& source)
{
    if (!index_)
        return nullptr;
    const QString file = QDir(QString::fromStdString(index_->htmlDirectory().string())).absoluteFilePath(source);
    auto* viewer = new ImageViewer(file, QFileInfo(source).fileName(), this);
    if (!viewer->valid()) {
        delete viewer;
        ui_->statusLabel->setText(tr("The image %1 could not be loaded.").arg(source));
        return nullptr;
    }
    viewer->show();
    return viewer;
}

int HelpWindow::loadedImageCount() const
{
    int count = 0;
    QTextDocument* doc = ui_->articleView->document();
    for (QTextBlock b = doc->begin(); b.isValid(); b = b.next())
        for (auto it = b.begin(); !it.atEnd(); ++it) {
            const QTextCharFormat f = it.fragment().charFormat();
            if (f.isImageFormat()) {
                const QVariant image = doc->resource(QTextDocument::ImageResource, QUrl(f.toImageFormat().name()));
                if (!image.isNull() || !ui_->articleView->loadResource(QTextDocument::ImageResource, QUrl(f.toImageFormat().name())).isNull())
                    ++count;
            }
        }
    return count;
}

QStringList HelpWindow::search(const QString& query)
{
    ui_->resultsList->clear();
    hits_.clear();
    QStringList ids;
    if (!index_)
        return ids;
    if (ui_->searchEdit->text() != query) {
        const QSignalBlocker block(ui_->searchEdit);
        ui_->searchEdit->setText(query);
    }
    hits_ = index_->search(query.toStdString());
    for (const auto& h : hits_) {
        const auto* t = index_->find(h.topic);
        auto* item = new QListWidgetItem(QStringLiteral("%1  —  %2").arg(QString::fromStdString(t->title), QString::fromStdString(t->category)));
        item->setToolTip(QString::fromStdString(t->summary));
        ui_->resultsList->addItem(item);
        ids.append(QString::fromStdString(h.topic));
    }
    ui_->resultsLabel->setText(query.trimmed().isEmpty() ? QString() : tr("%n topic(s)", nullptr, static_cast<int>(hits_.size())));
    if (!query.trimmed().isEmpty())
        ui_->navTabs->setCurrentWidget(ui_->searchTab);
    return ids;
}

bool HelpWindow::openResult(int row)
{
    if (row < 0 || row >= static_cast<int>(hits_.size()))
        return false;
    const auto hit = hits_[static_cast<size_t>(row)];
    display(QString::fromStdString(hit.topic), {}, true);
    QStringList words;
    for (const auto& w : hit.matchedTerms)
        words.append(QString::fromStdString(w));
    highlight(words);
    return true;
}

void HelpWindow::highlight(const QStringList& words)
{
    QList<QTextEdit::ExtraSelection> selections;
    QTextDocument* doc = ui_->articleView->document();
    QTextCharFormat mark;
    mark.setBackground(QColor(0xff, 0xec, 0x8b));
    for (const QString& w : words) {
        QTextCursor c(doc);
        while (!(c = doc->find(QRegularExpression(QStringLiteral("\\b") + QRegularExpression::escape(w), QRegularExpression::CaseInsensitiveOption), c))
                    .isNull()) {
            QTextEdit::ExtraSelection s;
            s.cursor = c;
            s.format = mark;
            selections.append(s);
            if (selections.size() > 400)
                break;
        }
    }
    ui_->articleView->setExtraSelections(selections);
    if (!selections.isEmpty()) {
        ui_->articleView->setTextCursor(selections.front().cursor);
        ui_->articleView->ensureCursorVisible();
    }
}

int HelpWindow::highlightCount() const
{
    return static_cast<int>(ui_->articleView->extraSelections().size());
}

QString HelpWindow::articleText() const
{
    return ui_->articleView->toPlainText();
}

QString HelpWindow::articleHtml() const
{
    return ui_->articleView->toHtml();
}

QString HelpWindow::breadcrumbText() const
{
    QTextDocument d;
    d.setHtml(ui_->breadcrumbLabel->text());
    return d.toPlainText();
}

QStringList HelpWindow::sections() const
{
    QStringList out;
    for (int k = 1; k < ui_->tocCombo->count(); ++k)
        out.append(ui_->tocCombo->itemText(k).trimmed());
    return out;
}

bool HelpWindow::showSection(int k)
{
    if (k < 0 || k >= ui_->tocCombo->count())
        return false;
    const QString anchor = ui_->tocCombo->itemData(k).toString();
    if (anchor.isEmpty())
        ui_->articleView->verticalScrollBar()->setValue(0);
    else
        ui_->articleView->scrollToAnchor(anchor);
    return true;
}

void HelpWindow::back()
{
    if (position_ > 0)
        display(history_[--position_], {}, false);
}

void HelpWindow::forward()
{
    if (position_ + 1 < history_.size())
        display(history_[++position_], {}, false);
}

void HelpWindow::previous()
{
    if (!index_ || current_.isEmpty())
        return;
    if (const auto* t = index_->find(current_.toStdString()); t && !t->previous.empty())
        showTopic(QString::fromStdString(t->previous));
}

void HelpWindow::next()
{
    if (!index_ || current_.isEmpty())
        return;
    if (const auto* t = index_->find(current_.toStdString()); t && !t->next.empty())
        showTopic(QString::fromStdString(t->next));
}

void HelpWindow::home()
{
    if (index_)
        showTopic(QString::fromStdString(index_->home()));
}

void HelpWindow::updateNavigation()
{
    const auto* t = index_ && !current_.isEmpty() ? index_->find(current_.toStdString()) : nullptr;
    ui_->backButton->setEnabled(position_ > 0);
    ui_->forwardButton->setEnabled(position_ + 1 < history_.size());
    ui_->previousButton->setEnabled(t && !t->previous.empty());
    ui_->nextButton->setEnabled(t && !t->next.empty());
    ui_->homeButton->setEnabled(index_.has_value());
}
