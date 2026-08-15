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

TEST(LegacyProjectImportTest, DisambiguatesSubjectIdsAfterNormalization) {
  const CsvTable table = {{"", "Shifts", "1"},           {"", "Group", "5a"},
                          {"", "Double lessons", ""},    {"Teacher", "Subject", ""},
                          {"Teacher A", "Italian", "1"}, {"", "Italian+", "1"}};

  const LegacyProjectImportResult result = import_legacy_timetable(settings(), table);

  ASSERT_TRUE(result.ok());
  ASSERT_EQ(result.project->subjects.size(), 2U);
  EXPECT_EQ(result.project->subjects[0].id.value(), "subject-italian");
  EXPECT_EQ(result.project->subjects[1].id.value(), "subject-italian-2");
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

TEST(LegacyProjectImportTest, AddsRoomCandidatesAndMethodicalDayAvailability) {
  const CsvTable timetable = {{"", "Shifts", "1"},
                              {"", "Group", "5a"},
                              {"", "Double lessons", ""},
                              {"Teacher", "Subject", ""},
                              {"Teacher A", "Math", "1"}};
  LegacyProjectImportResult timetable_result = import_legacy_timetable(settings(), timetable);
  ASSERT_TRUE(timetable_result.ok());
  const CsvTable classrooms = {{"Teacher", "Rooms"}, {"Teacher A", "101;102"}};
  const CsvTable methodical_days = {{"Teacher", "Days"}, {"Teacher A", "Tuesday"}};

  const LegacyProjectImportResult result =
      import_legacy_resources(std::move(*timetable_result.project), classrooms, methodical_days);

  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.project->rooms.size(), 2U);
  ASSERT_EQ(result.project->meetings.front().room_requirements.size(), 1U);
  EXPECT_EQ(result.project->meetings.front().room_requirements.front().candidates.size(), 2U);
  EXPECT_EQ(result.project->teachers.front().unavailable_slots.size(), 5U);
}

TEST(LegacyProjectImportTest, DiagnosesSpecialRoomCodesWithoutInventingFacilities) {
  const CsvTable timetable = {{"", "Shifts", "1"},
                              {"", "Group", "5a"},
                              {"", "Double lessons", ""},
                              {"Teacher", "Subject", ""},
                              {"Teacher A", "Sports", "1"}};
  LegacyProjectImportResult timetable_result = import_legacy_timetable(settings(), timetable);
  ASSERT_TRUE(timetable_result.ok());

  const LegacyProjectImportResult result = import_legacy_resources(
      std::move(*timetable_result.project), {{"Teacher", "Rooms"}, {"Teacher A", "S"}});

  ASSERT_TRUE(result.ok());
  EXPECT_TRUE(result.project->rooms.empty());
  EXPECT_EQ(result.report.ignored_fields, 1U);
  ASSERT_EQ(result.report.diagnostics.size(), 1U);
  EXPECT_EQ(result.report.diagnostics.front().code, "legacy.classrooms.special_code");
}

TEST(LegacyProjectImportTest, RejectsUnknownMethodicalDayWithoutPartialProject) {
  const CsvTable timetable = {{"", "Shifts", "1"},
                              {"", "Group", "5a"},
                              {"", "Double lessons", ""},
                              {"Teacher", "Subject", ""},
                              {"Teacher A", "Math", "1"}};
  LegacyProjectImportResult timetable_result = import_legacy_timetable(settings(), timetable);
  ASSERT_TRUE(timetable_result.ok());

  const LegacyProjectImportResult result = import_legacy_resources(
      std::move(*timetable_result.project), {{"Teacher", "Rooms"}, {"Teacher A", "101"}},
      CsvTable{{"Teacher", "Days"}, {"Teacher A", "Funday"}});

  EXPECT_FALSE(result.ok());
  EXPECT_FALSE(result.project.has_value());
  EXPECT_EQ(result.report.diagnostics.back().code, "legacy.methodical_days.unknown_day");
}

}  // namespace
}  // namespace schedmesh::import
