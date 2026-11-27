// The help window (RES-HELP-001..010, ADR-000215): the offline HTML help
// built from help/src. Contents tree by category, ranked full-text search
// with the matched words highlighted, breadcrumbs, back / forward, previous /
// next in reading order, "on this page" sections, related topics (in each
// page), screenshots that open full size, and a clear page for a topic that
// does not exist. Interface: forms/HelpWindow.ui, forms/HelpImageViewer.ui.
#pragma once

#include "help/HelpIndex.h"

#include <QStringList>
#include <QWidget>

#include <memory>
#include <optional>

class QDialog;
namespace Ui {
class HelpWindow;
}

class HelpWindow : public QWidget {
    Q_OBJECT
public:
    explicit HelpWindow(QWidget* parent = nullptr);
    ~HelpWindow() override;

    bool load(const QString& helpDirectory);
    bool isLoaded() const { return index_.has_value(); }
    QString loadError() const { return loadError_; }
    const resonant::help::HelpIndex* index() const { return index_ ? &*index_ : nullptr; }

    // False when the topic does not exist (a "not found" page is shown).
    bool showTopic(const QString& id, const QString& anchor = {});
    QString currentTopic() const { return current_; }
    QStringList search(const QString& query);  // fills the results; returns the topic ids, best first
    bool openResult(int row);                   // shows a result with the matched words highlighted
    int highlightCount() const;
    QString articleText() const;
    QString articleHtml() const;
    QString breadcrumbText() const;
    QStringList sections() const;
    bool showSection(int index);
    void back();
    void forward();
    void previous();
    void next();
    void home();
    bool canGoBack() const { return position_ > 0; }
    // Opens an image of the current page full size (its links); returns the dialog (tests close it).
    QDialog* showImage(const QString& source);
    int loadedImageCount() const;  // images of the current page that resolved to files

signals:
    void topicShown(const QString& id);

private:
    void fillContents();
    void display(const QString& id, const QString& anchor, bool record);
    void showMissing(const QString& id);
    void highlight(const QStringList& words);
    void updateNavigation();
    void linkActivated(const QUrl& url);

    std::unique_ptr<Ui::HelpWindow> ui_;
    std::optional<resonant::help::HelpIndex> index_;
    QString loadError_;
    QString current_;
    QStringList history_;
    int position_ = -1;
    std::vector<resonant::help::SearchHit> hits_;
};
