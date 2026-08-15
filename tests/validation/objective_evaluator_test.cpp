#include "schedmesh/validation/objective_evaluator.h"

#include <gtest/gtest.h>

#include "fixtures/tiny_project.h"

namespace schedmesh::validation {
namespace {

TEST(ObjectiveEvaluatorTest, ReportsIndependentRawPenaltyComponents) {
  domain::Project project = test::make_tiny_project();
  project.calendar = domain::make_calendar(
      {{.id = "mon", .display_name = "Day 1", .ordinal = 0}},
      {{.id = "p1", .ordinal = 0}, {.id = "p2", .ordinal = 1}, {.id = "p3", .ordinal = 2}});
  project.student_groups.front().allowed_slots.clear();
  for (const domain::Slot& slot : project.calendar.slots) {
    project.student_groups.front().allowed_slots.push_back(slot.id);
  }
  project.meetings.front().allowed_start_slots = project.student_groups.front().allowed_slots;
  domain::Meeting second = project.meetings.front();
  second.id = domain::MeetingId{"meeting-002"};
  project.meetings.push_back(second);
  project.preferences.minimize_last_day_load = true;
  const domain::Schedule schedule{.meetings = {{.meeting = domain::MeetingId{"meeting-001"},
                                                .start_slot = project.calendar.slots[0].id,
                                                .teachers = {domain::TeacherId{"teacher-001"}},
                                                .rooms = {domain::RoomId{"room-001"}}},
                                               {.meeting = domain::MeetingId{"meeting-002"},
                                                .start_slot = project.calendar.slots[2].id,
                                                .teachers = {domain::TeacherId{"teacher-001"}},
                                                .rooms = {domain::RoomId{"room-001"}}}}};

  const ObjectiveBreakdown result = ObjectiveEvaluator{}.evaluate(project, schedule);

  EXPECT_EQ(result.teacher_idle_periods, 1);
  EXPECT_EQ(result.group_idle_periods, 1);
  EXPECT_EQ(result.late_period_load, 2);
  EXPECT_EQ(result.last_day_load, 2);
  EXPECT_EQ(result.total(), 6);
}

}  // namespace
}  // namespace schedmesh::validation
