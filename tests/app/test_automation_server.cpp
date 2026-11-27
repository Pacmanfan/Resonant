// The automation server in the real main window (REQ-CHAT-20261126-012,
// RES-API-GUI-*): a remote client's change is a GUI change (same document,
// undo history and panels), a change made in the window reaches remote
// clients as an event from the GUI, the dialog starts and stops the server
// and lists the sessions, and stopping with a stream open does not hang.
#ifdef RESONANT_HAVE_AUTOMATION_API

#include "AutomationServer.h"
#include "AutomationServerDialog.h"
#include "DocumentController.h"
#include "MainWindow.h"
#include "resonant/transport/v1/local.pb.h"
#include "resonant/v1/automation/event_service.pb.h"
#include "resonant/v1/automation/session_service.pb.h"
#include "resonant/v1/geometry/geometry_service.pb.h"
#include "resonant/v1/scene/scene_service.pb.h"
#include "services/Session.h"

#include <QElapsedTimer>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <gtest/gtest.h>

#include <atomic>
#include <functional>
#include <sys/socket.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h>

using namespace resonant;
namespace v1 = ::resonant::v1;

namespace {

bool waitUntil(const std::function<bool()>& condition, int ms = 20000)
{
    QElapsedTimer t;
    t.start();
    while (!condition()) {
        if (t.elapsed() > ms)
            return false;
        QTest::qWait(5);
    }
    return true;
}

// A minimal client of the local endpoint (one call per connection).
struct Client {
    std::string path, token;
    int open(const std::string& method, const std::string& payload) const
    {
        const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        sockaddr_un a{};
        a.sun_family = AF_UNIX;
        std::strncpy(a.sun_path, path.c_str(), sizeof(a.sun_path) - 1);
        if (::connect(fd, reinterpret_cast<sockaddr*>(&a), sizeof a) != 0) {
            ::close(fd);
            return -1;
        }
        transport::v1::LocalRequest r;
        r.set_method(method);
        r.set_payload(payload);
        (*r.mutable_metadata())["token"] = token;
        const std::string data = r.SerializeAsString();
        const std::uint32_t n = static_cast<std::uint32_t>(data.size());
        std::string frame(reinterpret_cast<const char*>(&n), 4);
        frame += data;
        ::send(fd, frame.data(), frame.size(), MSG_NOSIGNAL);
        return fd;
    }
    static bool next(int fd, transport::v1::LocalResponse& out)
    {
        std::uint32_t n = 0;
        if (::recv(fd, &n, 4, MSG_WAITALL) != 4)
            return false;
        std::string body(n, '\0');
        if (n && ::recv(fd, body.data(), n, MSG_WAITALL) != static_cast<ssize_t>(n))
            return false;
        return out.ParseFromString(body);
    }
    transport::v1::LocalResponse call(const std::string& method, const google::protobuf::Message& request) const
    {
        transport::v1::LocalResponse out;
        const int fd = open(method, request.SerializeAsString());
        if (fd >= 0) {
            next(fd, out);
            ::close(fd);
        }
        return out;
    }
};

// Runs a call on another thread while this (the GUI) thread keeps its event loop going.
transport::v1::LocalResponse callAway(const Client& c, const std::string& method, const google::protobuf::Message& request)
{
    transport::v1::LocalResponse out;
    std::atomic<bool> done{false};
    std::thread t([&] {
        out = c.call(method, request);
        done = true;
    });
    waitUntil([&] { return done.load(); });
    t.join();
    return out;
}

class AutomationServerTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        ASSERT_TRUE(dir_.isValid());
        window_ = std::make_unique<MainWindow>();
        AutomationServer::Options o;
        o.socketPath = dir_.filePath(QStringLiteral("gui.sock"));
        ASSERT_EQ(window_->automationServer().start(o), QString());
        QFile token(window_->automationServer().tokenFile());
        ASSERT_TRUE(token.open(QIODevice::ReadOnly));
        client_ = {o.socketPath.toStdString(), token.readAll().trimmed().toStdString()};
        v1::OpenSessionRequest open;
        open.set_client_name("conformance client");
        v1::OpenSessionResponse opened;
        opened.ParseFromString(callAway(client_, "/resonant.v1.SessionService/OpenSession", open).payload());
        session_ = opened.session().session_id();
        ASSERT_FALSE(session_.empty());
    }
    void TearDown() override { window_.reset(); }

    QTemporaryDir dir_;
    std::unique_ptr<MainWindow> window_;
    Client client_;
    std::string session_;
};

}  // namespace

