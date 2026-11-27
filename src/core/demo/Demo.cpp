#include "demo/Demo.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>

namespace resonant::demo {

using json = nlohmann::json;

const std::vector<std::string>& stepActions()
{
    static const std::vector<std::string> actions = {
        "open-project",   // params: path (relative to the resource root)
        "workspace",      // params: panel, tab
        "select",         // params: object (id or name)
        "camera",         // params: preset (front|top|right|iso|frame-all|reset) and/or yaw, pitch (deg), distance, target
        "visualization",  // params: domain, quantity, slices, mode, component, grid, axes
        "run",            // params: what (acoustics.field|full-wave|mechanics.modal|mechanics.harmonic|em.pattern|em.analyse|
                          //         em.steer|em.taper|optimization|experiment|acoustics.focus), ...; waits for completion
        "show",           // params: panel, tab, result (index), item
        "help",           // params: topic
        "capture",        // params: name, target (window|viewport|<dock>), page, annotations
        "pause",          // params: text (waits for the presenter's Next; passes at once when unattended)
        "python",         // params: code; runs in the console's worker and waits for it
        "workflow",       // params: id, parameters; runs a workflow template on the open project
        "promote",        // params: candidate (best|<id>)
        "expect",         // params: the checks of `expect` without an action
        "presenter",      // params: on (bool)
        "project-action", // params: id (a workflow action: scene.set, em.steer, acoustics.focus, ...), params; one undoable change
        "experiment-data",// params: op (import-touchstone | compare), experiment, file; labelled measured or synthetic data
    };
    return actions;
}

namespace {
template <class T>
std::vector<T> listOf(const json& j, const char* key)
{
    return j.contains(key) && j[key].is_array() ? j[key].get<std::vector<T>>() : std::vector<T>{};
}
}  // namespace

Status DemoSequence::validate() const
{
    if (id.empty())
        return Status::failure("a demonstration sequence needs an id");
    if (steps.empty())
        return Status::failure("the sequence '" + id + "' has no steps");
    std::set<std::string> ids;
    const auto& known = stepActions();
    for (const DemoStep& s : steps) {
        if (s.id.empty())
            return Status::failure(id + ": a step has no id");
        if (!ids.insert(s.id).second)
            return Status::failure(id + ": the step id '" + s.id + "' is used twice");
        if (std::find(known.begin(), known.end(), s.action) == known.end())
            return Status::failure(id + "/" + s.id + ": unknown action '" + s.action + "'");
        if (!(s.timeoutSeconds > 0.0))
            return Status::failure(id + "/" + s.id + ": the timeout must be positive");
        const auto need = [&](const char* key) -> Status {
            if (!s.params.contains(key))
                return Status::failure(id + "/" + s.id + ": the action '" + s.action + "' needs the parameter '" + key + "'");
            return Status::success();
        };
        if (s.action == "open-project")
            if (Status st = need("path"); !st)
                return st;
        if (s.action == "workspace")
            if (Status st = need("panel"); !st)
                return st;
        if (s.action == "select")
            if (Status st = need("object"); !st)
                return st;
        if (s.action == "run")
            if (Status st = need("what"); !st)
                return st;
        if (s.action == "help")
            if (Status st = need("topic"); !st)
                return st;
        if (s.action == "capture")
            if (Status st = need("name"); !st)
                return st;
        if (s.action == "python")
            if (Status st = need("code"); !st)
                return st;
        if (s.action == "experiment-data")
            if (Status st = need("op"); !st)
                return st;
        if (s.action == "workflow" || s.action == "project-action")
            if (Status st = need("id"); !st)
                return st;
        if (s.action == "capture") {
            const std::string file = s.params["name"].get<std::string>();
            if (file.find('/') != std::string::npos || file.size() < 5 || file.substr(file.size() - 4) != ".png")
                return Status::failure(id + "/" + s.id + ": a screenshot name is a file name ending in .png");
        }
    }
    return Status::success();
}

Status DemoProfile::validate() const
{
    if (id.empty() || name.empty())
        return Status::failure("a demonstration profile needs an id and a name");
    if (status != "available" && status != "partial" && status != "planned")
        return Status::failure(id + ": the status must be available, partial or planned");
    if (status != "planned" && sequence.empty())
        return Status::failure(id + ": an available demonstration needs a sequence");
    if (status == "partial" && remaining.empty())
        return Status::failure(id + ": a partial demonstration says what remains");
    if (story.problem.empty() || story.calculate.empty() || story.assumptions.empty())
        return Status::failure(id + ": the story must say the problem, what Resonant calculates and the assumptions");
    return Status::success();
}

Result<DemoSequence> sequenceFromJson(const json& j)
{
    using R = Result<DemoSequence>;
    try {
        if (j.value("format", std::string()) != kSequenceFormat)
            return R::failure(std::string("not a demonstration sequence (format must be '") + kSequenceFormat + "')");
        if (j.value("version", 0) != kFormatVersion)
            return R::failure("unsupported demonstration sequence version " + std::to_string(j.value("version", 0)));
        DemoSequence s;
        s.id = j.at("id").get<std::string>();
        s.version = j.value("sequenceVersion", std::string("1"));
        s.name = j.value("name", s.id);
        for (const json& step : j.at("steps")) {
            DemoStep d;
            d.id = step.at("id").get<std::string>();
            d.action = step.at("action").get<std::string>();
            d.description = step.value("description", std::string());
            d.params = step.value("params", json::object());
            d.expect = step.value("expect", json::object());
            d.timeoutSeconds = step.value("timeoutSeconds", 60.0);
            const std::string f = step.value("onFailure", std::string("stop"));
            if (f != "stop" && f != "continue")
                return R::failure(s.id + "/" + d.id + ": onFailure must be stop or continue");
            d.onFailure = f == "continue" ? FailureBehaviour::Continue : FailureBehaviour::Stop;
            d.notes = step.value("notes", std::string());
            d.screenshot = step.value("screenshot", std::string());
            s.steps.push_back(std::move(d));
        }
        if (Status st = s.validate(); !st)
            return R::failure(st.message());
        return s;
    } catch (const std::exception& e) {
        return R::failure(std::string("demonstration sequence: ") + e.what());
    }
}

json toJson(const DemoSequence& s)
{
    json steps = json::array();
    for (const DemoStep& d : s.steps) {
        json step = {{"id", d.id}, {"action", d.action}, {"description", d.description}, {"params", d.params},
                     {"timeoutSeconds", d.timeoutSeconds}, {"onFailure", d.onFailure == FailureBehaviour::Continue ? "continue" : "stop"}};
        if (!d.expect.empty())
            step["expect"] = d.expect;
        if (!d.notes.empty())
            step["notes"] = d.notes;
        if (!d.screenshot.empty())
            step["screenshot"] = d.screenshot;
        steps.push_back(step);
    }
    return {{"format", kSequenceFormat}, {"version", kFormatVersion}, {"id", s.id}, {"sequenceVersion", s.version}, {"name", s.name}, {"steps", steps}};
}

Result<DemoProfile> profileFromJson(const json& j)
{
    using R = Result<DemoProfile>;
    try {
        if (j.value("format", std::string()) != kProfileFormat)
            return R::failure(std::string("not a demonstration profile (format must be '") + kProfileFormat + "')");
        if (j.value("version", 0) != kFormatVersion)
            return R::failure("unsupported demonstration profile version " + std::to_string(j.value("version", 0)));
        DemoProfile p;
        p.id = j.at("id").get<std::string>();
        p.name = j.at("name").get<std::string>();
        p.description = j.value("description", std::string());
        p.audience = j.value("audience", std::string());
        p.domain = j.value("domain", std::string());
        p.status = j.value("status", std::string("available"));
        p.project = j.value("project", std::string());
        p.workspace = j.value("workspace", std::string());
        p.requires_ = listOf<std::string>(j, "requires");
        p.data = listOf<std::string>(j, "data");
        p.sequence = j.value("sequence", std::string());
        p.expected = listOf<std::string>(j, "expected");
        p.camera = j.value("camera", json::object());
        p.visualization = j.value("visualization", json::object());
        p.screenshots = listOf<std::string>(j, "screenshots");
        p.notes = j.value("notes", std::string());
        p.durationMinutes = j.value("durationMinutes", 0.0);
        p.completion = listOf<std::string>(j, "completion");
        p.limitations = listOf<std::string>(j, "limitations");
        p.remaining = listOf<std::string>(j, "remaining");
        const json story = j.value("story", json::object());
        p.story = {story.value("problem", std::string()),   story.value("configure", std::string()), story.value("calculate", std::string()),
                   story.value("meaning", std::string()),   story.value("inspect", std::string()),   story.value("value", std::string()),
                   story.value("assumptions", std::string())};
        if (Status st = p.validate(); !st)
            return R::failure(st.message());
        return p;
    } catch (const std::exception& e) {
        return R::failure(std::string("demonstration profile: ") + e.what());
    }
}

json toJson(const DemoProfile& p)
{
    return {{"format", kProfileFormat},
            {"version", kFormatVersion},
            {"id", p.id},
            {"name", p.name},
            {"description", p.description},
            {"audience", p.audience},
            {"domain", p.domain},
            {"status", p.status},
            {"project", p.project},
            {"workspace", p.workspace},
            {"requires", p.requires_},
            {"data", p.data},
            {"sequence", p.sequence},
            {"expected", p.expected},
            {"camera", p.camera},
            {"visualization", p.visualization},
            {"screenshots", p.screenshots},
            {"notes", p.notes},
            {"durationMinutes", p.durationMinutes},
            {"completion", p.completion},
            {"limitations", p.limitations},
            {"remaining", p.remaining},
            {"story",
             {{"problem", p.story.problem},
              {"configure", p.story.configure},
              {"calculate", p.story.calculate},
              {"meaning", p.story.meaning},
              {"inspect", p.story.inspect},
              {"value", p.story.value},
              {"assumptions", p.story.assumptions}}}};
}

Result<DemoSequence> loadSequence(const std::filesystem::path& file)
{
    std::ifstream in(file);
    if (!in)
        return Result<DemoSequence>::failure("cannot read " + file.string());
    try {
        auto s = sequenceFromJson(json::parse(in));
        if (!s)
            return Result<DemoSequence>::failure(file.filename().string() + ": " + s.message());
        return s;
    } catch (const std::exception& e) {
        return Result<DemoSequence>::failure(file.filename().string() + ": " + e.what());
    }
}

const LoadedDemo* DemoLibrary::find(const std::string& id) const
{
    for (const LoadedDemo& d : demos)
        if (d.profile.id == id)
            return &d;
    return nullptr;
}

DemoLibrary loadDemoLibrary(const std::filesystem::path& root)
{
    DemoLibrary library;
    std::error_code error;
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(root / "demos" / "profiles", error))
        if (entry.path().extension() == ".json")
            files.push_back(entry.path());
    if (error)
        library.errors.push_back("no demonstration profiles under " + (root / "demos" / "profiles").string());
    std::sort(files.begin(), files.end());
    for (const auto& file : files) {
        try {
            auto p = profileFromJson(json::parse(std::ifstream(file)));
            if (!p) {
                library.errors.push_back(file.filename().string() + ": " + p.message());
                continue;
            }
            LoadedDemo d{p.take(), {}};
            if (!d.profile.sequence.empty()) {
                auto s = loadSequence(root / d.profile.sequence);
                if (!s) {
                    library.errors.push_back(d.profile.id + ": " + s.message());
                    continue;
                }
                d.sequence = s.take();
            }
            library.demos.push_back(std::move(d));
        } catch (const std::exception& e) {
            library.errors.push_back(file.filename().string() + ": " + e.what());
        }
    }
    return library;
}

