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

TEST(LegacyProjectImportTest, AssignsOneRoomLaneToEachSimultaneousSubgroup) {
  const CsvTable table = {{"", "Shifts", "1"},
                          {"", "Group", "5a"},
                          {"", "Double lessons", ""},
                          {"Teacher", "Subject", ""},
                          {"Teacher A", "Language", "1"},
                          {"Teacher B", "Language", "1"}};
  LegacyProjectImportResult timetable_result = import_legacy_timetable(settings(), table);
  ASSERT_TRUE(timetable_result.ok());

  const LegacyProjectImportResult result = import_legacy_resources(
      std::move(*timetable_result.project),
      {{"Teacher", "Rooms"}, {"Teacher A", "101;102"}, {"Teacher B", "101;102"}});

  ASSERT_TRUE(result.ok());
  ASSERT_EQ(result.project->meetings.size(), 1U);
  const domain::Meeting& meeting = result.project->meetings.front();
  ASSERT_EQ(meeting.teacher_requirements.size(), 2U);
  ASSERT_EQ(meeting.room_requirements.size(), 2U);
  EXPECT_EQ(meeting.room_requirements[0].lane, 0);
  EXPECT_EQ(meeting.room_requirements[1].lane, 1);
  EXPECT_EQ(meeting.room_requirements[0].candidates.size(), 2U);
  EXPECT_EQ(meeting.room_requirements[1].candidates.size(), 2U);
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

TEST(LegacyProjectImportTest, RemovesForbiddenSessionBoundarySlots) {
  LegacySettings legacy_settings = settings();
  legacy_settings.not_first_or_last = {"Math"};
  const CsvTable table = {{"", "Shifts", "2"},
                          {"", "Group", "5a"},
                          {"", "Double lessons", ""},
                          {"Teacher", "Subject", ""},
                          {"Teacher A", "Math", "1"}};

  const LegacyProjectImportResult result = import_legacy_timetable(legacy_settings, table);

  ASSERT_TRUE(result.ok());
  ASSERT_EQ(result.project->subjects.size(), 1U);
  EXPECT_TRUE(result.project->subjects.front().forbid_first_period);
  EXPECT_TRUE(result.project->subjects.front().forbid_last_period);
  ASSERT_EQ(result.project->meetings.size(), 1U);
  EXPECT_EQ(result.project->meetings.front().allowed_start_slots.size(), 2U);
}

TEST(LegacyProjectImportTest, ExpandsDoubleLessonsIntoConsecutiveMeetings) {
  LegacySettings legacy_settings = settings();
  legacy_settings.double_lessons = {"Math"};
  const CsvTable table = {{"", "Shifts", "1"},
                          {"", "Group", "5a"},
                          {"", "Double lessons", ""},
                          {"Teacher", "Subject", ""},
                          {"Teacher A", "Math", "4"}};

  const LegacyProjectImportResult result = import_legacy_timetable(legacy_settings, table);

  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.project->subjects.front().required_consecutive_periods, 2);
  ASSERT_EQ(result.project->meetings.size(), 2U);
  EXPECT_EQ(result.project->meetings.front().duration_in_periods, 2);
  EXPECT_EQ(result.project->meetings.front().allowed_start_slots.size(), 4U);
  EXPECT_EQ(result.project->teachers.front().maximum_weekly_load, 4);
}

TEST(LegacyProjectImportTest, RejectsIncompleteDoubleLessonBlock) {
  LegacySettings legacy_settings = settings();
  legacy_settings.double_lessons = {"Math"};
  const CsvTable table = {{"", "Shifts", "1"},
                          {"", "Group", "5a"},
                          {"", "Double lessons", ""},
                          {"Teacher", "Subject", ""},
                          {"Teacher A", "Math", "3"}};

  const LegacyProjectImportResult result = import_legacy_timetable(legacy_settings, table);

  EXPECT_FALSE(result.ok());
  EXPECT_FALSE(result.project.has_value());
  EXPECT_EQ(result.report.diagnostics.back().code, "legacy.timetable.incomplete_consecutive_block");
}

TEST(LegacyProjectImportTest, ImportsSubjectConflictsAndRepeatedSubjectGroupPolicy) {
  LegacySettings legacy_settings = settings();
  legacy_settings.conflicts = {{"Math", "Science"}};
  const CsvTable table = {{"", "Shifts", "1"},         {"", "Group", "5a"},
                          {"", "Double lessons", "1"}, {"Teacher", "Subject", ""},
                          {"Teacher A", "Math", "1"},  {"", "Science", "1"}};

  const LegacyProjectImportResult result = import_legacy_timetable(legacy_settings, table);

  ASSERT_TRUE(result.ok());
  EXPECT_TRUE(result.project->student_groups.front().allow_repeated_subjects_per_day);
  ASSERT_EQ(result.project->subjects.size(), 2U);
  EXPECT_EQ(result.project->subjects[0].conflicting_subjects,
            std::vector{result.project->subjects[1].id});
  EXPECT_EQ(result.project->subjects[1].conflicting_subjects,
            std::vector{result.project->subjects[0].id});
}

