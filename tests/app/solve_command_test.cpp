#include "schedmesh/app/solve_command.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <stop_token>
#include <string>
#include <thread>

#include "schedmesh/app/migrate_command.h"
#include "schedmesh/app/validate_command.h"
#include "schedmesh/io/project_json.h"

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

TEST(SolveCommandTest, ProvesHistoricalAcceptanceBaselineInfeasibleWithinBudget) {
  const std::filesystem::path project_path =
      std::filesystem::temp_directory_path() / "schedmesh-historical-project.json";
  const std::filesystem::path schedule_path =
      std::filesystem::temp_directory_path() / "schedmesh-historical-schedule.json";
  std::filesystem::remove(project_path);
  std::filesystem::remove(schedule_path);
  std::ostringstream migration_output;
  std::ostringstream migration_errors;
  ASSERT_EQ(migrate_legacy_project("data/settings.conf", project_path.string(), migration_output,
                                   migration_errors),
            kExitSuccess);
  std::ostringstream solve_output;
  std::ostringstream solve_errors;
  constexpr auto kHistoricalBudget = std::chrono::seconds{30};

  const int exit_code =
      solve_project_file(project_path.string(), schedule_path.string(),
                         {.time_limit = kHistoricalBudget, .worker_count = 1, .random_seed = 1},
                         solve_output, solve_errors);

  EXPECT_EQ(exit_code, kExitValidationError);
  EXPECT_NE(solve_output.str().find("status=infeasible"), std::string::npos);
  EXPECT_TRUE(solve_errors.str().empty());
  EXPECT_FALSE(std::filesystem::exists(schedule_path));
  std::filesystem::remove(project_path);
}

TEST(SolveCommandTest, CancelsHistoricalSolveDuringActiveSearch) {
  const std::filesystem::path project_path =
      std::filesystem::temp_directory_path() / "schedmesh-cancellation-project.json";
  std::filesystem::remove(project_path);
  std::ostringstream migration_output;
  std::ostringstream migration_errors;
  ASSERT_EQ(migrate_legacy_project("data/settings.conf", project_path.string(), migration_output,
                                   migration_errors),
            kExitSuccess);
  std::ifstream input(project_path, std::ios::binary);
  const std::string contents{std::istreambuf_iterator<char>{input},
                             std::istreambuf_iterator<char>{}};
  input.close();
  const io::ProjectReadResult project = io::read_project_json(contents);
  ASSERT_TRUE(project.ok());
  std::stop_source cancellation;
  constexpr auto kCancellationDelay = std::chrono::seconds{1};
  constexpr auto kShutdownMargin = std::chrono::seconds{3};
  std::jthread cancel_search([&cancellation, kCancellationDelay] {
    std::this_thread::sleep_for(kCancellationDelay);
    cancellation.request_stop();
  });

  const solver::SolveResult result = solver::solve(
      {.project = *project.project,
       .parameters = {.time_limit = std::chrono::seconds{30}, .worker_count = 1, .random_seed = 1},
       .cancellation = cancellation.get_token()});

  EXPECT_EQ(result.status, solver::SolveStatus::kCancelled);
  EXPECT_FALSE(result.schedule.has_value());
  EXPECT_LE(result.statistics.elapsed, kCancellationDelay + kShutdownMargin);
  std::filesystem::remove(project_path);
}

}  // namespace
}  // namespace schedmesh::app
