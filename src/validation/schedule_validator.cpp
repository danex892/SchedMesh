#include "schedmesh/validation/schedule_validator.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "schedmesh/validation/project_validator.h"

namespace schedmesh::validation {
namespace {

void add_error(ValidationResult& result, std::string code, std::string path, std::string message,
               std::string entity_id, std::string action) {
  result.diagnostics.push_back({.code = std::move(code),
                                .severity = DiagnosticSeverity::kError,
                                .path = std::move(path),
                                .message = std::move(message),
                                .entity_id = std::move(entity_id),
                                .suggested_action = std::move(action)});
}

template <typename Entity, typename EntityId>
const Entity* find_entity(const std::vector<Entity>& entities, const EntityId& id) {
  const auto found =
      std::ranges::find_if(entities, [&](const Entity& entity) { return entity.id == id; });
  return found == entities.end() ? nullptr : &*found;
}

template <typename Value>
bool contains(const std::vector<Value>& values, const Value& value) {
  return std::ranges::find(values, value) != values.end();
}

std::vector<const domain::Slot*> occupied_slots(const domain::Project& project,
                                                const domain::Meeting& meeting,
                                                const domain::Slot& start) {
  std::vector<const domain::Slot*> result;
  result.reserve(static_cast<std::size_t>(meeting.duration_in_periods));
  for (int offset = 0; offset < meeting.duration_in_periods; ++offset) {
    const std::size_t period = start.period_index + static_cast<std::size_t>(offset);
    const auto slot = std::ranges::find_if(project.calendar.slots, [&](const domain::Slot& item) {
      return item.day_index == start.day_index && item.period_index == period;
    });
    if (slot == project.calendar.slots.end()) {
      return {};
    }
    result.push_back(&*slot);
  }
  return result;
}

std::string occupancy_key(std::string_view resource_id, const domain::SlotId& slot_id) {
  std::string result{resource_id};
  result.push_back('\n');
  result.append(slot_id.value());
  return result;
}

}  // namespace

ValidationResult ScheduleValidator::validate(const domain::Project& project,
                                             const domain::Schedule& schedule) const {
  ValidationResult result = ProjectValidator{}.validate(project);
  if (!result.ok()) {
    return result;
  }

  std::set<domain::MeetingId> assigned_meetings;
  std::set<std::string> group_occupancy;
  std::set<std::string> teacher_occupancy;
  std::set<std::string> room_occupancy;
  std::map<domain::TeacherId, int> weekly_teacher_load;
  std::map<std::pair<domain::TeacherId, std::size_t>, int> daily_teacher_load;
  std::map<std::pair<domain::StudentGroupId, std::size_t>, std::vector<domain::SubjectId>>
      group_day_subjects;

  for (std::size_t index = 0; index < schedule.meetings.size(); ++index) {
    const domain::ScheduledMeeting& assignment = schedule.meetings[index];
    const std::string path = "/meetings/" + std::to_string(index);
    const domain::Meeting* meeting = find_entity(project.meetings, assignment.meeting);
    if (meeting == nullptr) {
      add_error(result, "schedule.unknown_meeting", path + "/meeting",
                "Schedule references a meeting absent from the project.",
                assignment.meeting.value(), "Use a meeting declared in the project.");
      continue;
    }
    if (!assigned_meetings.insert(assignment.meeting).second) {
      add_error(result, "schedule.duplicate_meeting", path + "/meeting",
                "Meeting is assigned more than once.", assignment.meeting.value(),
                "Keep exactly one assignment for each meeting.");
      continue;
    }
    const domain::Slot* start = find_entity(project.calendar.slots, assignment.start_slot);
    if (start == nullptr || !contains(meeting->allowed_start_slots, assignment.start_slot)) {
      add_error(result, "schedule.disallowed_start_slot", path + "/start_slot",
                "Meeting starts outside its allowed slot domain.", assignment.meeting.value(),
                "Choose one of the meeting's allowed start slots.");
      continue;
    }
    const std::vector<const domain::Slot*> occupied = occupied_slots(project, *meeting, *start);
    if (occupied.size() != static_cast<std::size_t>(meeting->duration_in_periods)) {
      add_error(result, "schedule.duration_out_of_calendar", path + "/start_slot",
                "Meeting duration extends beyond the calendar.", assignment.meeting.value(),
                "Choose a start slot that fits the full meeting duration.");
      continue;
    }

    if (assignment.teachers.size() != meeting->teacher_requirements.size()) {
      add_error(result, "schedule.teacher_lane_count", path + "/teachers",
                "Assigned teacher count does not match meeting lanes.", assignment.meeting.value(),
                "Assign exactly one teacher per teacher requirement.");
    }
    if (assignment.rooms.size() != meeting->room_requirements.size()) {
      add_error(result, "schedule.room_lane_count", path + "/rooms",
                "Assigned room count does not match meeting lanes.", assignment.meeting.value(),
                "Assign exactly one room per room requirement.");
    }

    for (const domain::StudentGroupId& group_id : meeting->groups) {
      const domain::StudentGroup* group = find_entity(project.student_groups, group_id);
      for (const domain::Slot* slot : occupied) {
        if (group == nullptr || !contains(group->allowed_slots, slot->id)) {
          add_error(result, "schedule.group_unavailable", path + "/start_slot",
                    "Student group is unavailable during the meeting.", group_id.value(),
                    "Move the meeting into the group's allowed slot domain.");
        }
        if (!group_occupancy.insert(occupancy_key(group_id.value(), slot->id)).second) {
          add_error(result, "schedule.group_overlap", path + "/start_slot",
                    "Student group has overlapping meetings.", group_id.value(),
                    "Move one of the overlapping meetings.");
        }
      }
      auto& subjects = group_day_subjects[{group_id, start->day_index}];
      const domain::Subject* subject = find_entity(project.subjects, meeting->subject);
      if (group != nullptr && subject != nullptr) {
        if (!group->allow_repeated_subjects_per_day && contains(subjects, subject->id)) {
          add_error(result, "schedule.repeated_subject_on_day", path + "/start_slot",
                    "Student group repeats a subject on the same day.", group_id.value(),
                    "Move one occurrence to another day or enable the group policy.");
        }
        if (std::ranges::any_of(subjects, [&](const domain::SubjectId& scheduled_subject) {
              return contains(subject->conflicting_subjects, scheduled_subject);
            })) {
          add_error(result, "schedule.subject_day_conflict", path + "/start_slot",
                    "Conflicting subjects are scheduled for the group on the same day.",
                    group_id.value(), "Move one conflicting subject to another day.");
        }
        subjects.push_back(subject->id);
      }
    }

    const std::size_t teacher_lanes =
        std::min(assignment.teachers.size(), meeting->teacher_requirements.size());
    for (std::size_t lane = 0; lane < teacher_lanes; ++lane) {
      const domain::TeacherId& teacher_id = assignment.teachers[lane];
      const domain::TeacherRequirement& requirement = meeting->teacher_requirements[lane];
      const domain::Teacher* teacher = find_entity(project.teachers, teacher_id);
      const bool eligible =
          requirement.fixed_teacher == teacher_id || contains(requirement.candidates, teacher_id);
      if (teacher == nullptr || !eligible) {
        add_error(result, "schedule.ineligible_teacher", path + "/teachers/" + std::to_string(lane),
                  "Assigned teacher does not satisfy the meeting lane.", assignment.meeting.value(),
                  "Choose the fixed teacher or an eligible candidate.");
        continue;
      }
      for (const domain::Slot* slot : occupied) {
        if (contains(teacher->unavailable_slots, slot->id)) {
          add_error(result, "schedule.teacher_unavailable", path + "/start_slot",
                    "Teacher is unavailable during the meeting.", teacher_id.value(),
                    "Move the meeting or assign another eligible teacher.");
        }
        if (!teacher_occupancy.insert(occupancy_key(teacher_id.value(), slot->id)).second) {
          add_error(result, "schedule.teacher_overlap", path + "/start_slot",
                    "Teacher has overlapping meetings.", teacher_id.value(),
                    "Move one meeting or assign another teacher.");
        }
        ++weekly_teacher_load[teacher_id];
        ++daily_teacher_load[{teacher_id, slot->day_index}];
      }
    }

    const std::size_t room_lanes =
        std::min(assignment.rooms.size(), meeting->room_requirements.size());
    for (std::size_t lane = 0; lane < room_lanes; ++lane) {
      const domain::RoomId& room_id = assignment.rooms[lane];
      const domain::RoomRequirement& requirement = meeting->room_requirements[lane];
      const domain::Room* room = find_entity(project.rooms, room_id);
      const bool eligible =
          requirement.fixed_room == room_id || contains(requirement.candidates, room_id);
      if (room == nullptr || !eligible) {
        add_error(result, "schedule.ineligible_room", path + "/rooms/" + std::to_string(lane),
                  "Assigned room does not satisfy the meeting lane.", assignment.meeting.value(),
                  "Choose the fixed room or an eligible candidate.");
        continue;
      }
      if (!std::ranges::all_of(requirement.required_features, [&](const std::string& feature) {
            return room->features.contains(feature);
          })) {
        add_error(result, "schedule.room_missing_feature", path + "/rooms/" + std::to_string(lane),
                  "Assigned room lacks a required feature.", room_id.value(),
                  "Choose a room with every required feature.");
      }
      for (const domain::Slot* slot : occupied) {
        if (contains(room->unavailable_slots, slot->id)) {
          add_error(result, "schedule.room_unavailable", path + "/start_slot",
                    "Room is unavailable during the meeting.", room_id.value(),
                    "Move the meeting or assign another eligible room.");
        }
        if (!room_occupancy.insert(occupancy_key(room_id.value(), slot->id)).second) {
          add_error(result, "schedule.room_overlap", path + "/start_slot",
                    "Room has overlapping meetings.", room_id.value(),
                    "Move one meeting or assign another room.");
        }
      }
    }
  }

  for (const domain::Meeting& meeting : project.meetings) {
    if (!assigned_meetings.contains(meeting.id)) {
      add_error(result, "schedule.missing_meeting", "/meetings", "Meeting is not scheduled.",
                meeting.id.value(), "Add exactly one assignment for the meeting.");
    }
  }
  for (const domain::Teacher& teacher : project.teachers) {
    if (weekly_teacher_load[teacher.id] > teacher.maximum_weekly_load) {
      add_error(result, "schedule.teacher_weekly_load", "/meetings",
                "Teacher exceeds the maximum weekly load.", teacher.id.value(),
                "Reduce or reassign the teacher's meetings.");
    }
    if (teacher.maximum_daily_load) {
      for (std::size_t day = 0; day < project.calendar.days.size(); ++day) {
        if (daily_teacher_load[{teacher.id, day}] > *teacher.maximum_daily_load) {
          add_error(result, "schedule.teacher_daily_load", "/meetings",
                    "Teacher exceeds the maximum daily load.", teacher.id.value(),
                    "Move or reassign meetings on that day.");
        }
      }
    }
  }
  return result;
}

}  // namespace schedmesh::validation
