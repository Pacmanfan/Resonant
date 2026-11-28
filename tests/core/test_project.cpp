#include "project/ProjectSerializer.h"

#include "foundation/Units.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

using namespace resonant;
using nlohmann::json;

namespace {

std::filesystem::path scratchFile(const std::string& name)
{
    const auto dir = std::filesystem::temp_directory_path() / "resonant-core-tests";
    std::filesystem::create_directories(dir);
    return dir / name;
}

// A project with a nested hierarchy, every transform component set, edited
// geometry, a material assignment and a hidden object.
std::unique_ptr<Project> sampleProject()
{
    auto project = Project::createNew("Sample");
    auto group = makeGroup("Assembly");
    group->state.local.position = Vec3(1.0, -2.0, 0.5);
    group->state.local.rotation = Vec3(0.1, 0.2, units::pi / 3.0);
    const Id groupId = project->scene.insert(std::move(group)).value()->id();

    Geometry cylinder = Geometry::create("cylinder").take();
    EXPECT_TRUE(cylinder.setParameter("radius", 0.125).ok());
    EXPECT_TRUE(cylinder.setParameter("height", 3.0).ok());
    auto rod = makeGeometryObject("Rod", cylinder);
    rod->state.local.scale = Vec3(1.0, 2.0, 0.5);
    rod->state.materialId = project->materials.findByName("Steel")->id();
    rod->state.visible = false;
    EXPECT_TRUE(project->scene.insert(std::move(rod), groupId).ok());

    EXPECT_TRUE(project->scene.insert(makeGeometryObject("Ball", Geometry::create("sphere").take())).ok());

    SimulationConfig config;
    config.id = Id::generate();
    config.name = "Placeholder configuration";
    config.domain = PhysicsDomain::Acoustic;
    config.solverId = "none";
    config.parameters = {{"frequencyHz", 40000.0}};
    project->simulations.push_back(config);
    project->extensions["futureFeature"] = {{"keep", true}};
    return project;
}

void expectSameScene(const SceneObject& a, const SceneObject& b)
{
    ASSERT_EQ(a.children().size(), b.children().size());
    for (size_t i = 0; i < a.children().size(); ++i) {
        const SceneObject& x = *a.children()[i];
        const SceneObject& y = *b.children()[i];
        EXPECT_EQ(x.id(), y.id());
        EXPECT_EQ(x.type(), y.type());
        EXPECT_EQ(x.state, y.state) << x.state.name;
        expectSameScene(x, y);
    }
}

}  // namespace

TEST(ProjectFormat, DocumentStatesFormatVersionAndUnits)
{
    const json document = serializeProject(*sampleProject());
    EXPECT_EQ(document["format"], "resonant-project");
    EXPECT_EQ(document["schemaVersion"], kProjectSchemaVersion);
    EXPECT_EQ(document["application"]["name"], "Resonant");
    EXPECT_EQ(document["units"]["length"], "m");
    EXPECT_EQ(document["units"]["angle"], "rad");
    EXPECT_EQ(document["units"]["frequency"], "Hz");
    EXPECT_TRUE(document["units"].contains("axes"));
    EXPECT_TRUE(document["units"].contains("rotation"));
}

TEST(ProjectFormat, RoundTripPreservesEverything)
{
    const auto original = sampleProject();
    auto loaded = deserializeProject(serializeProject(*original));
    ASSERT_TRUE(loaded.ok()) << loaded.message();
    EXPECT_TRUE(loaded.value().warnings.empty());
    const Project& copy = *loaded.value().project;

    EXPECT_EQ(copy.id, original->id);
    EXPECT_EQ(copy.name, original->name);
    EXPECT_EQ(copy.createdUtc, original->createdUtc);
    EXPECT_EQ(copy.materials, original->materials);
    EXPECT_EQ(copy.simulations, original->simulations);
    EXPECT_EQ(copy.extensions, original->extensions);
    EXPECT_EQ(copy.scene.objectCount(), original->scene.objectCount());
    expectSameScene(original->scene.root(), copy.scene.root());

    // Serializing the copy gives the identical document: nothing is lost
    // or reordered by a load.
    EXPECT_EQ(serializeProject(copy), serializeProject(*original));
}

