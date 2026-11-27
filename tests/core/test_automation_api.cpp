// The protobuf adapter and the local endpoint (REQ-CHAT-20261126-012,
// RES-API-PB-*, RES-API-SEC-*): the dispatcher implements exactly the methods
// of the contract's descriptor set, round-trips messages, enforces the
// session policy, batches atomically, and the endpoint admits only the same
// user with the token. Built only with the automation API (protobuf).
#ifdef RESONANT_HAVE_AUTOMATION_API

#include "Dispatcher.h"
#include "LocalEndpoint.h"
#include "project/Project.h"
#include "resonant/transport/v1/local.pb.h"
#include "resonant/v1/application/application_service.pb.h"
#include "resonant/v1/automation/session_service.pb.h"
#include "resonant/v1/automation/transaction_service.pb.h"
#include "resonant/v1/geometry/geometry_service.pb.h"
#include "resonant/v1/project/project_service.pb.h"
#include "resonant/v1/results/results_service.pb.h"
#include "resonant/v1/scene/scene_service.pb.h"
#include "resonant/v1/simulation/simulation_service.pb.h"
#include "coupling/WorkflowActions.h"
#include "services/Services.h"

#include <google/protobuf/descriptor.pb.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <set>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h>

using namespace resonant;
using namespace resonant::automation_api;
namespace v1 = ::resonant::v1;

namespace {

std::filesystem::path descriptorSet()
{
    std::error_code error;
    const auto exe = std::filesystem::read_symlink("/proc/self/exe", error);
    return std::filesystem::weakly_canonical(exe.parent_path() / ".." / ".." / "src" / "automation_api" / "resonant_api.desc");
}

struct Api {
    services::OwnedProjectHost host{Project::createNew("api")};
    services::Session session{host};
    Dispatcher dispatcher{session};
    Api() { host.setSession(&session); }

    template <class Response, class Request>
    Response call(const std::string& method, Request request, const std::string& sessionId, int* status = nullptr, v1::Error* error = nullptr)
    {
        request.mutable_header()->set_session_id(sessionId);
        const Reply r = dispatcher.call(method, request.SerializeAsString(), services::Interface::Grpc);
        if (status)
            *status = r.status;
        if (error && !r.error.empty())
            error->ParseFromString(r.error);
        Response out;
        out.ParseFromString(r.payload);
        return out;
    }
    std::string open(const std::string& client, std::vector<v1::Permission> permissions = {})
    {
        v1::OpenSessionRequest r;
        r.set_client_name(client);
        for (auto p : permissions)
            r.add_requested_permissions(p);
        return call<v1::OpenSessionResponse>("/resonant.v1.SessionService/OpenSession", r, "").session().session_id();
    }
};

}  // namespace

TEST(AutomationApi, TheDispatcherImplementsExactlyTheContractsMethods)
{
    std::ifstream in(descriptorSet(), std::ios::binary);
    ASSERT_TRUE(in) << descriptorSet();
    google::protobuf::FileDescriptorSet set;
    ASSERT_TRUE(set.ParseFromIstream(&in));
    std::map<std::string, bool> contract;  // method -> server streaming
    for (const auto& file : set.file())
        if (file.package() == "resonant.v1")
            for (const auto& service : file.service())
                for (const auto& method : service.method())
                    contract["/resonant.v1." + service.name() + "/" + method.name()] = method.server_streaming();
    Api api;
    std::map<std::string, bool> implemented;
    for (const MethodInfo& m : api.dispatcher.methods())
        implemented[m.method] = m.streaming;
    EXPECT_EQ(implemented, contract);
    EXPECT_GE(contract.size(), 60u);
    // Mutating methods require the modify (or files) permission; streams are reads.
    for (const MethodInfo& m : api.dispatcher.methods()) {
        if (m.mutating) {
            EXPECT_TRUE(m.permission == services::Permission::Modify || m.permission == services::Permission::Files) << m.method;
        }
        if (m.streaming) {
            EXPECT_EQ(m.permission, services::Permission::Read) << m.method;
        }
    }
}