TEST(LegacyProjectImportTest, DiagnosesLegacySettingThatNeverAffectedScheduling) {
  LegacySettings legacy_settings = settings();
  legacy_settings.entire_course_per_day = {"Math"};
  const CsvTable table = {{"", "Shifts", "1"},
                          {"", "Group", "5a"},
                          {"", "Double lessons", ""},
                          {"Teacher", "Subject", ""},
                          {"Teacher A", "Math", "1"}};

  const LegacyProjectImportResult result = import_legacy_timetable(legacy_settings, table);

  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.report.ignored_fields, 1U);
  EXPECT_EQ(result.report.diagnostics.back().code,
            "legacy.entire_course_per_day.unimplemented_legacy_setting");
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

TEST(LegacyProjectImportTest, SharesRoomIdentityAndDeduplicatesAlternatives) {
  const CsvTable timetable = {{"", "Shifts", "1"},        {"", "Group", "5a"},
                              {"", "Double lessons", ""}, {"Teacher", "Subject", ""},
                              {"Teacher A", "Math", "1"}, {"Teacher B", "Science", "1"}};
  LegacyProjectImportResult timetable_result = import_legacy_timetable(settings(), timetable);
  ASSERT_TRUE(timetable_result.ok());

  const LegacyProjectImportResult result = import_legacy_resources(
      std::move(*timetable_result.project),
      {{"Teacher", "Rooms"}, {"Teacher A", "101;102;101"}, {"Teacher B", "102;103"}});

  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.project->rooms.size(), 3U);
  ASSERT_EQ(result.project->meetings.size(), 2U);
  const auto& first = result.project->meetings[0].room_requirements.front().candidates;
  const auto& second = result.project->meetings[1].room_requirements.front().candidates;
  ASSERT_EQ(first.size(), 2U);
  ASSERT_EQ(second.size(), 2U);
  EXPECT_EQ(first[1], second[0]);
}

TEST(LegacyProjectImportTest, DisambiguatesNormalizedRoomIdCollisions) {
  const CsvTable timetable = {{"", "Shifts", "1"},
                              {"", "Group", "5a"},
                              {"", "Double lessons", ""},
                              {"Teacher", "Subject", ""},
                              {"Teacher A", "Math", "1"}};
  LegacyProjectImportResult timetable_result = import_legacy_timetable(settings(), timetable);
  ASSERT_TRUE(timetable_result.ok());

  const LegacyProjectImportResult result = import_legacy_resources(
      std::move(*timetable_result.project), {{"Teacher", "Rooms"}, {"Teacher A", "Room A;Room-A"}});

  ASSERT_TRUE(result.ok());
  ASSERT_EQ(result.project->rooms.size(), 2U);
  EXPECT_EQ(result.project->rooms[0].id, domain::RoomId{"room-room-a"});
  EXPECT_EQ(result.project->rooms[1].id, domain::RoomId{"room-room-a-2"});
}

TEST(LegacyProjectImportTest, ReconstructsTwoLegacyGymLanes) {
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
  ASSERT_EQ(result.project->rooms.size(), 2U);
  EXPECT_TRUE(result.project->rooms[0].features.contains("gym"));
  ASSERT_EQ(result.project->meetings.front().room_requirements.size(), 1U);
  EXPECT_EQ(result.project->meetings.front().room_requirements.front().candidates.size(), 2U);
  EXPECT_EQ(result.report.consumed_fields, 1U);
  ASSERT_EQ(result.report.diagnostics.size(), 1U);
  EXPECT_EQ(result.report.diagnostics.front().code, "legacy.classrooms.special_code_interpreted");
}

TEST(LegacyProjectImportTest, ReconstructsDedicatedLegacyTechnologyRoom) {
  const CsvTable timetable = {{"", "Shifts", "1"},
                              {"", "Group", "5a"},
                              {"", "Double lessons", ""},
                              {"Teacher", "Subject", ""},
                              {"Teacher A", "Technology", "1"}};
  LegacyProjectImportResult timetable_result = import_legacy_timetable(settings(), timetable);
  ASSERT_TRUE(timetable_result.ok());

  const LegacyProjectImportResult result = import_legacy_resources(
      std::move(*timetable_result.project), {{"Teacher", "Rooms"}, {"Teacher A", "T"}});

  ASSERT_TRUE(result.ok());
  ASSERT_EQ(result.project->rooms.size(), 1U);
  EXPECT_TRUE(result.project->rooms.front().features.contains("technology"));
  ASSERT_EQ(result.project->meetings.front().room_requirements.size(), 1U);
  EXPECT_EQ(result.project->meetings.front().room_requirements.front().candidates,
            (std::vector<domain::RoomId>{result.project->rooms.front().id}));
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