TEST(ProjectFormat, SaveAndLoadFile)
{
    auto original = sampleProject();
    const auto path = scratchFile("roundtrip.resonant");
    ASSERT_TRUE(saveProjectFile(*original, path).ok());
    EXPECT_FALSE(std::filesystem::exists(path.string() + ".tmp"));

    auto loaded = loadProjectFile(path);
    ASSERT_TRUE(loaded.ok()) << loaded.message();
    EXPECT_EQ(serializeProject(*loaded.value().project), serializeProject(*original));
}

TEST(ProjectFormat, MissingFileIsReportedNotThrown)
{
    const auto loaded = loadProjectFile(scratchFile("does-not-exist.resonant"));
    EXPECT_FALSE(loaded.ok());
    EXPECT_NE(loaded.message().find("could not open"), std::string::npos);
}

TEST(ProjectFormat, UnwritableLocationIsReportedAndLeavesNoFile)
{
    auto project = sampleProject();
    const std::string before = project->modifiedUtc;
    const auto path = scratchFile("no-such-directory") / "x" / "project.resonant";
    const Status status = saveProjectFile(*project, path);
    EXPECT_FALSE(status.ok());
    EXPECT_FALSE(std::filesystem::exists(path));
    EXPECT_EQ(project->modifiedUtc, before);
}

TEST(ProjectFormat, MalformedJsonFileIsRejected)
{
    const auto path = scratchFile("broken.resonant");
    std::ofstream(path) << "{ \"format\": \"resonant-project\", ";
    const auto loaded = loadProjectFile(path);
    EXPECT_FALSE(loaded.ok());
    EXPECT_NE(loaded.message().find("not well-formed JSON"), std::string::npos);
}

TEST(ProjectFormat, InvalidDocumentsAreRejectedWithTheReason)
{
    const json good = serializeProject(*sampleProject());
    const auto rejects = [&](const std::function<void(json&)>& damage, const std::string& expected) {
        json document = good;
        damage(document);
        const auto loaded = deserializeProject(document);
        EXPECT_FALSE(loaded.ok()) << "expected rejection mentioning: " << expected;
        EXPECT_NE(loaded.message().find(expected), std::string::npos) << loaded.message();
    };

    rejects([](json& d) { d = json::array(); }, "JSON object");
    rejects([](json& d) { d.erase("format"); }, "not a Resonant project");
    rejects([](json& d) { d["format"] = "something-else"; }, "not a Resonant project");
    rejects([](json& d) { d.erase("schemaVersion"); }, "schemaVersion");
    rejects([](json& d) { d["schemaVersion"] = "one"; }, "schemaVersion");
    rejects([](json& d) { d["schemaVersion"] = 0; }, "invalid schema version");
    rejects([](json& d) { d.erase("scene"); }, "scene");
    rejects([](json& d) { d.erase("materials"); }, "materials");
    rejects([](json& d) { d["project"]["id"] = ""; }, "id is empty");
    rejects([](json& d) { d["scene"]["objects"][0]["transform"]["position"] = {1, 2}; }, "three numbers");
    rejects([](json& d) { d["scene"]["objects"][0]["transform"]["scale"] = {1, "a", 3}; }, "three numbers");
    rejects([](json& d) { d["scene"]["objects"][0].erase("id"); }, "\"id\"");
    rejects([](json& d) { d["scene"]["objects"][1]["id"] = d["scene"]["objects"][0]["id"]; }, "duplicate object id");
    rejects([](json& d) { d["scene"]["objects"][1]["geometry"]["kind"] = "teapot"; }, "teapot");
    rejects([](json& d) { d["scene"]["objects"][1]["geometry"]["parameters"]["radius"] = -1.0; }, "Radius");
    rejects([](json& d) { d["scene"]["objects"][1]["geometry"]["parameters"]["bogus"] = 1.0; }, "bogus");
    rejects([](json& d) { d["scene"]["objects"][1]["visible"] = "yes"; }, "visible");
    rejects([](json& d) { d["scene"]["objects"][1]["component"] = {{"type", "no.such"}, {"data", json::object()}}; },
            "unknown component type");
    rejects([](json& d) { d["materials"][0]["domains"]["acoustic"]["density"]["value"] = -3.0; }, "Density");
    rejects([](json& d) { d["materials"][0]["domains"]["acoustic"]["density"]["provenance"] = "guess"; },
            "unknown provenance");
    rejects([](json& d) { d["materials"][0]["domains"]["optical"] = json::object(); }, "unknown physics domain");  // thermal is a domain since REQ-CHAT-20261126-016
    rejects([](json& d) { d["materials"][1]["id"] = d["materials"][0]["id"]; }, "already exists");
}