TEST_F(AutomationServerTest, ARemoteChangeIsAGuiChange)
{
    DocumentController& doc = window_->controller();
    v1::CreatePrimitiveRequest create;
    create.mutable_header()->set_session_id(session_);
    create.set_kind_id("sphere");
    create.set_name("Remote sphere");
    const auto reply = callAway(client_, "/resonant.v1.GeometryService/CreatePrimitive", create);
    ASSERT_EQ(reply.status(), 0);
    v1::CreatePrimitiveResponse created;
    created.ParseFromString(reply.payload());
    const Id id(created.object().object_id());
    ASSERT_NE(doc.scene().find(id), nullptr);
    EXPECT_EQ(doc.scene().find(id)->state.name, "Remote sphere");
    EXPECT_TRUE(doc.isModified());
    EXPECT_TRUE(doc.commands().canUndo());
    // The window's undo undoes it, and the client sees that through the same revisions.
    const std::uint64_t revision = created.header().project_revision();
    doc.undo();
    EXPECT_EQ(doc.scene().find(id), nullptr);
    v1::GetObjectRequest get;
    get.mutable_header()->set_session_id(session_);
    get.set_object_id(id.str());
    EXPECT_EQ(callAway(client_, "/resonant.v1.SceneService/GetObject", get).status(), 5);  // NOT_FOUND
    EXPECT_GT(doc.session().projectRevision(), revision);
}

TEST_F(AutomationServerTest, AWindowChangeReachesClientsAsAnEventFromTheGui)
{
    DocumentController& doc = window_->controller();
    const auto made = doc.createPrimitive(QStringLiteral("box"));
    ASSERT_TRUE(made.ok());
    const std::uint64_t after = doc.session().events().lastSequence();
    ASSERT_TRUE(doc.renameObject(made.value(), QStringLiteral("Renamed in the window")).ok());
    v1::ListEventsRequest list;
    list.mutable_header()->set_session_id(session_);
    list.set_after_sequence(after);
    v1::ListEventsResponse events;
    events.ParseFromString(callAway(client_, "/resonant.v1.EventService/ListEvents", list).payload());
    bool found = false;
    for (const auto& e : events.events())
        if (e.type() == "scene.object.updated" && e.object().object_id() == made.value().str()) {
            found = true;
            EXPECT_EQ(e.source().source_interface(), v1::SOURCE_INTERFACE_GUI);
        }
    EXPECT_TRUE(found);
    // The GUI's own operations are in the same audit trail as the clients'.
    services::AuditQuery q;
    q.operation = "SceneService.UpdateObject";
    const auto audit = doc.session().audit(q);
    ASSERT_FALSE(audit.empty());
    EXPECT_EQ(audit.back().source, services::Interface::Gui);
}

TEST_F(AutomationServerTest, TheDialogListsSessionsAndStopsWithAStreamOpen)
{
    AutomationServerDialog* dialog = window_->showAutomationServerDialog();
    dialog->refresh();
    bool listed = false;
    for (int row = 0; row < dialog->sessionRows(); ++row)
        if (dialog->sessionCell(row, 0) == QStringLiteral("conformance client")) {
            listed = true;
            EXPECT_EQ(dialog->sessionCell(row, 1), QStringLiteral("gRPC"));
            EXPECT_EQ(dialog->sessionCell(row, 2), QStringLiteral("read, modify, execute"));
        }
    EXPECT_TRUE(listed);
    EXPECT_TRUE(dialog->stateText().startsWith(QStringLiteral("Listening")));
    // A subscription is open on another thread when the user stops the server.
    v1::SubscribeEventsRequest subscribe;
    subscribe.mutable_header()->set_session_id(session_);
    subscribe.set_include_snapshot(true);
    std::atomic<int> items{0};
    std::atomic<bool> ended{false};
    std::thread watcher([&] {
        const int fd = client_.open("/resonant.v1.EventService/SubscribeEvents", subscribe.SerializeAsString());
        transport::v1::LocalResponse r;
        while (fd >= 0 && Client::next(fd, r)) {
            if (r.end_of_stream())
                break;
            ++items;
        }
        if (fd >= 0)
            ::close(fd);
        ended = true;
    });
    ASSERT_TRUE(waitUntil([&] { return items.load() >= 1; }));  // the snapshot
    ASSERT_TRUE(window_->controller().createPrimitive(QStringLiteral("cone")).ok());
    ASSERT_TRUE(waitUntil([&] { return items.load() >= 2; }));  // and the change
    EXPECT_EQ(dialog->toggleServer(), QString());                // stop
    EXPECT_FALSE(window_->automationServer().running());
    ASSERT_TRUE(waitUntil([&] { return ended.load(); }, 5000));
    watcher.join();
    EXPECT_FALSE(QFile::exists(QString::fromStdString(client_.path)));
    EXPECT_EQ(dialog->stateText(), QStringLiteral("Stopped"));
}

#endif  // RESONANT_HAVE_AUTOMATION_API
