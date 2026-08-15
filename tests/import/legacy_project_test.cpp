#include "schedmesh/import/legacy_project.h"

#include <gtest/gtest.h>

namespace schedmesh::import {
namespace {

LegacySettings settings() {
  return {.days = 2,
          .maximum_lessons_per_session = 3,
          .sessions = 2,
          .day_names = {"Monday", "Tuesday"}};
}

TEST(LegacyProjectImportTest, MapsGroupsSessionsTeachersSubjectsAndHours) {
  const CsvTable table = {{"", "Shifts", "1", "2"},       {"", "Group", "5a", "6a"},
                          {"", "Double lessons", "", ""}, {"Teacher", "Subject", "", ""},
                          {"Teacher A", "Math", "2", ""}, {"", "Science", "", "1"}};

  const LegacyProjectImportResult result = import_legacy_timetable(settings(), table);

  ASSERT_TRUE(result.ok());
  ASSERT_EQ(result.project->calendar.periods.size(), 5U);
  ASSERT_EQ(result.project->student_groups.size(), 2U);
  EXPECT_EQ(result.project->student_groups[0].allowed_slots.size(), 6U);
  EXPECT_EQ(result.project->student_groups[1].allowed_slots.size(), 6U);
  ASSERT_EQ(result.project->teachers.size(), 1U);
  EXPECT_EQ(result.project->teachers[0].maximum_weekly_load, 3);
  EXPECT_EQ(result.project->subjects.size(), 2U);
  EXPECT_EQ(result.project->meetings.size(), 3U);
}

TEST(LegacyProjectImportTest, ConvertsRepeatedSubjectRowsIntoSimultaneousTeacherLanes) {
  const CsvTable table = {{"", "Shifts", "1"},
                          {"", "Group", "5a"},
                          {"", "Double lessons", ""},
                          {"Teacher", "Subject", ""},
                          {"Teacher A", "Language", "2"},
                          {"Teacher B", "Language", "2"}};

  const LegacyProjectImportResult result = import_legacy_timetable(settings(), table);

  ASSERT_TRUE(result.ok());
  ASSERT_EQ(result.project->meetings.size(), 2U);
  EXPECT_EQ(result.project->meetings[0].teacher_requirements.size(), 2U);
  EXPECT_EQ(result.project->meetings[0].teacher_requirements[0].lane, 0);
  EXPECT_EQ(result.project->meetings[0].teacher_requirements[1].lane, 1);
}

TEST(LegacyProjectImportTest, RejectsInvalidHoursWithoutReturningPartialProject) {
  const CsvTable table = {{"", "Shifts", "1"},
                          {"", "Group", "5a"},
                          {"", "Double lessons", ""},
                          {"Teacher", "Subject", ""},
                          {"Teacher A", "Math", "two"}};

  const LegacyProjectImportResult result = import_legacy_timetable(settings(), table);

  EXPECT_FALSE(result.ok());
  EXPECT_FALSE(result.project.has_value());
  ASSERT_EQ(result.report.diagnostics.size(), 1U);
  EXPECT_EQ(result.report.diagnostics[0].code, "legacy.timetable.invalid_weekly_hours");
}

}  // namespace
}  // namespace schedmesh::import
