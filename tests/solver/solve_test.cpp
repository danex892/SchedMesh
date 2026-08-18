#include "schedmesh/solver/solve.h"

#include <gtest/gtest.h>

#include "fixtures/tiny_project.h"

namespace schedmesh::solver {
namespace {

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

TEST(SolveTest, RejectsInvalidProjectBeforeCallingCpSat) {
  domain::Project project = test::make_tiny_project();
  project.meetings.front().allowed_start_slots.clear();

  const SolveResult result = solve({.project = project});

  EXPECT_EQ(result.status, SolveStatus::kInvalidProject);
  EXPECT_FALSE(result.schedule.has_value());
  EXPECT_FALSE(result.diagnostics.ok());
}

}  // namespace
}  // namespace schedmesh::solver
