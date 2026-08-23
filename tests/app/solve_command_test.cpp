#include "schedmesh/app/solve_command.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>

#include "schedmesh/app/validate_command.h"

namespace schedmesh::app {
namespace {

TEST(SolveCommandTest, SolvesCanonicalProjectAndWritesDeterministicSchedule) {
  const std::filesystem::path output_path =
      std::filesystem::temp_directory_path() / "schedmesh-tiny-schedule.json";
  std::filesystem::remove(output_path);
  std::ostringstream output;
  std::ostringstream errors;

  const int exit_code = solve_project_file("tests/fixtures/tiny_project.json", output_path.string(),
                                           {}, output, errors);

  EXPECT_EQ(exit_code, kExitSuccess);
  EXPECT_TRUE(errors.str().empty());
  EXPECT_NE(output.str().find("status=optimal"), std::string::npos);
  std::ifstream input(output_path, std::ios::binary);
  const std::string contents{std::istreambuf_iterator<char>{input},
                             std::istreambuf_iterator<char>{}};
  input.close();
  EXPECT_EQ(contents,
            "{\n"
            "  \"meetings\": [\n"
            "    {\n"
            "      \"meeting\": \"meeting-001\",\n"
            "      \"rooms\": [\n"
            "        \"room-001\"\n"
            "      ],\n"
            "      \"start_slot\": \"slot-mon-p1\",\n"
            "      \"teachers\": [\n"
            "        \"teacher-001\"\n"
            "      ]\n"
            "    }\n"
            "  ]\n"
            "}\n");
  std::filesystem::remove(output_path);
}

TEST(SolveCommandTest, DoesNotCreateScheduleForMissingProject) {
  const std::filesystem::path output_path =
      std::filesystem::temp_directory_path() / "schedmesh-missing-schedule.json";
  std::filesystem::remove(output_path);
  std::ostringstream output;
  std::ostringstream errors;

  const int exit_code = solve_project_file("definitely-missing-project.json", output_path.string(),
                                           {}, output, errors);

  EXPECT_EQ(exit_code, kExitUsageOrIoError);
  EXPECT_FALSE(std::filesystem::exists(output_path));
  EXPECT_NE(errors.str().find("io.open_failed"), std::string::npos);
}

}  // namespace
}  // namespace schedmesh::app
