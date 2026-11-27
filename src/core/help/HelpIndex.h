// The in-app help's navigation and search (RES-HELP-003..006, ADR-000215):
// reads what tools/help/build_help.py generated (index.json, search.json)
// and answers topic lookups and ranked full-text queries, offline. No user
// interface: the application's HelpWindow shows what this finds.
#pragma once

#include "foundation/Result.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace resonant::help {

struct Heading {
    std::string text, anchor;
    int level = 2;
};

struct Topic {
    std::string id, title, category, status, summary, file;
    std::vector<std::string> keywords, related, screenshots;
    std::vector<Heading> headings;
    std::string previous, next;  // reading order; empty at the ends
};

struct Category {
    std::string id, title, summary;
    std::vector<std::string> topics;
};

struct SearchHit {
    std::string topic;
    double score = 0.0;
    std::vector<std::string> matchedTerms;  // the indexed words that matched (to highlight)
};

class HelpIndex {
public:
    // `directory` holds index.json, search.json and html/.
    static Result<HelpIndex> load(const std::filesystem::path& directory);

    const std::filesystem::path& directory() const { return directory_; }
    std::filesystem::path htmlDirectory() const { return directory_ / "html"; }
    const std::string& home() const { return home_; }
    const std::string& built() const { return built_; }
    const std::string& applicationVersion() const { return version_; }
    const std::vector<Category>& categories() const { return categories_; }
    const std::vector<Topic>& topics() const { return topics_; }
    const Topic* find(const std::string& id) const;
    // "Help > Category > Title" for a topic (Help alone for the home page).
    std::vector<std::string> breadcrumbs(const std::string& id) const;
    std::filesystem::path pathOf(const std::string& id) const;  // the topic's HTML file

    // Ranked search: every query word must match (the last one also as a
    // prefix, so results appear while typing); a topic's score is the sum of
    // its weights for the matched words (title > keywords > headings >
    // summary > body). Ties keep the reading order.
    std::vector<SearchHit> search(const std::string& query, size_t limit = 30) const;
    static std::vector<std::string> words(const std::string& text);

private:
    std::filesystem::path directory_;
    std::string home_, built_, version_;
    std::vector<Category> categories_;
    std::vector<Topic> topics_;
    std::map<std::string, size_t> position_;
    std::map<std::string, std::map<std::string, double>> terms_;  // word -> topic -> weight
};

}  // namespace resonant::help
