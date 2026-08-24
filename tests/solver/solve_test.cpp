#include "schedmesh/solver/solve.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <stop_token>

#include "fixtures/medium_project.h"
#include "fixtures/tiny_project.h"
#include "schedmesh/validation/room_audit.h"
#include "schedmesh/validation/schedule_validator.h"

namespace schedmesh::solver {
namespace {

domain::Project project_with_calendar(std::vector<domain::Day> days,
                                      std::vector<domain::Period> periods) {
  domain::Project project = test::make_tiny_project();
  project.calendar = domain::make_calendar(std::move(days), std::move(periods));
  std::vector<domain::SlotId> allowed_slots;
  allowed_slots.reserve(project.calendar.slots.size());
  for (const domain::Slot& slot : project.calendar.slots) {
    allowed_slots.push_back(slot.id);
  }
  project.student_groups.front().allowed_slots = allowed_slots;
  project.meetings.front().allowed_start_slots = allowed_slots;
  return project;
}

domain::Project one_day_two_period_project() {
  return project_with_calendar({{.id = "mon", .display_name = "Day 1", .ordinal = 0}},
                               {{.id = "p1", .ordinal = 0}, {.id = "p2", .ordinal = 1}});
}

domain::Project one_day_three_period_project() {
  return project_with_calendar(
      {{.id = "mon", .display_name = "Day 1", .ordinal = 0}},
      {{.id = "p1", .ordinal = 0}, {.id = "p2", .ordinal = 1}, {.id = "p3", .ordinal = 2}});
}

domain::Project two_day_project() {
  return project_with_calendar({{.id = "mon", .display_name = "Day 1", .ordinal = 0},
                                {.id = "tue", .display_name = "Day 2", .ordinal = 1}},
                               {{.id = "p1", .ordinal = 0}});
}

void add_second_meeting(domain::Project& project) {
  domain::Meeting second = project.meetings.front();
  second.id = domain::MeetingId{"meeting-002"};
  project.meetings.push_back(std::move(second));
}

void make_second_subject_conflicting(domain::Project& project) {
  project.subjects.front().conflicting_subjects = {domain::SubjectId{"subject-science"}};
  project.subjects.push_back({.id = domain::SubjectId{"subject-science"},
                              .display_name = "Subject 2",
                              .conflicting_subjects = {domain::SubjectId{"subject-math"}}});
  project.teachers.front().qualified_subjects.emplace_back("subject-science");
  project.meetings.back().subject = domain::SubjectId{"subject-science"};
}

TEST(SolveTest, SolvesTinyProjectAndReturnsValidatedSchedule) {
  const domain::Project project = test::make_tiny_project();

  const SolveResult result = solve({.project = project});

  EXPECT_EQ(result.status, SolveStatus::kOptimal);
  ASSERT_TRUE(result.schedule.has_value());
  ASSERT_EQ(result.schedule->meetings.size(), 1U);
  EXPECT_EQ(result.schedule->meetings.front().start_slot, domain::SlotId{"slot-mon-p1"});
  EXPECT_TRUE(result.diagnostics.ok());
}

TEST(SolveTest, ProvesGroupConflictInfeasible) {
  domain::Project project = test::make_tiny_project();
  domain::Meeting second = project.meetings.front();
  second.id = domain::MeetingId{"meeting-002"};
  project.meetings.push_back(second);
  project.student_groups.front().allow_repeated_subjects_per_day = true;

  const SolveResult result = solve({.project = project});

  EXPECT_EQ(result.status, SolveStatus::kInfeasible);
  EXPECT_FALSE(result.schedule.has_value());
}

TEST(SolveTest, ChoosesNonConflictingCandidateRooms) {
  domain::Project project = test::make_tiny_project();
  project.rooms.push_back({.id = domain::RoomId{"room-002"},
                           .display_name = "Room 2",
                           .capacity = project.rooms.front().capacity});
  project.teachers.push_back({.id = domain::TeacherId{"teacher-002"},
                              .display_name = "Teacher 2",
                              .qualified_subjects = {domain::SubjectId{"subject-math"}},
                              .maximum_weekly_load = project.teachers.front().maximum_weekly_load});
  auto& room_requirement = project.meetings.front().room_requirements.front();
  room_requirement.fixed_room.reset();
  room_requirement.candidates = {domain::RoomId{"room-001"}, domain::RoomId{"room-002"}};
  domain::Meeting second = project.meetings.front();
  second.id = domain::MeetingId{"meeting-002"};
  second.teacher_requirements.front().fixed_teacher = domain::TeacherId{"teacher-002"};
  project.meetings.push_back(second);
  project.student_groups.push_back({.id = domain::StudentGroupId{"group-02"},
                                    .display_name = "Group 2",
                                    .grade = project.student_groups.front().grade,
                                    .allowed_slots = project.student_groups.front().allowed_slots});
  project.meetings.back().groups = {domain::StudentGroupId{"group-02"}};
  project.student_groups.front().allow_repeated_subjects_per_day = true;
  project.student_groups.back().allow_repeated_subjects_per_day = true;

  const SolveResult result = solve({.project = project});

  ASSERT_EQ(result.status, SolveStatus::kOptimal);
  ASSERT_TRUE(result.schedule.has_value());
  ASSERT_EQ(result.schedule->meetings.size(), 2U);
  EXPECT_NE(result.schedule->meetings[0].rooms, result.schedule->meetings[1].rooms);
}

TEST(SolveTest, ChoosesRoomMeetingTheLaneCapacity) {
  constexpr int kLargeRoomCapacity = 40;
  constexpr int kRequiredCapacity = 35;
  domain::Project project = test::make_tiny_project();
  project.rooms.push_back(
      {.id = domain::RoomId{"room-002"}, .display_name = "Room 2", .capacity = kLargeRoomCapacity});
  auto& requirement = project.meetings.front().room_requirements.front();
  requirement.fixed_room.reset();
  requirement.candidates = {domain::RoomId{"room-001"}, domain::RoomId{"room-002"}};
  requirement.minimum_capacity = kRequiredCapacity;

  const SolveResult result = solve({.project = project});

  ASSERT_TRUE(result.schedule.has_value());
  ASSERT_EQ(result.schedule->meetings.front().rooms.size(), 1U);
  EXPECT_EQ(result.schedule->meetings.front().rooms.front(), domain::RoomId{"room-002"});
}

TEST(SolveTest, RejectsConcurrentGymUseByDistantGrades) {
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
  project.meetings.push_back(std::move(second));

  const SolveResult result = solve({.project = project});

  EXPECT_EQ(result.status, SolveStatus::kInfeasible);
}

TEST(SolveTest, SolvesPublicMediumRoomFixture) {
  const domain::Project project = test::make_medium_project();
  const validation::RoomAuditReport audit = validation::audit_rooms(project);

  EXPECT_EQ(audit.statistics.meetings_without_rooms, 0U);
  EXPECT_GT(audit.statistics.alternative_room_lanes, 0U);
  EXPECT_GT(audit.statistics.feature_room_lanes, 0U);
  EXPECT_GT(audit.statistics.capacity_room_lanes, 0U);
  EXPECT_FALSE(audit.has_proven_capacity_overload());

  const SolveResult result = solve({.project = project});

  EXPECT_EQ(result.status, SolveStatus::kOptimal);
  ASSERT_TRUE(result.schedule.has_value());
  EXPECT_EQ(result.schedule->meetings.size(), project.meetings.size());
  EXPECT_TRUE(result.diagnostics.ok());
}

TEST(SolveTest, RejectsInvalidProjectBeforeCallingCpSat) {
  domain::Project project = test::make_tiny_project();
  project.meetings.front().allowed_start_slots.clear();

  const SolveResult result = solve({.project = project});

  EXPECT_EQ(result.status, SolveStatus::kInvalidProject);
  EXPECT_FALSE(result.schedule.has_value());
  EXPECT_FALSE(result.diagnostics.ok());
}

TEST(SolveTest, RejectsInvalidExecutionParameters) {
  const domain::Project project = test::make_tiny_project();

  const SolveResult result =
      solve({.project = project,
             .parameters = {.time_limit = std::chrono::milliseconds::zero(), .worker_count = 0}});

  EXPECT_EQ(result.status, SolveStatus::kInvalidParameters);
  EXPECT_FALSE(result.schedule.has_value());
  EXPECT_FALSE(result.diagnostics.ok());
}

TEST(SolveTest, HonorsCancellationRequestedBeforeSearch) {
  const domain::Project project = test::make_tiny_project();
  std::stop_source cancellation;
  cancellation.request_stop();

  const SolveResult result = solve({.project = project, .cancellation = cancellation.get_token()});

  EXPECT_EQ(result.status, SolveStatus::kCancelled);
  EXPECT_FALSE(result.schedule.has_value());
}

TEST(SolveTest, AppliesShortTimeLimitWithinShutdownMargin) {
  const domain::Project project = test::make_tiny_project();
  constexpr auto kTimeLimit = std::chrono::milliseconds{1};
  constexpr auto kShutdownMargin = std::chrono::seconds{1};

  const SolveResult result = solve({.project = project, .parameters = {.time_limit = kTimeLimit}});

  EXPECT_TRUE(result.status == SolveStatus::kOptimal || result.status == SolveStatus::kTimeLimit);
  EXPECT_LE(result.statistics.elapsed, kTimeLimit + kShutdownMargin);
}

TEST(SolveTest, SpreadsTeacherLoadAcrossDays) {
  domain::Project project = two_day_project();
  project.teachers.front().maximum_daily_load = 1;
  project.student_groups.front().allow_repeated_subjects_per_day = true;
  add_second_meeting(project);

  const SolveResult result = solve({.project = project});

  EXPECT_EQ(result.status, SolveStatus::kOptimal);
  ASSERT_TRUE(result.schedule.has_value());
  EXPECT_TRUE(result.diagnostics.ok());
}

TEST(SolveTest, ProvesTeacherDailyLoadExcessInfeasible) {
  domain::Project project = one_day_two_period_project();
  project.teachers.front().maximum_daily_load = 1;
  project.student_groups.front().allow_repeated_subjects_per_day = true;
  add_second_meeting(project);

  const SolveResult result = solve({.project = project});

  EXPECT_EQ(result.status, SolveStatus::kInfeasible);
}

TEST(SolveTest, ProvesTeacherWeeklyLoadExcessInfeasible) {
  domain::Project project = two_day_project();
  project.teachers.front().maximum_weekly_load = 1;
  project.student_groups.front().allow_repeated_subjects_per_day = true;
  add_second_meeting(project);

  const SolveResult result = solve({.project = project});

  EXPECT_EQ(result.status, SolveStatus::kInfeasible);
}

TEST(SolveTest, ProvesRepeatedSubjectOnOneDayInfeasible) {
  domain::Project project = one_day_two_period_project();
  add_second_meeting(project);

  const SolveResult result = solve({.project = project});

  EXPECT_EQ(result.status, SolveStatus::kInfeasible);
}

TEST(SolveTest, AllowsDistinctCoursesOfTheSameSubjectOnOneDay) {
  domain::Project project = one_day_two_period_project();
  project.teachers.front().maximum_weekly_load = 2;
  add_second_meeting(project);
  project.meetings.back().distribution_key = "math-subgroup-course";

  const SolveResult result = solve({.project = project});

  EXPECT_EQ(result.status, SolveStatus::kOptimal);
  ASSERT_TRUE(result.schedule.has_value());
  EXPECT_TRUE(result.diagnostics.ok());
}

TEST(SolveTest, AppliesSubjectSpecificDailyOccurrenceLimit) {
  domain::Project project = one_day_three_period_project();
  project.student_groups.front().allow_repeated_subjects_per_day = true;
  project.subjects.front().maximum_occurrences_per_day = 2;
  add_second_meeting(project);

  const SolveResult two_occurrences = solve({.project = project});

  EXPECT_EQ(two_occurrences.status, SolveStatus::kOptimal);
  domain::Meeting third = project.meetings.front();
  third.id = domain::MeetingId{"meeting-003"};
  project.meetings.push_back(std::move(third));

  const SolveResult three_occurrences = solve({.project = project});

  EXPECT_EQ(three_occurrences.status, SolveStatus::kInfeasible);
}

TEST(SolveTest, StartsLinkedMeetingsSimultaneously) {
  constexpr int kMaximumWeeklyLoad = 20;
  constexpr int kRoomCapacity = 30;
  domain::Project project = one_day_two_period_project();
  project.teachers.push_back({.id = domain::TeacherId{"teacher-002"},
                              .display_name = "Teacher 2",
                              .qualified_subjects = {domain::SubjectId{"subject-math"}},
                              .maximum_weekly_load = kMaximumWeeklyLoad});
  project.student_groups.push_back({.id = domain::StudentGroupId{"group-02"},
                                    .display_name = "Group 2",
                                    .allowed_slots = project.student_groups.front().allowed_slots});
  project.rooms.push_back(
      {.id = domain::RoomId{"room-002"}, .display_name = "Room 2", .capacity = kRoomCapacity});
  project.meetings.front().simultaneity_keys = {"linked-lessons"};
  domain::Meeting second = project.meetings.front();
  second.id = domain::MeetingId{"meeting-002"};
  second.groups = {domain::StudentGroupId{"group-02"}};
  second.teacher_requirements.front().fixed_teacher = domain::TeacherId{"teacher-002"};
  second.room_requirements.front().fixed_room = domain::RoomId{"room-002"};
  project.meetings.push_back(std::move(second));

  const SolveResult result = solve({.project = project});

  ASSERT_EQ(result.status, SolveStatus::kOptimal);
  ASSERT_TRUE(result.schedule.has_value());
  ASSERT_EQ(result.schedule->meetings.size(), 2U);
  EXPECT_EQ(result.schedule->meetings[0].start_slot, result.schedule->meetings[1].start_slot);

  domain::Schedule broken = *result.schedule;
  broken.meetings[1].start_slot = broken.meetings[0].start_slot == domain::SlotId{"slot-mon-p1"}
                                      ? domain::SlotId{"slot-mon-p2"}
                                      : domain::SlotId{"slot-mon-p1"};
  const validation::ValidationResult validation =
      validation::ScheduleValidator{}.validate(project, broken);
  EXPECT_TRUE(std::ranges::any_of(validation.diagnostics, [](const auto& diagnostic) {
    return diagnostic.code == "schedule.simultaneity_violation";
  }));
}

TEST(SolveTest, AllowsCoTeachersToShareOneIndependentRoomLane) {
  domain::Project project = test::make_tiny_project();
  project.teachers.push_back({.id = domain::TeacherId{"teacher-002"},
                              .display_name = "Teacher 2",
                              .qualified_subjects = {domain::SubjectId{"subject-math"}},
                              .maximum_weekly_load = 1});
  project.meetings.front().teacher_requirements.push_back(
      {.fixed_teacher = domain::TeacherId{"teacher-002"}, .lane = 1});
  project.meetings.front().resource_lanes_aligned = false;

  const SolveResult result = solve({.project = project});

  ASSERT_EQ(result.status, SolveStatus::kOptimal);
  ASSERT_TRUE(result.schedule.has_value());
  EXPECT_EQ(result.schedule->meetings.front().teachers.size(), 2U);
  EXPECT_EQ(result.schedule->meetings.front().rooms.size(), 1U);
}

TEST(SolveTest, SeparatesConflictingSubjectsAcrossDays) {
  domain::Project project = two_day_project();
  project.student_groups.front().allow_repeated_subjects_per_day = true;
  add_second_meeting(project);
  make_second_subject_conflicting(project);

  const SolveResult result = solve({.project = project});

  EXPECT_EQ(result.status, SolveStatus::kOptimal);
  ASSERT_TRUE(result.schedule.has_value());
  EXPECT_TRUE(result.diagnostics.ok());
}

TEST(SolveTest, ProvesConflictingSubjectsOnOneDayInfeasible) {
  domain::Project project = one_day_two_period_project();
  project.student_groups.front().allow_repeated_subjects_per_day = true;
  add_second_meeting(project);
  make_second_subject_conflicting(project);

  const SolveResult result = solve({.project = project});

  EXPECT_EQ(result.status, SolveStatus::kInfeasible);
}

}  // namespace
}  // namespace schedmesh::solver
