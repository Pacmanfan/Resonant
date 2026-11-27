// The Python API's own tests (python/tests, REQ-CHAT-20261126-010), run from
// the core suite against the native module of the same build tree
// (<tree>/src/python/_resonant*.so), and the automation API's client tests
// against this tree's headless host. Skipped, with the reason, when this build
// has no Python module (no pybind11 or Python headers).
#include <gtest/gtest.h>

#include <array>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>

namespace {

std::filesystem::path moduleDirectory()
{
    std::error_code error;
    const auto exe = std::filesystem::read_symlink("/proc/self/exe", error);
    if (error)
        return {};
    return std::filesystem::weakly_canonical(exe.parent_path() / ".." / ".." / "src" / "python");
}

bool hasModule(const std::filesystem::path& dir)
{
    std::error_code error;
    if (!std::filesystem::is_directory(dir, error))
        return false;
    for (const auto& entry : std::filesystem::directory_iterator(dir, error))
        if (entry.path().filename().string().rfind("_resonant", 0) == 0 && entry.path().extension() == ".so")
            return true;
    return false;
}

}  // namespace

TEST(PythonApi, ThePackageTestsPassAgainstThisBuildsModule)
{
    const std::filesystem::path dir = moduleDirectory();
    if (!hasModule(dir))
        GTEST_SKIP() << "this build has no Python module (" << dir << "): built only with pybind11 and the Python headers";
    const std::string source = RESONANT_SOURCE_DIR;
    const std::string help = std::filesystem::weakly_canonical(dir / ".." / ".." / "help").string();
    // The automation API's tests use this tree's headless host and descriptor set (skipped without them).
    const std::filesystem::path tree = std::filesystem::weakly_canonical(dir / ".." / "..");
    const std::string automation = "RESONANT_AUTOMATION_HOST='" + (tree / "src" / "automation_host" / "resonant-automation").string() +
                                   "' RESONANT_API_DESCRIPTORS='" + (tree / "src" / "automation_api" / "resonant_api.desc").string() + "' ";
    const std::string command = "cd '" + source + "' && " + automation + "RESONANT_HELP_DIR='" + help + "' RESONANT_PYTHON_MODULE_DIR='" + dir.string() +
                                "' PYTHONPATH='" + source + "/python' python3 -W ignore::ResourceWarning -m unittest discover -s python/tests 2>&1";
    std::unique_ptr<FILE, int (*)(FILE*)> pipe(popen(command.c_str(), "r"), pclose);
    ASSERT_TRUE(pipe);
    std::string output;
    std::array<char, 4096> buffer{};
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()))
        output += buffer.data();
    const int status = pclose(pipe.release());
    EXPECT_EQ(status, 0) << output;
    EXPECT_NE(output.find("OK"), std::string::npos) << output;
}