TEST(ProjectFormat, MissingMaterialReferenceIsRepairedWithAWarning)
{
    json document = serializeProject(*sampleProject());
    document["scene"]["objects"][0]["children"][0]["material"] = "mat-unobtainium";
    auto loaded = deserializeProject(document);
    ASSERT_TRUE(loaded.ok()) << loaded.message();
    ASSERT_EQ(loaded.value().warnings.size(), 1u);
    EXPECT_NE(loaded.value().warnings[0].find("mat-unobtainium"), std::string::npos);
    const SceneObject& rod = *loaded.value().project->scene.root().children()[0]->children()[0];
    EXPECT_TRUE(rod.state.materialId.isNull());
}

TEST(ProjectFormat, NewerFileVersionIsRefusedWithAnExplanation)
{
    json document = serializeProject(*sampleProject());
    document["schemaVersion"] = kProjectSchemaVersion + 1;
    const auto loaded = deserializeProject(document);
    EXPECT_FALSE(loaded.ok());
    EXPECT_NE(loaded.message().find("newer version of Resonant"), std::string::npos);
}

TEST(ProjectFormat, UnknownTopLevelDataInExtensionsSurvivesARoundTrip)
{
    json document = serializeProject(*sampleProject());
    document["extensions"]["measurements"] = {{"sessions", json::array({1, 2, 3})}};
    auto loaded = deserializeProject(document);
    ASSERT_TRUE(loaded.ok());
    EXPECT_EQ(serializeProject(*loaded.value().project)["extensions"]["measurements"]["sessions"], json::array({1, 2, 3}));
}

TEST(Migration, StepsRunInOrderAndUpdateTheVersion)
{
    json document = {{"schemaVersion", 1}, {"title", "old"}};
    MigrationSteps steps;
    steps[1] = [](json& d) {
        d["name"] = d["title"];
        d.erase("title");
        return Status::success();
    };
    steps[2] = [](json& d) {
        d["name"] = d["name"].get<std::string>() + "!";
        return Status::success();
    };
    std::vector<std::string> notes;
    ASSERT_TRUE(migrateDocument(document, steps, 3, notes).ok());
    EXPECT_EQ(document["schemaVersion"], 3);
    EXPECT_EQ(document["name"], "old!");
    EXPECT_FALSE(document.contains("title"));
    EXPECT_EQ(notes.size(), 2u);
}

TEST(Migration, CurrentVersionNeedsNoSteps)
{
    json document = {{"schemaVersion", 3}};
    std::vector<std::string> notes;
    EXPECT_TRUE(migrateDocument(document, {}, 3, notes).ok());
    EXPECT_TRUE(notes.empty());
}

TEST(Migration, FailsForMissingStepFailedStepOrNewerVersion)
{
    std::vector<std::string> notes;
    json missing = {{"schemaVersion", 1}};
    EXPECT_FALSE(migrateDocument(missing, {}, 2, notes).ok());
    EXPECT_EQ(missing["schemaVersion"], 1);

    json failing = {{"schemaVersion", 1}};
    MigrationSteps steps;
    steps[1] = [](json&) { return Status::failure("cannot convert"); };
    const Status status = migrateDocument(failing, steps, 2, notes);
    EXPECT_FALSE(status.ok());
    EXPECT_NE(status.message().find("cannot convert"), std::string::npos);

    json newer = {{"schemaVersion", 9}};
    EXPECT_FALSE(migrateDocument(newer, steps, 2, notes).ok());
}

TEST(Project, NewProjectHasExampleMaterialsAndAnEmptyScene)
{
    const auto project = Project::createNew("Fresh");
    EXPECT_EQ(project->name, "Fresh");
    EXPECT_FALSE(project->id.isNull());
    EXPECT_EQ(project->scene.objectCount(), 0u);
    EXPECT_EQ(project->materials.materials().size(), 18u);
    EXPECT_NE(Project::createNew()->id, project->id);
}
