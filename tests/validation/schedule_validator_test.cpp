#include "schedmesh/validation/schedule_validator.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <string_view>

#include "fixtures/tiny_project.h"

namespace schedmesh::validation {
namespace {

bool has_code(const ValidationResult& result, std::string_view code) {
  return std::ranges::any_of(
      result.diagnostics, [code](const Diagnostic& diagnostic) { return diagnostic.code == code; });
}

domain::Schedule tiny_schedule() {
  return {.meetings = {{.meeting = domain::MeetingId{"meeting-001"},
                        .start_slot = domain::SlotId{"slot-mon-p1"},
                        .teachers = {domain::TeacherId{"teacher-001"}},
                        .rooms = {domain::RoomId{"room-001"}}}}};
}

domain::Project two_period_project() {
  domain::Project project = test::make_tiny_project();
  project.calendar =
      domain::make_calendar({{.id = "mon", .display_name = "Day 1", .ordinal = 0}},
                            {{.id = "p1", .ordinal = 0}, {.id = "p2", .ordinal = 1}});
  project.student_groups.front().allowed_slots = {domain::SlotId{"slot-mon-p1"},
                                                  domain::SlotId{"slot-mon-p2"}};
  project.meetings.front().allowed_start_slots = project.student_groups.front().allowed_slots;
  return project;
}

TEST(ScheduleValidatorTest, AcceptsCompleteFeasibleSchedule) {
  const domain::Project project = test::make_tiny_project();

  const ValidationResult result = ScheduleValidator{}.validate(project, tiny_schedule());

  EXPECT_TRUE(result.ok());
  EXPECT_TRUE(result.diagnostics.empty());
}

TEST(ScheduleValidatorTest, RejectsMissingMeeting) {
  const domain::Project project = test::make_tiny_project();

  const ValidationResult result = ScheduleValidator{}.validate(project, {});

  EXPECT_FALSE(result.ok());
  EXPECT_TRUE(has_code(result, "schedule.missing_meeting"));
}

TEST(ScheduleValidatorTest, RejectsResourceAndGroupOverlaps) {
  domain::Project project = test::make_tiny_project();
  domain::Meeting second = project.meetings.front();
  second.id = domain::MeetingId{"meeting-002"};
  project.meetings.push_back(second);
  domain::Schedule schedule = tiny_schedule();
  schedule.meetings.push_back({.meeting = second.id,
                               .start_slot = domain::SlotId{"slot-mon-p1"},
                               .teachers = {domain::TeacherId{"teacher-001"}},
                               .rooms = {domain::RoomId{"room-001"}}});

  const ValidationResult result = ScheduleValidator{}.validate(project, schedule);

  EXPECT_FALSE(result.ok());
  EXPECT_TRUE(has_code(result, "schedule.group_overlap"));
  EXPECT_TRUE(has_code(result, "schedule.teacher_overlap"));
  EXPECT_TRUE(has_code(result, "schedule.room_overlap"));
  EXPECT_TRUE(has_code(result, "schedule.repeated_subject_on_day"));
}

TEST(ScheduleValidatorTest, RejectsWrongResourceAssignments) {
  const domain::Project project = test::make_tiny_project();
  domain::Schedule schedule = tiny_schedule();
  schedule.meetings.front().teachers = {domain::TeacherId{"teacher-missing"}};
  schedule.meetings.front().rooms = {domain::RoomId{"room-missing"}};

  const ValidationResult result = ScheduleValidator{}.validate(project, schedule);

  EXPECT_FALSE(result.ok());
  EXPECT_TRUE(has_code(result, "schedule.ineligible_teacher"));
  EXPECT_TRUE(has_code(result, "schedule.ineligible_room"));
}

TEST(ScheduleValidatorTest, RejectsRoomBelowLaneCapacity) {
  constexpr int kRequiredCapacity = 31;
  constexpr int kLargeRoomCapacity = 40;
  domain::Project project = test::make_tiny_project();
  project.rooms.push_back(
      {.id = domain::RoomId{"room-002"}, .display_name = "Room 2", .capacity = kLargeRoomCapacity});
  auto& requirement = project.meetings.front().room_requirements.front();
  requirement.fixed_room.reset();
  requirement.candidates = {domain::RoomId{"room-001"}, domain::RoomId{"room-002"}};
  requirement.minimum_capacity = kRequiredCapacity;

  const ValidationResult result = ScheduleValidator{}.validate(project, tiny_schedule());

  EXPECT_FALSE(result.ok());
  EXPECT_TRUE(has_code(result, "schedule.room_capacity"));
}

TEST(ScheduleValidatorTest, RejectsConcurrentGymUseByDistantGrades) {
  constexpr int kFirstGrade = 5;
  constexpr int kDistantGrade = 8;
  domain::Project project = test::make_tiny_project();
  project.student_groups.front().grade = kFirstGrade;
  project.rooms.front().features = {"gym"};
  project.rooms.push_back(
      {.id = domain::RoomId{"room-002"}, .display_name = "Gym lane 2", .features = {"gym"}});
  auto& first_requirement = project.meetings.front().room_requirements.front();
  first_requirement.fixed_room.reset();
  first_requirement.candidates = {domain::RoomId{"room-001"}, domain::RoomId{"room-002"}};
  first_requirement.required_features = {"gym"};

  project.student_groups.push_back(project.student_groups.front());
  project.student_groups.back().id = domain::StudentGroupId{"group-02"};
  project.student_groups.back().grade = kDistantGrade;
  project.teachers.push_back(project.teachers.front());
  project.teachers.back().id = domain::TeacherId{"teacher-002"};
  domain::Meeting second = project.meetings.front();
  second.id = domain::MeetingId{"meeting-002"};
  second.groups = {domain::StudentGroupId{"group-02"}};
  second.teacher_requirements = {{.fixed_teacher = domain::TeacherId{"teacher-002"}, .lane = 0}};
  second.distribution_key = "math-group-02";
  project.meetings.push_back(second);

  domain::Schedule schedule = tiny_schedule();
  schedule.meetings.push_back({.meeting = second.id,
                               .start_slot = domain::SlotId{"slot-mon-p1"},
                               .teachers = {domain::TeacherId{"teacher-002"}},
                               .rooms = {domain::RoomId{"room-002"}}});

  const ValidationResult result = ScheduleValidator{}.validate(project, schedule);

  EXPECT_FALSE(result.ok());
  EXPECT_TRUE(has_code(result, "schedule.gym_grade_conflict"));
}

TEST(ScheduleValidatorTest, RejectsTeacherDailyLoadExcess) {
  domain::Project project = two_period_project();
  project.teachers.front().maximum_daily_load = 1;
  project.student_groups.front().allow_repeated_subjects_per_day = true;
  domain::Meeting second = project.meetings.front();
  second.id = domain::MeetingId{"meeting-002"};
  project.meetings.push_back(second);
  domain::Schedule schedule = tiny_schedule();
  schedule.meetings.push_back({.meeting = second.id,
                               .start_slot = domain::SlotId{"slot-mon-p2"},
                               .teachers = {domain::TeacherId{"teacher-001"}},
                               .rooms = {domain::RoomId{"room-001"}}});

  const ValidationResult result = ScheduleValidator{}.validate(project, schedule);

  EXPECT_FALSE(result.ok());
  EXPECT_TRUE(has_code(result, "schedule.teacher_daily_load"));
}

TEST(ScheduleValidatorTest, RejectsConflictingSubjectsOnTheSameDay) {
  domain::Project project = two_period_project();
  project.subjects.front().conflicting_subjects = {domain::SubjectId{"subject-science"}};
  project.subjects.push_back({.id = domain::SubjectId{"subject-science"},
                              .display_name = "Subject 2",
                              .conflicting_subjects = {domain::SubjectId{"subject-math"}}});
  project.teachers.front().qualified_subjects.emplace_back("subject-science");
  domain::Meeting second = project.meetings.front();
  second.id = domain::MeetingId{"meeting-002"};
  second.subject = domain::SubjectId{"subject-science"};
  project.meetings.push_back(second);
  domain::Schedule schedule = tiny_schedule();
  schedule.meetings.push_back({.meeting = second.id,
                               .start_slot = domain::SlotId{"slot-mon-p2"},
                               .teachers = {domain::TeacherId{"teacher-001"}},
                               .rooms = {domain::RoomId{"room-001"}}});

  const ValidationResult result = ScheduleValidator{}.validate(project, schedule);

  EXPECT_FALSE(result.ok());
  EXPECT_TRUE(has_code(result, "schedule.subject_day_conflict"));
}

}  // namespace
}  // namespace schedmesh::validation
