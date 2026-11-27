#include "help/HelpIndex.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <set>

namespace resonant::help {

using json = nlohmann::json;

namespace {
Result<json> readJson(const std::filesystem::path& file)
{
    std::ifstream in(file);
    if (!in)
        return Result<json>::failure("the help has not been built: " + file.string() + " is missing (build the 'help' target)");
    try {
        return json::parse(in);
    } catch (const std::exception& e) {
        return Result<json>::failure(file.string() + " is damaged: " + e.what());
    }
}
std::vector<std::string> strings(const json& j, const char* key)
{
    return j.contains(key) && j[key].is_array() ? j[key].get<std::vector<std::string>>() : std::vector<std::string>{};
}
std::string text(const json& j, const char* key)
{
    return j.contains(key) && j[key].is_string() ? j[key].get<std::string>() : std::string();
}
// The same stop words as the build tool, so a query never needs a word the index left out.
const std::set<std::string>& stopWords()
{
    static const std::set<std::string> s = {"a",  "an",   "and", "are",  "as",   "at",   "be",  "by",   "can",  "for",  "from", "has",
                                            "have", "in", "is",  "it",   "its",  "of",   "on",  "or",   "that", "the",  "this", "to",
                                            "was",  "were", "will", "with", "you", "your"};
    return s;
}
}  // namespace

std::vector<std::string> HelpIndex::words(const std::string& input)
{
    std::vector<std::string> out;
    std::string current;
    const auto flush = [&] {
        if (current.size() > 1 && !stopWords().contains(current))
            out.push_back(current);
        current.clear();
    };
    for (char raw : input) {
        const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(raw)));
        const bool alnum = std::isalnum(static_cast<unsigned char>(c)) != 0;
        if (alnum || (!current.empty() && (c == '_' || c == '-')))
            current.push_back(c);
        else
            flush();
    }
    flush();
    return out;
}

Result<HelpIndex> HelpIndex::load(const std::filesystem::path& directory)
{
    using R = Result<HelpIndex>;
    auto index = readJson(directory / "index.json");
    if (!index)
        return R::failure(index.message());
    auto search = readJson(directory / "search.json");
    if (!search)
        return R::failure(search.message());
    const json& j = index.value();
    if (text(j, "format") != "resonant.help-index" || j.value("version", 0) != 1)
        return R::failure(directory.string() + ": not a version 1 Resonant help index");
    HelpIndex h;
    h.directory_ = directory;
    h.home_ = text(j, "home");
    h.built_ = text(j, "built");
    h.version_ = text(j, "applicationVersion");
    const json categories = j.value("categories", json::array());
    const json topics = j.value("topics", json::array());
    for (const json& c : categories)
        h.categories_.push_back({text(c, "id"), text(c, "title"), text(c, "summary"), strings(c, "topics")});
    for (const json& t : topics) {
        Topic topic;
        topic.id = text(t, "id");
        topic.title = text(t, "title");
        topic.category = text(t, "category");
        topic.status = text(t, "status");
        topic.summary = text(t, "summary");
        topic.file = text(t, "file");
        topic.keywords = strings(t, "keywords");
        topic.related = strings(t, "related");
        topic.screenshots = strings(t, "screenshots");
        topic.previous = text(t, "previous");
        topic.next = text(t, "next");
        const json headings = t.value("headings", json::array());
        for (const json& hd : headings)
            topic.headings.push_back({text(hd, "text"), text(hd, "anchor"), hd.value("level", 2)});
        h.position_[topic.id] = h.topics_.size();
        h.topics_.push_back(std::move(topic));
    }
    if (h.topics_.empty() || !h.find(h.home_))
        return R::failure(directory.string() + ": the help index has no home page");
    const json terms = search.value().value("terms", json::object());  // a named copy: items() must not outlive it
    for (const auto& [word, postings] : terms.items())
        for (const auto& [topic, weight] : postings.items())
            h.terms_[word][topic] = weight.get<double>();
    return h;
}

const Topic* HelpIndex::find(const std::string& id) const
{
    const auto it = position_.find(id);
    return it == position_.end() ? nullptr : &topics_[it->second];
}

std::vector<std::string> HelpIndex::breadcrumbs(const std::string& id) const
{
    std::vector<std::string> out = {"Help"};
    const Topic* t = find(id);
    if (!t || id == home_)
        return out;
    out.push_back(t->category);
    out.push_back(t->title);
    return out;
}

std::filesystem::path HelpIndex::pathOf(const std::string& id) const
{
    const Topic* t = find(id);
    return t ? htmlDirectory() / t->file : std::filesystem::path();
}

std::vector<SearchHit> HelpIndex::search(const std::string& query, size_t limit) const
{
    const std::vector<std::string> q = words(query);
    if (q.empty())
        return {};
    std::map<std::string, SearchHit> hits;
    bool first = true;
    for (size_t k = 0; k < q.size(); ++k) {
        // The topics this word matches, and their weight (exact, or as a prefix of an indexed word for the last one).
        std::map<std::string, std::pair<double, std::set<std::string>>> matched;
        const auto add = [&](const std::string& word, const std::map<std::string, double>& postings, double factor) {
            for (const auto& [topic, weight] : postings) {
                auto& m = matched[topic];
                m.first = std::max(m.first, weight * factor);
                m.second.insert(word);
            }
        };
        if (auto it = terms_.find(q[k]); it != terms_.end())
            add(q[k], it->second, 1.0);
        if (k + 1 == q.size())
            for (auto it = terms_.lower_bound(q[k]); it != terms_.end() && it->first.compare(0, q[k].size(), q[k]) == 0; ++it)
                if (it->first != q[k])
                    add(it->first, it->second, 0.7);
        std::map<std::string, SearchHit> next;
        for (auto& [topic, m] : matched) {
            if (!first && !hits.contains(topic))
                continue;
            SearchHit hit = first ? SearchHit{topic, 0.0, {}} : hits[topic];
            hit.score += m.first;
            hit.matchedTerms.insert(hit.matchedTerms.end(), m.second.begin(), m.second.end());
            next[topic] = std::move(hit);
        }
        hits = std::move(next);
        first = false;
    }
    std::vector<SearchHit> out;
    for (auto& [topic, hit] : hits)
        if (find(topic))
            out.push_back(std::move(hit));
    std::stable_sort(out.begin(), out.end(), [&](const SearchHit& a, const SearchHit& b) {
        if (a.score != b.score)
            return a.score > b.score;
        return position_.at(a.topic) < position_.at(b.topic);
    });
    if (out.size() > limit)
        out.resize(limit);
    return out;
}

}  // namespace resonant::help
