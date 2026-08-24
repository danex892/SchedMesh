#include "schedmesh/app/migrate_command.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <numeric>
#include <sstream>
#include <string>

#include "schedmesh/app/validate_command.h"
#include "schedmesh/io/project_json.h"
#include "schedmesh/validation/room_audit.h"

namespace schedmesh::app {
namespace {

TEST(MigrateCommandTest, MigratesHistoricalFixtureToCanonicalJson) {
  const std::filesystem::path output_path =
      std::filesystem::temp_directory_path() / "schedmesh-migrated-project.json";
  const std::filesystem::path second_output_path =
      std::filesystem::temp_directory_path() / "schedmesh-migrated-project-second.json";
  std::filesystem::remove(output_path);
  std::filesystem::remove(second_output_path);
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
  EXPECT_EQ(project.project->student_groups.size(), 29U);
  EXPECT_EQ(project.project->teachers.size(), 39U);
  EXPECT_EQ(project.project->subjects.size(), 41U);
  EXPECT_EQ(project.project->rooms.size(), 41U);
  EXPECT_EQ(project.project->meetings.size(), 952U);
  EXPECT_EQ(project.project->calendar.periods.size(), 13U);
  EXPECT_EQ(project.project->calendar.slots.size(), 78U);
  const int meeting_periods =
      std::accumulate(project.project->meetings.begin(), project.project->meetings.end(), 0,
                      [](int total, const domain::Meeting& meeting) {
                        return total + meeting.duration_in_periods;
                      });
  EXPECT_EQ(meeting_periods, 969);
  const validation::RoomAuditReport room_audit = validation::audit_rooms(*project.project);
  EXPECT_EQ(room_audit.statistics.meetings_without_rooms, 0U);
  EXPECT_GE(room_audit.statistics.room_lanes, project.project->meetings.size());
  EXPECT_GT(room_audit.statistics.alternative_room_lanes, 0U);
  EXPECT_GT(room_audit.statistics.feature_room_lanes, 0U);
  EXPECT_FALSE(room_audit.has_proven_capacity_overload());
  EXPECT_NE(output.str().find("Migrated legacy project"), std::string::npos);
  EXPECT_EQ(errors.str().find("error "), std::string::npos);
  EXPECT_NE(errors.str().find("legacy.classrooms.special_code_interpreted"), std::string::npos);
  EXPECT_NE(errors.str().find("legacy.entire_course_per_day.unimplemented_legacy_setting"),
            std::string::npos);

  std::ostringstream second_output;
  std::ostringstream second_errors;
  ASSERT_EQ(migrate_legacy_project("data/settings.conf", second_output_path.string(), second_output,
                                   second_errors),
            kExitSuccess);
  std::ifstream second_input(second_output_path, std::ios::binary);
  const std::string second_contents{std::istreambuf_iterator<char>{second_input},
                                    std::istreambuf_iterator<char>{}};
  second_input.close();
  EXPECT_EQ(second_contents, contents);
  std::filesystem::remove(output_path);
  std::filesystem::remove(second_output_path);
}

}  // namespace
}  // namespace schedmesh::app