TEST(AutomationApi, UnaryCallsRoundTripAndErrorsAreStructured)
{
    Api api;
    // Without a session a remote caller may only ask what the application is.
    int status = -1;
    v1::Error error;
    auto info = api.call<v1::GetApplicationInfoResponse>("/resonant.v1.ApplicationService/GetApplicationInfo", v1::GetApplicationInfoRequest(), "", &status);
    EXPECT_EQ(status, 0);
    EXPECT_EQ(info.info().api_version(), "resonant.v1");
    api.call<v1::GetProjectResponse>("/resonant.v1.ProjectService/GetProject", v1::GetProjectRequest(), "", &status, &error);
    EXPECT_EQ(status, 7);  // PERMISSION_DENIED
    EXPECT_EQ(error.code(), v1::ERROR_CODE_PERMISSION_DENIED);
    EXPECT_FALSE(error.request_id().empty());

    const std::string s = api.open("round trip");
    ASSERT_FALSE(s.empty());
    v1::CreatePrimitiveRequest create;
    create.set_kind_id("cylinder");
    (*create.mutable_parameters())["radius"] = 0.05;
    create.mutable_local_transform()->mutable_position_meters()->set_z_meters(0.1);
    auto created = api.call<v1::CreatePrimitiveResponse>("/resonant.v1.GeometryService/CreatePrimitive", create, s, &status);
    ASSERT_EQ(status, 0);
    const std::string id = created.object().object_id();
    EXPECT_EQ(created.object().geometry().kind_id(), "cylinder");
    EXPECT_DOUBLE_EQ(created.object().geometry().parameters().at("radius"), 0.05);
    EXPECT_DOUBLE_EQ(created.object().world_position_meters().z_meters(), 0.1);
    EXPECT_EQ(created.header().project_revision(), created.object().revision());
    // Only the position is given: rotation and scale stay.
    v1::UpdateObjectRequest move;
    move.set_object_id(id);
    move.mutable_local_transform()->mutable_position_meters()->set_x_meters(0.2);
    auto moved = api.call<v1::UpdateObjectResponse>("/resonant.v1.SceneService/UpdateObject", move, s, &status);
    ASSERT_EQ(status, 0);
    EXPECT_DOUBLE_EQ(moved.object().local_transform().position_meters().x_meters(), 0.2);
    EXPECT_DOUBLE_EQ(moved.object().local_transform().scale().x(), 1.0);
    // A conflict carries the current revision.
    move.set_expected_revision(1);
    api.call<v1::UpdateObjectResponse>("/resonant.v1.SceneService/UpdateObject", move, s, &status, &error);
    EXPECT_EQ(status, 10);  // ABORTED
    EXPECT_EQ(error.code(), v1::ERROR_CODE_CONFLICT);
    EXPECT_EQ(error.current_revision(), moved.object().revision());
    // Field violations name the field.
    (*create.mutable_parameters())["radius"] = -1.0;
    api.call<v1::CreatePrimitiveResponse>("/resonant.v1.GeometryService/CreatePrimitive", create, s, &status, &error);
    EXPECT_EQ(status, 3);
    ASSERT_GT(error.violations_size(), 0);
    EXPECT_EQ(error.violations(0).field(), "parameters.radius");
    // An unknown method and a malformed request.
    const Reply unknown = api.dispatcher.call("/resonant.v1.SceneService/Teleport", "", services::Interface::Grpc);
    EXPECT_EQ(unknown.status, 12);
    const Reply garbage = api.dispatcher.call("/resonant.v1.SceneService/GetObject", std::string("\xff\xff\xff", 3), services::Interface::Grpc);
    EXPECT_EQ(garbage.status, 3);
}

TEST(AutomationApi, RemoteSessionsGetNoMoreThanThePolicy)
{
    Api api;  // default policy: read, modify, execute
    const std::string s = api.open("greedy", {v1::PERMISSION_READ, v1::PERMISSION_FILES, v1::PERMISSION_ADMIN});
    v1::ListSessionsRequest list;
    auto sessions = api.call<v1::ListSessionsResponse>("/resonant.v1.SessionService/ListSessions", list, s);
    ASSERT_EQ(sessions.sessions_size(), 1);  // without admin: only its own
    const auto& permissions = sessions.sessions(0).permissions();
    EXPECT_EQ(std::set<int>(permissions.begin(), permissions.end()), std::set<int>({v1::PERMISSION_READ}));
    int status = -1;
    v1::SaveProjectRequest save;
    save.set_file_name("x.resonant");
    api.call<v1::SaveProjectResponse>("/resonant.v1.ProjectService/SaveProject", save, s, &status);
    EXPECT_EQ(status, 7);
    // A remote client cannot claim to be the GUI.
    v1::OpenSessionRequest gui;
    gui.set_client_name("impostor");
    gui.set_source_interface(v1::SOURCE_INTERFACE_GUI);
    auto opened = api.call<v1::OpenSessionResponse>("/resonant.v1.SessionService/OpenSession", gui, "");
    EXPECT_EQ(opened.session().source_interface(), v1::SOURCE_INTERFACE_GRPC);
}

