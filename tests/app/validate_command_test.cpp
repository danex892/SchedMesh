#include "schedmesh/app/validate_command.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <sstream>

namespace schedmesh::app {
namespace {

TEST(ValidateCommandTest, ValidatesPublicTinyFixture) {
  const auto fixture = std::filesystem::path{"tests"} / "fixtures" / "tiny_project.json";
  std::ostringstream output;
  std::ostringstream errors;

  const int exit_code = validate_project_file(fixture.string(), output, errors);

  EXPECT_EQ(exit_code, kExitSuccess);
  EXPECT_EQ(output.str(), "Valid project project-tiny\n");
  EXPECT_TRUE(errors.str().empty());
}

TEST(ValidateCommandTest, ReportsMissingFileAsIoError) {
  std::ostringstream output;
  std::ostringstream errors;

  const int exit_code = validate_project_file("definitely-missing-project.json", output, errors);

  EXPECT_EQ(exit_code, kExitUsageOrIoError);
  EXPECT_TRUE(output.str().empty());
  EXPECT_NE(errors.str().find("io.open_failed"), std::string::npos);
}

}  // namespace
}  // namespace schedmesh::app