std::string_view toString(StepOutcome o)
{
    switch (o) {
    case StepOutcome::Done: return "done";
    case StepOutcome::Failed: return "failed";
    case StepOutcome::Skipped: return "skipped";
    case StepOutcome::Cancelled: return "cancelled";
    }
    return "failed";
}

json toJson(const DemoRun& r)
{
    json steps = json::array();
    for (const StepRun& s : r.steps)
        steps.push_back({{"id", s.id}, {"action", s.action}, {"outcome", toString(s.outcome)}, {"message", s.message},
                         {"startedUtc", s.startedUtc}, {"seconds", s.seconds}, {"result", s.result}});
    return {{"format", kRunFormat},
            {"version", kFormatVersion},
            {"profile", r.profileId},
            {"sequence", r.sequenceId},
            {"sequenceVersion", r.sequenceVersion},
            {"applicationVersion", r.applicationVersion},
            {"project", {{"file", r.projectFile}, {"id", r.projectId}, {"sha256", r.projectSha256}}},
            {"simulation", r.simulation},
            {"screenshots", r.screenshots},
            {"notes", r.notes},
            {"status", r.status},
            {"message", r.message},
            {"startedUtc", r.startedUtc},
            {"finishedUtc", r.finishedUtc},
            {"steps", steps}};
}

