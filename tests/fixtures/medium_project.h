#pragma once

#include <string>
#include <utility>
#include <vector>

#include "schedmesh/domain/project.h"

namespace schedmesh::test {

inline domain::Project make_medium_project() {
  using namespace domain;
  constexpr int kTeacherLoad = 8;

  Project project{
      .metadata = {.id = "project-medium", .display_name = "Public medium room fixture"},
      .calendar = make_calendar({{.id = "mon", .display_name = "Monday", .ordinal = 0},
                                 {.id = "tue", .display_name = "Tuesday", .ordinal = 1},
                                 {.id = "wed", .display_name = "Wednesday", .ordinal = 2}},
                                {{.id = "p1", .ordinal = 0},
                                 {.id = "p2", .ordinal = 1},
                                 {.id = "p3", .ordinal = 2},
                                 {.id = "p4", .ordinal = 3}}),
      .subjects = {{.id = SubjectId{"subject-math"}, .display_name = "Math"},
                   {.id = SubjectId{"subject-language"}, .display_name = "Language"},
                   {.id = SubjectId{"subject-science"}, .display_name = "Science"},
                   {.id = SubjectId{"subject-sports"}, .display_name = "Sports"},
                   {.id = SubjectId{"subject-technology"},
                    .display_name = "Technology",
                    .required_consecutive_periods = 2}},
      .teachers = {{.id = TeacherId{"teacher-math"},
                    .display_name = "Math teacher",
                    .qualified_subjects = {SubjectId{"subject-math"}},
                    .maximum_weekly_load = kTeacherLoad},
                   {.id = TeacherId{"teacher-language-a"},
                    .display_name = "Language teacher A",
                    .qualified_subjects = {SubjectId{"subject-language"}},
                    .maximum_weekly_load = kTeacherLoad},
                   {.id = TeacherId{"teacher-language-b"},
                    .display_name = "Language teacher B",
                    .qualified_subjects = {SubjectId{"subject-language"}},
                    .maximum_weekly_load = kTeacherLoad},
                   {.id = TeacherId{"teacher-science"},
                    .display_name = "Science teacher",
                    .qualified_subjects = {SubjectId{"subject-science"}},
                    .maximum_weekly_load = kTeacherLoad},
                   {.id = TeacherId{"teacher-sports"},
                    .display_name = "Sports teacher",
                    .qualified_subjects = {SubjectId{"subject-sports"}},
                    .maximum_weekly_load = kTeacherLoad},
                   {.id = TeacherId{"teacher-sports-b"},
                    .display_name = "Sports teacher B",
                    .qualified_subjects = {SubjectId{"subject-sports"}},
                    .maximum_weekly_load = kTeacherLoad},
                   {.id = TeacherId{"teacher-technology"},
                    .display_name = "Technology teacher",
                    .qualified_subjects = {SubjectId{"subject-technology"}},
                    .maximum_weekly_load = kTeacherLoad}},
      .rooms = {{.id = RoomId{"room-small"}, .display_name = "Small room", .capacity = 20},
                {.id = RoomId{"room-large"}, .display_name = "Large room", .capacity = 35},
                {.id = RoomId{"room-language-a"},
                 .display_name = "Language room A",
                 .capacity = 18,
                 .features = {"language"}},
                {.id = RoomId{"room-language-b"},
                 .display_name = "Language room B",
                 .capacity = 18,
                 .features = {"language"}},
                {.id = RoomId{"room-lab"},
                 .display_name = "Laboratory",
                 .capacity = 30,
                 .features = {"laboratory"}},
                {.id = RoomId{"room-gym-a"}, .display_name = "Gym lane A", .features = {"gym"}},
                {.id = RoomId{"room-gym-b"}, .display_name = "Gym lane B", .features = {"gym"}},
                {.id = RoomId{"room-technology"},
                 .display_name = "Technology workshop",
                 .capacity = 24,
                 .features = {"technology"}}}};

  std::vector<SlotId> all_slots;
  for (const Slot& slot : project.calendar.slots) {
    all_slots.push_back(slot.id);
  }
  project.student_groups = {{.id = StudentGroupId{"group-07"},
                             .display_name = "Grade 7",
                             .grade = 7,
                             .allowed_slots = all_slots},
                            {.id = StudentGroupId{"group-08"},
                             .display_name = "Grade 8",
                             .grade = 8,
                             .allowed_slots = all_slots}};

  const auto meeting = [&](std::string id, SubjectId subject, StudentGroupId group,
                           TeacherId teacher, RoomRequirement room, int duration = 1) {
    return Meeting{.id = MeetingId{std::move(id)},
                   .subject = std::move(subject),
                   .groups = {std::move(group)},
                   .teacher_requirements = {{.fixed_teacher = std::move(teacher), .lane = 0}},
                   .room_requirements = {std::move(room)},
                   .allowed_start_slots = all_slots,
                   .duration_in_periods = duration,
                   .distribution_key = "medium"};
  };
  project.meetings = {meeting("meeting-math-07", SubjectId{"subject-math"},
                              StudentGroupId{"group-07"}, TeacherId{"teacher-math"},
                              {.candidates = {RoomId{"room-small"}, RoomId{"room-large"}},
                               .minimum_capacity = 30,
                               .lane = 0}),
                      meeting("meeting-math-08", SubjectId{"subject-math"},
                              StudentGroupId{"group-08"}, TeacherId{"teacher-math"},
                              {.candidates = {RoomId{"room-small"}, RoomId{"room-large"}},
                               .minimum_capacity = 15,
                               .lane = 0}),
                      meeting("meeting-science-07", SubjectId{"subject-science"},
                              StudentGroupId{"group-07"}, TeacherId{"teacher-science"},
                              {.fixed_room = RoomId{"room-lab"},
                               .required_features = {"laboratory"},
                               .minimum_capacity = 25,
                               .lane = 0}),
                      meeting("meeting-sports-07", SubjectId{"subject-sports"},
                              StudentGroupId{"group-07"}, TeacherId{"teacher-sports"},
                              {.candidates = {RoomId{"room-gym-a"}, RoomId{"room-gym-b"}},
                               .required_features = {"gym"},
                               .lane = 0}),
                      meeting("meeting-sports-08", SubjectId{"subject-sports"},
                              StudentGroupId{"group-08"}, TeacherId{"teacher-sports-b"},
                              {.candidates = {RoomId{"room-gym-a"}, RoomId{"room-gym-b"}},
                               .required_features = {"gym"},
                               .lane = 0}),
                      meeting("meeting-technology-08", SubjectId{"subject-technology"},
                              StudentGroupId{"group-08"}, TeacherId{"teacher-technology"},
                              {.fixed_room = RoomId{"room-technology"},
                               .required_features = {"technology"},
                               .minimum_capacity = 20,
                               .lane = 0},
                              2)};

  Meeting language = meeting("meeting-language-07", SubjectId{"subject-language"},
                             StudentGroupId{"group-07"}, TeacherId{"teacher-language-a"},
                             {.candidates = {RoomId{"room-language-a"}, RoomId{"room-language-b"}},
                              .required_features = {"language"},
                              .lane = 0});
  language.teacher_requirements.push_back(
      {.fixed_teacher = TeacherId{"teacher-language-b"}, .lane = 1});
  language.room_requirements.push_back(
      {.candidates = {RoomId{"room-language-a"}, RoomId{"room-language-b"}},
       .required_features = {"language"},
       .lane = 1});
  project.meetings.push_back(std::move(language));
  return project;
}

}  // namespace schedmesh::test
