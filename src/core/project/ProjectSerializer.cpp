#include "project/ProjectSerializer.h"

#include "foundation/Log.h"
#include "foundation/Time.h"
#include "foundation/Units.h"
#include "foundation/Version.h"

#include <fstream>
#include <set>
#include <sstream>

namespace resonant {

using nlohmann::json;

namespace {

// Thrown inside this file to abort a load with a message; caught at the
// public boundary and turned into a failed Result.
struct FormatError {
    std::string message;
};

[[noreturn]] void fail(const std::string& message)
{
    throw FormatError{message};
}

const json& member(const json& object, const char* key, std::string_view where)
{
    if (!object.is_object())
        fail(std::string(where) + " must be an object");
    const auto it = object.find(key);
    if (it == object.end())
        fail(std::string(where) + " is missing \"" + key + "\"");
    return *it;
}

std::string stringMember(const json& object, const char* key, const std::string& where)
{
    const json& value = member(object, key, where);
    if (!value.is_string())
        fail(where + ": \"" + key + "\" must be a string");
    return value.get<std::string>();
}

std::string optionalString(const json& object, const char* key, const std::string& where)
{
    const auto it = object.find(key);
    if (it == object.end() || it->is_null())
        return {};
    if (!it->is_string())
        fail(where + ": \"" + key + "\" must be a string");
    return it->get<std::string>();
}

double numberValue(const json& value, const std::string& where)
{
    if (!value.is_number())
        fail(where + " must be a number");
    return value.get<double>();
}

json optionalNumber(const std::optional<double>& value)
{
    return value ? json(*value) : json(nullptr);
}

std::optional<double> readOptionalNumber(const json& object, const char* key, const std::string& where)
{
    const auto it = object.find(key);
    if (it == object.end() || it->is_null())
        return std::nullopt;
    return numberValue(*it, where + "." + key);
}

json propertyToJson(const PropertyValue& property)
{
    json result = {{"value", property.value},
                   {"provenance", std::string(toString(property.provenance))},
                   {"source", property.source}};
    const ValidityRange& range = property.validity;
    if (range.minFrequencyHz || range.maxFrequencyHz || range.minTemperatureK || range.maxTemperatureK) {
        result["validity"] = {{"minFrequencyHz", optionalNumber(range.minFrequencyHz)},
                              {"maxFrequencyHz", optionalNumber(range.maxFrequencyHz)},
                              {"minTemperatureK", optionalNumber(range.minTemperatureK)},
                              {"maxTemperatureK", optionalNumber(range.maxTemperatureK)}};
    }
    if (!property.frequencyTable.empty()) {
        json table = json::array();
        for (const FrequencySample& sample : property.frequencyTable)
            table.push_back({sample.frequencyHz, sample.value});
        result["frequencyTable"] = std::move(table);
    }
    if (!property.derivation.empty())
        result["derivation"] = property.derivation;
    if (property.uncertainty)
        result["uncertainty"] = *property.uncertainty;
    return result;
}

PropertyValue propertyFromJson(const json& value, const std::string& where)
{
    PropertyValue property;
    property.value = numberValue(member(value, "value", where), where + ".value");
    const std::string provenance = stringMember(value, "provenance", where);
    const auto parsed = provenanceFromString(provenance);
    if (!parsed)
        fail(where + ": unknown provenance \"" + provenance + "\"");
    property.provenance = *parsed;
    property.source = optionalString(value, "source", where);
    if (const auto it = value.find("validity"); it != value.end() && !it->is_null()) {
        if (!it->is_object())
            fail(where + ".validity must be an object");
        property.validity.minFrequencyHz = readOptionalNumber(*it, "minFrequencyHz", where + ".validity");
        property.validity.maxFrequencyHz = readOptionalNumber(*it, "maxFrequencyHz", where + ".validity");
        property.validity.minTemperatureK = readOptionalNumber(*it, "minTemperatureK", where + ".validity");
        property.validity.maxTemperatureK = readOptionalNumber(*it, "maxTemperatureK", where + ".validity");
    }
    if (const auto it = value.find("frequencyTable"); it != value.end() && !it->is_null()) {
        if (!it->is_array())
            fail(where + ".frequencyTable must be an array");
        for (const json& row : *it) {
            if (!row.is_array() || row.size() != 2)
                fail(where + ".frequencyTable rows must be [frequencyHz, value]");
            property.frequencyTable.push_back({numberValue(row[0], where + ".frequencyTable"),
                                               numberValue(row[1], where + ".frequencyTable")});
        }
    }
    property.derivation = optionalString(value, "derivation", where);
    property.uncertainty = readOptionalNumber(value, "uncertainty", where);
    return property;
}

Material readMaterial(const json& value, const std::string& where)
{
    Material material(Id(stringMember(value, "id", where)), stringMember(value, "name", where));
    material.setDescription(optionalString(value, "description", where));
    if (const auto it = value.find("domains"); it != value.end()) {
        if (!it->is_object())
            fail(where + ".domains must be an object");
        for (const auto& [domainName, set] : it->items()) {
            const auto domain = physicsDomainFromString(domainName);
            if (!domain)
                fail(where + ": unknown physics domain \"" + domainName + "\"");
            if (!set.is_object())
                fail(where + ".domains." + domainName + " must be an object");
            material.addDomain(*domain);
            for (const auto& [key, property] : set.items()) {
                const std::string propertyWhere = where + ".domains." + domainName + "." + key;
                if (Status status = material.setProperty(*domain, key, propertyFromJson(property, propertyWhere));
                    !status)
                    fail(propertyWhere + ": " + status.message());
            }
        }
    }
    if (Status status = material.validate(); !status)
        fail(where + ": " + status.message());
    return material;
}

json geometryToJson(const Geometry& geometry)
{
    json parameters = json::object();
    for (const auto& [key, value] : geometry.parameters())
        parameters[key] = value;
    return {{"kind", geometry.kindId()}, {"parameters", std::move(parameters)}};
}

Geometry geometryFromJson(const json& value, const std::string& where)
{
    const std::string kind = stringMember(value, "kind", where);
    auto created = Geometry::create(kind);
    if (!created)
        fail(where + ": " + created.message());
    Geometry geometry = created.take();
    const json& parameters = member(value, "parameters", where);
    if (!parameters.is_object())
        fail(where + ".parameters must be an object");
    for (const auto& [key, parameter] : parameters.items()) {
        if (!geometry.kind().findParameter(key))
            fail(where + ": " + geometry.kind().displayName + " has no parameter \"" + key + "\"");
    }
    for (const ParameterDef& def : geometry.kind().parameters) {
        const auto it = parameters.find(def.key);
        if (it == parameters.end())
            continue;  // a parameter added by a later version keeps its default
        const double number = numberValue(*it, where + ".parameters." + def.key);
        if (Status status = checkParameter(def, number); !status)
            fail(where + ": " + status.message());
    }
    // Parameters of one kind can constrain each other, so the order of
    // assignment matters: keep applying the ones that are accepted until no
    // more progress is made.
    std::vector<std::pair<std::string, double>> pending;
    for (const auto& [key, parameter] : parameters.items())
        pending.emplace_back(key, parameter.get<double>());
    for (size_t pass = 0; pass <= pending.size() && !pending.empty(); ++pass) {
        std::vector<std::pair<std::string, double>> remaining;
        for (const auto& [key, number] : pending)
            if (!geometry.setParameter(key, number))
                remaining.emplace_back(key, number);
        if (remaining.size() == pending.size())
            break;
        pending = std::move(remaining);
    }
    if (!pending.empty()) {
        const Status status = geometry.setParameter(pending.front().first, pending.front().second);
        fail(where + ": " + status.message());
    }
    return geometry;
}

json transformToJson(const Transform& transform)
{
    return {{"position", toJson(transform.position)},
            {"rotation", toJson(transform.rotation)},
            {"scale", toJson(transform.scale)}};
}

Vec3 readVec3(const json& value, const std::string& where)
{
    auto result = vec3FromJson(value, where);
    if (!result)
        fail(result.message());
    return result.take();
}

json objectToJson(const SceneObject& object)
{
    json result = {{"id", object.id().str()},
                   {"type", object.type()},
                   {"name", object.state.name},
                   {"visible", object.state.visible},
                   {"transform", transformToJson(object.state.local)}};
    if (object.state.geometry)
        result["geometry"] = geometryToJson(*object.state.geometry);
    if (!object.state.materialId.isNull())
        result["material"] = object.state.materialId.str();
    if (object.state.component)
        result["component"] = {{"type", object.state.component->typeId()}, {"data", object.state.component->toJson()}};
    json children = json::array();
    for (const auto& child : object.children())
        children.push_back(objectToJson(*child));
    result["children"] = std::move(children);
    return result;
}

std::unique_ptr<SceneObject> objectFromJson(const json& value, const std::string& where, std::set<std::string>& ids,
                                            const MaterialLibrary& materials, std::vector<std::string>& warnings)
{
    const std::string id = stringMember(value, "id", where);
    if (id.empty())
        fail(where + ": object id is empty");
    if (!ids.insert(id).second)
        fail(where + ": duplicate object id " + id);
    const std::string here = where + " (\"" + optionalString(value, "name", where) + "\")";

    auto object = std::make_unique<SceneObject>(Id(id), stringMember(value, "type", here));
    object->state.name = stringMember(value, "name", here);
    const json& visible = member(value, "visible", here);
    if (!visible.is_boolean())
        fail(here + ": \"visible\" must be true or false");
    object->state.visible = visible.get<bool>();

    const json& transform = member(value, "transform", here);
    object->state.local.position = readVec3(member(transform, "position", here + ".transform"), here + ".transform.position");
    object->state.local.rotation = readVec3(member(transform, "rotation", here + ".transform"), here + ".transform.rotation");
    object->state.local.scale = readVec3(member(transform, "scale", here + ".transform"), here + ".transform.scale");

    if (const auto it = value.find("geometry"); it != value.end() && !it->is_null())
        object->state.geometry = geometryFromJson(*it, here + ".geometry");

    if (const std::string material = optionalString(value, "material", here); !material.empty()) {
        if (materials.find(Id(material))) {
            object->state.materialId = Id(material);
        } else {
            warnings.push_back("Object \"" + object->state.name + "\" referred to material " + material
                               + ", which is not in the project; the assignment was removed.");
        }
    }

    if (const auto it = value.find("component"); it != value.end() && !it->is_null()) {
        const std::string type = stringMember(*it, "type", here + ".component");
        auto component = ComponentRegistry::instance().create(type, member(*it, "data", here + ".component"));
        if (!component)
            fail(here + ".component: " + component.message());
        object->state.component = component.take();
    }

    if (const auto it = value.find("children"); it != value.end()) {
        if (!it->is_array())
            fail(here + ": \"children\" must be an array");
        size_t index = 0;
        for (const json& child : *it)
            object->addChild(objectFromJson(child, where + ".children[" + std::to_string(index++) + "]", ids, materials,
                                            warnings));
    }
    return object;
}

SimulationConfig simulationFromJson(const json& value, const std::string& where)
{
    SimulationConfig config;
    config.id = Id(stringMember(value, "id", where));
    config.name = stringMember(value, "name", where);
    const std::string domain = stringMember(value, "domain", where);
    const auto parsed = physicsDomainFromString(domain);
    if (!parsed)
        fail(where + ": unknown physics domain \"" + domain + "\"");
    config.domain = *parsed;
    config.solverId = stringMember(value, "solver", where);
    config.parameters = member(value, "parameters", where);
    if (!config.parameters.is_object())
        fail(where + ".parameters must be an object");
    return config;
}

LoadedProject readDocument(json document)
{
    LoadedProject loaded;
    if (!document.is_object())
        fail("the file does not contain a JSON object");
    const auto format = document.find("format");
    if (format == document.end() || !format->is_string() || format->get<std::string>() != kProjectFormatTag)
        fail("this is not a Resonant project file (missing format tag \"" + std::string(kProjectFormatTag) + "\")");

    if (Status status = migrateDocument(document, projectMigrationSteps(), kProjectSchemaVersion, loaded.warnings);
        !status)
        fail(status.message());

    auto project = std::make_unique<Project>();
    const json& info = member(document, "project", "document");
    project->id = Id(stringMember(info, "id", "project"));
    if (project->id.isNull())
        fail("project: id is empty");
    project->name = stringMember(info, "name", "project");
    project->createdUtc = optionalString(info, "createdUtc", "project");
    project->modifiedUtc = optionalString(info, "modifiedUtc", "project");

    const json& materials = member(document, "materials", "document");
    if (!materials.is_array())
        fail("\"materials\" must be an array");
    size_t index = 0;
    for (const json& value : materials) {
        const std::string where = "materials[" + std::to_string(index++) + "]";
        if (Status status = project->materials.add(readMaterial(value, where)); !status)
            fail(where + ": " + status.message());
    }

    const json& objects = member(member(document, "scene", "document"), "objects", "scene");
    if (!objects.is_array())
        fail("scene: \"objects\" must be an array");
    std::set<std::string> ids;
    index = 0;
    for (const json& value : objects) {
        const std::string where = "scene.objects[" + std::to_string(index++) + "]";
        auto inserted = project->scene.insert(objectFromJson(value, where, ids, project->materials, loaded.warnings));
        if (!inserted)
            fail(where + ": " + inserted.message());
    }

    if (const auto it = document.find("simulations"); it != document.end()) {
        if (!it->is_array())
            fail("\"simulations\" must be an array");
        index = 0;
        for (const json& value : *it)
            project->simulations.push_back(simulationFromJson(value, "simulations[" + std::to_string(index++) + "]"));
    }
    if (const auto it = document.find("extensions"); it != document.end()) {
        if (!it->is_object())
            fail("\"extensions\" must be an object");
        project->extensions = *it;
    }

    loaded.project = std::move(project);
    return loaded;
}

}  // namespace

json toJson(const Vec3& vector)
{
    return json::array({vector.x(), vector.y(), vector.z()});
}

Result<Vec3> vec3FromJson(const json& value, const std::string& what)
{
    if (!value.is_array() || value.size() != 3 || !value[0].is_number() || !value[1].is_number()
        || !value[2].is_number())
        return Result<Vec3>::failure(what + " must be an array of three numbers");
    const Vec3 vector(value[0].get<double>(), value[1].get<double>(), value[2].get<double>());
    if (!vector.allFinite())
        return Result<Vec3>::failure(what + " must contain finite numbers");
    return vector;
}

json toJson(const Material& material)
{
    json domains = json::object();
    for (const PhysicsDomain domain : material.domains()) {
        json set = json::object();
        for (const auto& [key, property] : *material.properties(domain))
            set[key] = propertyToJson(property);
        domains[std::string(toString(domain))] = std::move(set);
    }
    return {{"id", material.id().str()},
            {"name", material.name()},
            {"description", material.description()},
            {"domains", std::move(domains)}};
}

Result<Material> materialFromJson(const json& value)
{
    try {
        return readMaterial(value, "material");
    } catch (const FormatError& error) {
        return Result<Material>::failure(error.message);
    }
}

json serializeProject(const Project& project)
{
    json materials = json::array();
    for (const Material& material : project.materials.materials())
        materials.push_back(toJson(material));

    json objects = json::array();
    for (const auto& child : project.scene.root().children())
        objects.push_back(objectToJson(*child));

    json simulations = json::array();
    for (const SimulationConfig& config : project.simulations)
        simulations.push_back(toJson(config));

    using units::Conventions;
    return {
        {"format", kProjectFormatTag},
        {"schemaVersion", kProjectSchemaVersion},
        {"application", {{"name", kApplicationName}, {"version", kApplicationVersion}}},
        {"project",
         {{"id", project.id.str()},
          {"name", project.name},
          {"createdUtc", project.createdUtc},
          {"modifiedUtc", project.modifiedUtc}}},
        {"units",
         {{"length", Conventions::length},
          {"time", Conventions::time},
          {"frequency", Conventions::frequency},
          {"mass", Conventions::mass},
          {"angle", Conventions::angle},
          {"rotation", Conventions::rotation},
          {"axes", Conventions::axes}}},
        {"materials", std::move(materials)},
        {"scene", {{"objects", std::move(objects)}}},
        {"simulations", std::move(simulations)},
        {"extensions", project.extensions},
    };
}

Result<LoadedProject> deserializeProject(json document)
{
    try {
        return readDocument(std::move(document));
    } catch (const FormatError& error) {
        return Result<LoadedProject>::failure(error.message);
    } catch (const json::exception& error) {
        return Result<LoadedProject>::failure(std::string("malformed project data: ") + error.what());
    }
}

Status migrateDocument(json& document, const MigrationSteps& steps, int targetVersion, std::vector<std::string>& notes)
{
    if (!document.is_object())
        return Status::failure("the file does not contain a JSON object");
    const auto it = document.find("schemaVersion");
    if (it == document.end() || !it->is_number_integer())
        return Status::failure("the project file has no valid \"schemaVersion\"");
    int version = it->get<int>();
    if (version < 1)
        return Status::failure("the project file has an invalid schema version (" + std::to_string(version) + ")");
    if (version > targetVersion) {
        return Status::failure("the project was saved by a newer version of Resonant (file format "
                               + std::to_string(version) + ", this version reads up to "
                               + std::to_string(targetVersion) + "). Update Resonant to open it.");
    }
    while (version < targetVersion) {
        const auto step = steps.find(version);
        if (step == steps.end())
            return Status::failure("no migration is available from file format " + std::to_string(version) + " to "
                                   + std::to_string(version + 1));
        if (Status status = step->second(document); !status)
            return Status::failure("migrating from file format " + std::to_string(version) + " failed: "
                                   + status.message());
        ++version;
        document["schemaVersion"] = version;
        notes.push_back("The project was converted from file format " + std::to_string(version - 1) + " to "
                        + std::to_string(version) + ".");
    }
    return Status::success();
}

const MigrationSteps& projectMigrationSteps()
{
    static const MigrationSteps steps;
    return steps;
}

Status saveProjectFile(Project& project, const std::filesystem::path& path)
{
    const std::string previousModified = project.modifiedUtc;
    project.modifiedUtc = utcNowIso8601();
    const std::string text = serializeProject(project).dump(2) + "\n";

    std::filesystem::path temporary = path;
    temporary += ".tmp";
    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        out << text;
        out.flush();
        if (!out) {
            project.modifiedUtc = previousModified;
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            logError("project", "could not write " + temporary.string());
            return Status::failure("could not write to " + path.string()
                                   + ". Check that the folder exists and is writable.");
        }
    }
    std::error_code error;
    std::filesystem::rename(temporary, path, error);
    if (error) {
        project.modifiedUtc = previousModified;
        std::filesystem::remove(temporary, error);
        logError("project", "could not replace " + path.string());
        return Status::failure("could not replace " + path.string() + ": the file may be in use or read-only.");
    }
    logInfo("project", "saved " + path.string());
    return Status::success();
}

Result<LoadedProject> loadProjectFile(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        logError("project", "could not open " + path.string());
        return Result<LoadedProject>::failure("could not open " + path.string()
                                              + ". The file may have been moved or deleted.");
    }
    std::stringstream buffer;
    buffer << in.rdbuf();
    json document = json::parse(buffer.str(), nullptr, false);
    if (document.is_discarded()) {
        logError("project", "not valid JSON: " + path.string());
        return Result<LoadedProject>::failure(path.filename().string()
                                              + " is not a valid project file (it is not well-formed JSON).");
    }
    auto loaded = deserializeProject(std::move(document));
    if (!loaded) {
        logError("project", "could not load " + path.string() + ": " + loaded.message());
        return Result<LoadedProject>::failure(path.filename().string() + " could not be loaded: " + loaded.message());
    }
    for (const std::string& warning : loaded.value().warnings)
        logWarning("project", warning);
    logInfo("project", "loaded " + path.string());
    return loaded;
}

}  // namespace resonant
