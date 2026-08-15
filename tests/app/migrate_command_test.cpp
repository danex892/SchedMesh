#include "schedmesh/app/migrate_command.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>

#include "schedmesh/app/validate_command.h"
#include "schedmesh/io/project_json.h"

namespace schedmesh::app {
namespace {

TEST(MigrateCommandTest, MigratesHistoricalFixtureToCanonicalJson) {
  const std::filesystem::path output_path =
      std::filesystem::temp_directory_path() / "schedmesh-migrated-project.json";
  std::filesystem::remove(output_path);
  std::ostringstream output;
  std::ostringstream errors;

  const int exit_code =
      migrate_legacy_project("data/settings.conf", output_path.string(), output, errors);

  EXPECT_EQ(exit_code, kExitSuccess) << errors.str();
  std::ifstream input(output_path, std::ios::binary);
  const std::string contents{std::istreambuf_iterator<char>{input},
                             std::istreambuf_iterator<char>{}};
  input.close();
  const io::ProjectReadResult project = io::read_project_json(contents);
  ASSERT_TRUE(project.ok());
  EXPECT_FALSE(project.project->student_groups.empty());
  EXPECT_FALSE(project.project->teachers.empty());
  EXPECT_FALSE(project.project->meetings.empty());
  EXPECT_FALSE(project.project->rooms.empty());
  EXPECT_NE(output.str().find("Migrated legacy project"), std::string::npos);
  std::filesystem::remove(output_path);
}

}  // namespace
}  // namespace schedmesh::app
