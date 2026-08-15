#pragma once

#include "schedmesh/domain/project.h"

namespace schedmesh::test {

inline domain::Project make_tiny_project() {
  using namespace domain;

  const SlotId slot_id{"slot-mon-p1"};
  return Project{.metadata = {.id = "project-tiny", .display_name = "Tiny fixture"},
                 .calendar = make_calendar({{.id = "mon", .display_name = "Day 1", .ordinal = 0}},
                                           {{.id = "p1", .ordinal = 0}}),
                 .subjects = {{.id = SubjectId{"subject-math"}, .display_name = "Subject 1"}},
                 .teachers = {{.id = TeacherId{"teacher-001"},
                               .display_name = "Teacher 1",
                               .qualified_subjects = {SubjectId{"subject-math"}},
                               .maximum_weekly_load = 20}},
                 .student_groups = {{.id = StudentGroupId{"group-01"},
                                     .display_name = "Group 1",
                                     .grade = 5,
                                     .allowed_slots = {slot_id}}},
                 .rooms = {{.id = RoomId{"room-001"}, .display_name = "Room 1", .capacity = 30}},
                 .meetings = {{.id = MeetingId{"meeting-001"},
                               .subject = SubjectId{"subject-math"},
                               .groups = {StudentGroupId{"group-01"}},
                               .teacher_requirements = {{.fixed_teacher = TeacherId{"teacher-001"},
                                                         .lane = 0}},
                               .room_requirements = {{.fixed_room = RoomId{"room-001"}, .lane = 0}},
                               .allowed_start_slots = {slot_id},
                               .distribution_key = "math-group-01"}}};
}

}  // namespace schedmesh::test