TEST(AutomationApi, BatchesThroughTheContractAreAtomic)
{
    Api api;
    const std::string s = api.open("batch");
    v1::ExecuteBatchRequest batch;
    batch.set_description("two shapes and a bad one");
    auto* a = batch.add_operations();
    a->set_operation_id("a");
    a->mutable_create_primitive()->set_kind_id("box");
    auto* b = batch.add_operations();
    b->set_operation_id("b");
    b->mutable_create_array()->set_count_x(-3);
    int status = -1;
    auto failed = api.call<v1::ExecuteBatchResponse>("/resonant.v1.TransactionService/ExecuteBatch", batch, s, &status);
    ASSERT_EQ(status, 0);
    EXPECT_FALSE(failed.committed());
    EXPECT_TRUE(failed.results(0).succeeded());
    EXPECT_FALSE(failed.results(1).succeeded());
    EXPECT_EQ(api.host.project().scene.objectCount(), 0u);
    b->mutable_create_array()->set_count_x(4);
    const std::size_t steps = api.host.commands().size();
    auto ok = api.call<v1::ExecuteBatchResponse>("/resonant.v1.TransactionService/ExecuteBatch", batch, s, &status);
    ASSERT_EQ(status, 0);
    EXPECT_TRUE(ok.committed());
    EXPECT_EQ(ok.results(1).created_ids_size(), 1);
    EXPECT_EQ(api.host.project().scene.objectCount(), 2u);
    EXPECT_EQ(api.host.commands().size(), steps + 1);
}

TEST(AutomationApi, FieldSlicesStreamInChunks)
{
    Api api;
    const std::string s = api.open("field");
    ASSERT_TRUE(api.session.services().arrays.create({}, "Array", std::nullopt, Id(), {{"countX", 4}}).ok());
    ASSERT_TRUE(coupling::addSceneObject(api.host.project(), {{"type", "region"}, {"id", "plane"}, {"position", {0, 0, 0.05}},
                                                              {"component", {{"countU", 30}, {"countV", 20}}}})
                    .ok());
    v1::StartAcousticFieldSimulationRequest start;
    start.set_region_id("plane");
    int status = -1;
    auto started = api.call<v1::StartAcousticFieldSimulationResponse>("/resonant.v1.SimulationService/StartAcousticFieldSimulation", start, s, &status);
    ASSERT_EQ(status, 0);
    for (int k = 0; k < 500 && api.host.jobs().hasUnfinishedJobs(); ++k)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    api.session.collectFinishedJobs();
    const auto results = api.session.results();
    ASSERT_EQ(results.size(), 1u);
    v1::ReadFieldSliceRequest read;
    read.mutable_header()->set_session_id(s);
    read.set_result_id(results.front()->id);
    read.set_chunk_samples(256);
    std::vector<v1::FieldDataChunk> chunks;
    const Reply end = api.dispatcher.stream("/resonant.v1.ResultsService/ReadFieldSlice", read.SerializeAsString(), services::Interface::Grpc,
                                            [&](const std::string& m) {
                                                if (!m.empty())
                                                    chunks.emplace_back().ParseFromString(m);
                                                return true;
                                            });
    EXPECT_EQ(end.status, 0);
    ASSERT_EQ(chunks.size(), 3u);  // 600 samples in chunks of 256
    EXPECT_EQ(chunks[0].rows(), 30);
    EXPECT_EQ(chunks[0].columns(), 20);
    EXPECT_EQ(chunks[2].offset(), 512);
    EXPECT_TRUE(chunks[2].last());
    std::complex<float> first;
    std::memcpy(&first, chunks[0].samples().data(), sizeof first);
    EXPECT_EQ(first, results.front()->field->pressure[0]);
}