// --- SHA-256 (FIPS 180-4), for the project version recorded in a run manifest.
namespace {
constexpr std::array<uint32_t, 64> kK = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be,
    0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa,
    0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85,
    0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f,
    0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
inline uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
}  // namespace

std::string sha256OfFile(const std::filesystem::path& file)
{
    std::ifstream in(file, std::ios::binary);
    if (!in)
        return {};
    std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const uint64_t bits = static_cast<uint64_t>(data.size()) * 8;
    data.push_back(static_cast<char>(0x80));
    while (data.size() % 64 != 56)
        data.push_back('\0');
    for (int i = 7; i >= 0; --i)
        data.push_back(static_cast<char>((bits >> (8 * i)) & 0xff));
    std::array<uint32_t, 8> h = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    for (size_t chunk = 0; chunk < data.size(); chunk += 64) {
        std::array<uint32_t, 64> w{};
        for (int i = 0; i < 16; ++i)
            w[i] = (static_cast<uint32_t>(static_cast<unsigned char>(data[chunk + 4 * i])) << 24) |
                   (static_cast<uint32_t>(static_cast<unsigned char>(data[chunk + 4 * i + 1])) << 16) |
                   (static_cast<uint32_t>(static_cast<unsigned char>(data[chunk + 4 * i + 2])) << 8) |
                   static_cast<uint32_t>(static_cast<unsigned char>(data[chunk + 4 * i + 3]));
        for (int i = 16; i < 64; ++i) {
            const uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], k = h[7];
        for (int i = 0; i < 64; ++i) {
            const uint32_t t1 = k + (rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25)) + ((e & f) ^ (~e & g)) + kK[i] + w[i];
            const uint32_t t2 = (rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
            k = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += k;
    }
    std::ostringstream out;
    for (uint32_t v : h)
        out << std::hex << std::setw(8) << std::setfill('0') << v;
    return out.str();
}

}  // namespace resonant::demo
