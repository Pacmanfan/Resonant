#include "project/Project.h"
#include "project/ProjectSerializer.h"
#include "services/Services.h"

#include <algorithm>

namespace resonant::services {

namespace {
void merge(ChangeSummary& into, const ChangeSummary& from)
{
    const auto add = [](std::vector<Id>& v, const Id& id) {
        if (std::find(v.begin(), v.end(), id) == v.end())
            v.push_back(id);
    };
    for (const Id& id : from.created)
        add(into.created, id);
    for (const Id& id : from.updated)
        if (std::find(into.created.begin(), into.created.end(), id) == into.created.end())
            add(into.updated, id);
    for (const Id& id : from.deleted) {
        into.created.erase(std::remove(into.created.begin(), into.created.end(), id), into.created.end());
        into.updated.erase(std::remove(into.updated.begin(), into.updated.end(), id), into.updated.end());
        add(into.deleted, id);
    }
    into.materials = into.materials || from.materials;
    into.settings = into.settings || from.settings;
}
}  // namespace

Outcome<TransactionService::BatchResult> TransactionService::run(const CallContext& c, const std::vector<Operation>& operations,
                                                                 const std::string& description, bool commitAtEnd, std::optional<std::uint64_t> baseRevision)
{
    auto a = s_.authorize(c, Permission::Modify);
    if (!a)
        return a.error();
    if (operations.empty())
        return invalid("a batch needs at least one operation", "operations");
    if (operations.size() > 10000)
        return ServiceError::make(ErrorCode::ResourceExhausted, "at most 10 000 operations per batch");
    if (auto replayed = s_.replay(a.value(), "TransactionService.ExecuteBatch")) {
        if (!*replayed)
            return replayed->error();
        BatchResult r;
        r.committed = true;
        r.mutation = replayed->value();
        return r;
    }
    const std::uint64_t live = s_.projectRevision();
    if (baseRevision && *baseRevision != live) {
        ServiceError e = ServiceError::make(ErrorCode::Conflict, "the project changed since the transaction began (revision " + std::to_string(live) +
                                                                     ", began at " + std::to_string(*baseRevision) + ")");
        e.currentRevision = live;
        return e;
    }
    if (a.value().expectedProjectRevision && *a.value().expectedProjectRevision != live) {
        ServiceError e = ServiceError::make(ErrorCode::Conflict, "the project changed (revision " + std::to_string(live) + ")");
        e.currentRevision = live;
        return e;
    }
    // Stage on a copy with the same revisions: the operations' own revision checks still hold.
    auto copy = deserializeProject(serializeProject(s_.project()));
    if (!copy)
        return ServiceError::make(ErrorCode::Internal, "the project could not be copied: " + copy.message());
    OwnedProjectHost host(std::move(copy.value().project), 1);
    Session staging(host, "staging");
    host.setSession(&staging);
    staging.seedRevisions(s_);
    CallContext inner = a.value();
    inner.sessionId = Session::kApplicationSession;
    inner.source = Interface::Internal;
    inner.dryRun = false;
    inner.idempotencyKey.clear();
    inner.expectedProjectRevision.reset();

    BatchResult result;
    bool failed = false;
    for (const Operation& op : operations) {
        OperationResult r;
        r.id = op.id;
        if (failed) {
            r.error = ServiceError::make(ErrorCode::FailedPrecondition, "not run: an earlier operation failed (the batch is atomic)");
            result.results.push_back(std::move(r));
            continue;
        }
        auto outcome = op.apply(staging.services(), inner);
        if (outcome) {
            r.succeeded = true;
            r.changes = outcome.value().changes;
            merge(result.mutation.changes, r.changes);
        } else {
            r.error = outcome.error();
            failed = true;
        }
        result.results.push_back(std::move(r));
    }
    result.mutation.revisionBefore = result.mutation.revisionAfter = live;
    if (failed || a.value().dryRun || !commitAtEnd) {
        result.mutation.dryRun = a.value().dryRun || !commitAtEnd;
        AuditRecord r;
        r.requestId = a.value().requestId;
        r.sessionId = a.value().sessionId;
        r.clientName = a.value().clientName;
        r.source = a.value().source;
        r.operation = "TransactionService.ExecuteBatch";
        r.revisionBefore = r.revisionAfter = live;
        r.succeeded = !failed;
        r.dryRun = result.mutation.dryRun;
        if (failed)
            for (const OperationResult& o : result.results)
                if (o.error && o.error->code != ErrorCode::FailedPrecondition) {
                    r.error = o.error;
                    break;
                }
        s_.record(std::move(r));
        return result;
    }
    // One undoable step for the whole batch.
    const std::string name = description.empty() ? "Batch of " + std::to_string(operations.size()) + " operation(s)" : description;
    auto committed = s_.applyCopy(a.value(), "TransactionService.ExecuteBatch", host.project(), name);
    if (!committed)
        return committed.error();
    result.committed = true;
    result.mutation.revisionAfter = committed.value().revisionAfter;
    result.mutation.changes.description = committed.value().changes.description;
    return result;
}

Outcome<TransactionService::BatchResult> TransactionService::executeBatch(const CallContext& c, const std::vector<Operation>& operations,
                                                                          const std::string& description)
{
    return run(c, operations, description, true, std::nullopt);
}

Outcome<TransactionService::TransactionView> TransactionService::begin(const CallContext& c, const std::string& description)
{
    auto a = s_.authorize(c, Permission::Modify);
    if (!a)
        return a.error();
    Staged t;
    t.view.id = "T-" + Id::generate().str();
    t.view.state = "open";
    t.view.description = description;
    t.view.baseRevision = s_.projectRevision();
    t.sessionId = a.value().sessionId;
    const TransactionView v = t.view;
    transactions_[v.id] = std::move(t);
    return v;
}

namespace {
template <class Map>
auto findOwned(Map& transactions, const std::string& id, const std::string& sessionId) -> decltype(&transactions.begin()->second)
{
    const auto it = transactions.find(id);
    if (it == transactions.end() || it->second.sessionId != sessionId)
        return nullptr;
    return &it->second;
}
}  // namespace

Outcome<TransactionService::TransactionView> TransactionService::stage(const CallContext& c, const std::string& id, std::vector<Operation> operations)
{
    auto a = s_.authorize(c, Permission::Modify);
    if (!a)
        return a.error();
    auto* t = findOwned(transactions_, id, a.value().sessionId);
    if (!t)
        return notFound("there is no transaction '" + id + "' of this session", "transaction", id);
    if (t->view.state != "open")
        return ServiceError::make(ErrorCode::FailedPrecondition, "the transaction is " + t->view.state);
    for (Operation& op : operations)
        t->operations.push_back(std::move(op));
    t->view.staged = static_cast<int>(t->operations.size());
    return t->view;
}

Outcome<std::pair<TransactionService::TransactionView, std::vector<TransactionService::OperationResult>>> TransactionService::validate(
    const CallContext& c, const std::string& id)
{
    auto a = s_.authorize(c, Permission::Modify);
    if (!a)
        return a.error();
    auto* t = findOwned(transactions_, id, a.value().sessionId);
    if (!t)
        return notFound("there is no transaction '" + id + "' of this session", "transaction", id);
    auto r = run(a.value(), t->operations, t->view.description, false, t->view.baseRevision);
    t->view.validation = {};
    if (!r) {
        t->view.validation.valid = false;
        t->view.validation.issues.push_back({3, "", r.error().message, r.error().resourceId});
        return std::make_pair(t->view, std::vector<OperationResult>{});
    }
    for (const OperationResult& o : r.value().results)
        if (o.error && o.error->code != ErrorCode::FailedPrecondition)
            t->view.validation.issues.push_back({3, o.id, o.error->message, o.error->resourceId});
    t->view.validation.valid = t->view.validation.issues.empty();
    return std::make_pair(t->view, r.value().results);
}

Outcome<std::pair<TransactionService::TransactionView, TransactionService::BatchResult>> TransactionService::commit(const CallContext& c,
                                                                                                                   const std::string& id)
{
    auto a = s_.authorize(c, Permission::Modify);
    if (!a)
        return a.error();
    auto* t = findOwned(transactions_, id, a.value().sessionId);
    if (!t)
        return notFound("there is no transaction '" + id + "' of this session", "transaction", id);
    if (t->view.state != "open")
        return ServiceError::make(ErrorCode::FailedPrecondition, "the transaction is " + t->view.state);
    CallContext context = a.value();
    context.dryRun = false;
    auto r = run(context, t->operations, t->view.description, true, t->view.baseRevision);
    if (!r) {
        if (r.error().code == ErrorCode::Conflict)
            return r.error();  // still open: the client may rebase (abort and begin again)
        t->view.state = "failed";
        return r.error();
    }
    t->view.state = r.value().committed ? "committed" : "failed";
    return std::make_pair(t->view, r.take());
}

Outcome<TransactionService::TransactionView> TransactionService::abort(const CallContext& c, const std::string& id)
{
    auto a = s_.authorize(c, Permission::Modify);
    if (!a)
        return a.error();
    auto* t = findOwned(transactions_, id, a.value().sessionId);
    if (!t)
        return notFound("there is no transaction '" + id + "' of this session", "transaction", id);
    if (t->view.state == "open")
        t->view.state = "aborted";
    t->operations.clear();
    return t->view;
}

Outcome<TransactionService::TransactionView> TransactionService::get(const CallContext& c, const std::string& id)
{
    auto a = s_.authorize(c, Permission::Read);
    if (!a)
        return a.error();
    auto* t = findOwned(transactions_, id, a.value().sessionId);
    if (!t)
        return notFound("there is no transaction '" + id + "' of this session", "transaction", id);
    return t->view;
}

}  // namespace resonant::services