TEST(AutomationApi, TheLocalEndpointAdmitsOnlyTheTokenHolder)
{
    Api api;
    const auto dir = std::filesystem::temp_directory_path() / ("resonant-endpoint-test-" + std::to_string(::getpid()));
    std::filesystem::remove_all(dir);
    const std::string path = (dir / "a.sock").string();
    LocalEndpoint endpoint(api.dispatcher);
    const std::string token = LocalEndpoint::newToken();
    ASSERT_EQ(token.size(), 64u);
    ASSERT_EQ(endpoint.start(path, token), "");
    struct stat st{};
    ASSERT_EQ(::stat(dir.c_str(), &st), 0);
    EXPECT_EQ(st.st_mode & 0777, 0700u);
    ASSERT_EQ(::stat(path.c_str(), &st), 0);
    EXPECT_EQ(st.st_mode & 0777, 0600u);

    const auto roundTrip = [&](const std::string& withToken) {
        const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        sockaddr_un address{};
        address.sun_family = AF_UNIX;
        std::strncpy(address.sun_path, path.c_str(), sizeof(address.sun_path) - 1);
        EXPECT_EQ(::connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof address), 0);
        transport::v1::LocalRequest request;
        request.set_method("/resonant.v1.ApplicationService/GetApplicationInfo");
        request.set_payload(v1::GetApplicationInfoRequest().SerializeAsString());
        (*request.mutable_metadata())["token"] = withToken;
        const std::string data = request.SerializeAsString();
        const std::uint32_t size = static_cast<std::uint32_t>(data.size());
        std::string frame(reinterpret_cast<const char*>(&size), 4);
        frame += data;
        EXPECT_EQ(::send(fd, frame.data(), frame.size(), 0), static_cast<ssize_t>(frame.size()));
        std::uint32_t length = 0;
        EXPECT_EQ(::recv(fd, &length, 4, MSG_WAITALL), 4);
        std::string body(length, '\0');
        EXPECT_EQ(::recv(fd, body.data(), length, MSG_WAITALL), static_cast<ssize_t>(length));
        ::close(fd);
        transport::v1::LocalResponse response;
        response.ParseFromString(body);
        return response;
    };
    EXPECT_EQ(roundTrip("not the token").status(), 16);  // UNAUTHENTICATED
    const auto ok = roundTrip(token);
    EXPECT_EQ(ok.status(), 0);
    v1::GetApplicationInfoResponse info;
    ASSERT_TRUE(info.ParseFromString(ok.payload()));
    EXPECT_EQ(info.info().application_name(), "Resonant");
    // Many short connections: the endpoint keeps serving and lets its threads go.
    for (int k = 0; k < 50; ++k)
        EXPECT_EQ(roundTrip(token).status(), 0);
    endpoint.stop();
    EXPECT_FALSE(std::filesystem::exists(path));
    std::filesystem::remove_all(dir);
}

TEST(AutomationApi, ConcurrentClientsAreSerializedWithoutLostUpdates)
{
    Api api;
    std::mutex owner;
    Dispatcher locked(
        api.session,
        [&owner](const std::function<void()>& f) {
            std::lock_guard lock(owner);
            f();
        });
    std::vector<std::string> sessions;
    for (int k = 0; k < 4; ++k) {
        v1::OpenSessionRequest r;
        r.set_client_name("client " + std::to_string(k));
        v1::OpenSessionResponse out;
        out.ParseFromString(locked.call("/resonant.v1.SessionService/OpenSession", r.SerializeAsString(), services::Interface::Grpc).payload);
        sessions.push_back(out.session().session_id());
    }
    const std::uint64_t before = api.session.projectRevision();
    std::vector<std::thread> threads;
    std::atomic<int> failures{0};
    for (int k = 0; k < 4; ++k)
        threads.emplace_back([&, k] {
            for (int i = 0; i < 25; ++i) {
                v1::CreatePrimitiveRequest r;
                r.mutable_header()->set_session_id(sessions[k]);
                r.set_kind_id("box");
                if (locked.call("/resonant.v1.GeometryService/CreatePrimitive", r.SerializeAsString(), services::Interface::Grpc).status != 0)
                    ++failures;
            }
        });
    for (auto& t : threads)
        t.join();
    EXPECT_EQ(failures.load(), 0);
    EXPECT_EQ(api.host.project().scene.objectCount(), 100u);
    EXPECT_EQ(api.session.projectRevision(), before + 100);
    // Every creation is one event, in one order, attributed to the session that made it.
    std::map<std::string, int> perSession;
    std::uint64_t last = 0;
    for (const auto& e : api.session.events().since(0).events) {
        EXPECT_GT(e.sequence, last);
        last = e.sequence;
        if (e.type == "scene.object.created")
            ++perSession[e.sessionId];
    }
    for (const std::string& s : sessions)
        EXPECT_EQ(perSession[s], 25) << s;
}

#endif  // RESONANT_HAVE_AUTOMATION_API
