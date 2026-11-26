#include "project/Project.h"

#include "foundation/Time.h"

namespace resonant {

Project::Project() : id(Id::generate()), createdUtc(utcNowIso8601()), modifiedUtc(createdUtc) {}

std::unique_ptr<Project> Project::createNew(std::string name)
{
    auto project = std::make_unique<Project>();
    project->name = std::move(name);
    project->materials = createExampleMaterialLibrary();
    return project;
}

}  // namespace resonant
