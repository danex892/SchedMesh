#include "schedmesh/validation/project_validator.h"

#include <algorithm>
#include <set>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace schedmesh::validation {
namespace {

using domain::Id;

void add_error(ValidationResult& result, std::string code, std::string path, std::string message,
               std::string entity_id, std::string action) {
  result.diagnostics.push_back({.code = std::move(code),
                                .severity = DiagnosticSeverity::kError,
                                .path = std::move(path),
                                .message = std::move(message),
                                .entity_id = std::move(entity_id),
                                .suggested_action = std::move(action)});
}

template <typename Entity, typename IdAccessor>
void validate_unique_ids(const std::vector<Entity>& entities, std::string_view path,
                         IdAccessor id_of, ValidationResult& result) {
  std::set<std::string> seen;
  for (std::size_t index = 0; index < entities.size(); ++index) {
    const std::string& id = id_of(entities[index]).value();
    const std::string item_path = std::string(path) + "/" + std::to_string(index) + "/id";
    if (id.empty()) {
      add_error(result, "project.empty_id", item_path, "Entity ID must not be empty.", {},
                "Assign a stable ID.");
    } else if (!seen.insert(id).second) {
      add_error(result, "project.duplicate_id", item_path, "Entity ID is duplicated.", id,
                "Assign a unique stable ID.");
    }
  }
}

template <typename Tag>
bool contains(const std::unordered_set<Id<Tag>>& ids, const Id<Tag>& id) {
  return ids.contains(id);
}

template <typename Value>
bool contains(const std::vector<Value>& values, const Value& value) {
  return std::ranges::find(values, value) != values.end();
}

template <typename Entity, typename EntityId>
const Entity* find_entity(const std::vector<Entity>& entities, const EntityId& id) {
  const auto found =
      std::ranges::find_if(entities, [&](const Entity& entity) { return entity.id == id; });
  return found == entities.end() ? nullptr : &*found;
}

bool has_required_features(const domain::Room& room,
                           const std::set<std::string>& required_features) {
  return std::ranges::all_of(required_features, [&](const std::string& feature) {
    return room.features.contains(feature);
  });
}

template <typename Tag>
std::unordered_set<Id<Tag>> collect_ids(const auto& entities) {
  std::unordered_set<Id<Tag>> ids;
  for (const auto& entity : entities) {
    ids.insert(entity.id);
  }
  return ids;
}

template <typename Tag>
void validate_references(const std::vector<Id<Tag>>& references,
                         const std::unordered_set<Id<Tag>>& known_ids, const std::string& path,
                         std::string_view entity_id, std::string_view kind,
                         ValidationResult& result) {
  for (std::size_t index = 0; index < references.size(); ++index) {
    if (!contains(known_ids, references[index])) {
      add_error(result, "project.unknown_reference", path + "/" + std::to_string(index),
                "Referenced " + std::string(kind) + " does not exist.", std::string(entity_id),
                "Use an ID declared in this project.");
    }
  }
}

template <typename Requirement>
void validate_lanes(const std::vector<Requirement>& requirements, const std::string& path,
                    std::string_view meeting_id, ValidationResult& result) {
  std::set<int> lanes;
  for (std::size_t index = 0; index < requirements.size(); ++index) {
    const auto& requirement = requirements[index];
    const std::string requirement_path = path + "/" + std::to_string(index);
    if (requirement.lane < 0) {
      add_error(result, "meeting.negative_lane", requirement_path + "/lane",
                "Lane number must be non-negative.", std::string(meeting_id),
                "Use a zero-based lane number.");
    } else if (!lanes.insert(requirement.lane).second) {
      add_error(result, "meeting.duplicate_lane", requirement_path + "/lane",
                "Lane number is duplicated within the requirement type.", std::string(meeting_id),
                "Assign a unique lane number.");
    }
  }
}

}  // namespace

bool ValidationResult::ok() const noexcept {
  return std::ranges::none_of(diagnostics, [](const Diagnostic& diagnostic) {
    return diagnostic.severity == DiagnosticSeverity::kError;
  });
}

ValidationResult ProjectValidator::validate(const domain::Project& project) const {
  ValidationResult result;
  if (project.schema_version != domain::kCurrentSchemaVersion) {
    add_error(result, "project.unsupported_schema", "/schema_version",
              "Project schema version is not supported.", project.metadata.id,
              "Migrate the project to schema version " +
                  std::to_string(domain::kCurrentSchemaVersion) + ".");
  }
  if (project.metadata.id.empty()) {
    add_error(result, "project.empty_metadata_id", "/metadata/id",
              "Project metadata ID must not be empty.", {}, "Assign a stable project ID.");
  }
  if (project.calendar.days.empty()) {
    add_error(result, "calendar.no_days", "/calendar/days", "Calendar must contain a day.",
              project.metadata.id, "Add at least one day.");
  }
  if (project.calendar.periods.empty()) {
    add_error(result, "calendar.no_periods", "/calendar/periods", "Calendar must contain a period.",
              project.metadata.id, "Add at least one period.");
  }

  validate_unique_ids(
      project.calendar.slots, "/calendar/slots",
      [](const domain::Slot& slot) -> const domain::SlotId& { return slot.id; }, result);
  validate_unique_ids(
      project.subjects, "/subjects",
      [](const domain::Subject& subject) -> const domain::SubjectId& { return subject.id; },
      result);
  validate_unique_ids(
      project.teachers, "/teachers",
      [](const domain::Teacher& teacher) -> const domain::TeacherId& { return teacher.id; },
      result);
  validate_unique_ids(
      project.student_groups, "/student_groups",
      [](const domain::StudentGroup& group) -> const domain::StudentGroupId& { return group.id; },
      result);
  validate_unique_ids(
      project.rooms, "/rooms",
      [](const domain::Room& room) -> const domain::RoomId& { return room.id; }, result);
  validate_unique_ids(
      project.meetings, "/meetings",
      [](const domain::Meeting& meeting) -> const domain::MeetingId& { return meeting.id; },
      result);

  const auto slot_ids = collect_ids<domain::SlotTag>(project.calendar.slots);
  const auto subject_ids = collect_ids<domain::SubjectTag>(project.subjects);
  const auto teacher_ids = collect_ids<domain::TeacherTag>(project.teachers);
  const auto group_ids = collect_ids<domain::StudentGroupTag>(project.student_groups);
  const auto room_ids = collect_ids<domain::RoomTag>(project.rooms);

  for (std::size_t index = 0; index < project.calendar.slots.size(); ++index) {
    const auto& slot = project.calendar.slots[index];
    if (slot.day_index >= project.calendar.days.size() ||
        slot.period_index >= project.calendar.periods.size()) {
      add_error(result, "calendar.slot_out_of_range", "/calendar/slots/" + std::to_string(index),
                "Slot references a day or period outside the calendar.", slot.id.value(),
                "Regenerate slots from the calendar axes.");
    }
  }

  for (std::size_t index = 0; index < project.subjects.size(); ++index) {
    const domain::Subject& subject = project.subjects[index];
    const std::string path = "/subjects/" + std::to_string(index);
    validate_references(subject.conflicting_subjects, subject_ids, path + "/conflicting_subjects",
                        subject.id.value(), "subject", result);
    if (subject.required_consecutive_periods <= 0) {
      add_error(result, "subject.invalid_consecutive_periods",
                path + "/required_consecutive_periods",
                "Required consecutive period count must be positive.", subject.id.value(),
                "Use a value of at least one period.");
    }
    for (std::size_t conflict_index = 0; conflict_index < subject.conflicting_subjects.size();
         ++conflict_index) {
      const domain::SubjectId& conflicting_id = subject.conflicting_subjects[conflict_index];
      const domain::Subject* conflicting = find_entity(project.subjects, conflicting_id);
      const std::string conflict_path =
          path + "/conflicting_subjects/" + std::to_string(conflict_index);
      if (conflicting_id == subject.id) {
        add_error(result, "subject.self_conflict", conflict_path,
                  "Subject cannot conflict with itself.", subject.id.value(),
                  "Remove the self-reference.");
      } else if (conflicting != nullptr &&
                 !contains(conflicting->conflicting_subjects, subject.id)) {
        add_error(result, "subject.asymmetric_conflict", conflict_path,
                  "Subject conflict must be declared symmetrically.", subject.id.value(),
                  "Add the reverse conflict reference.");
      }
    }
  }

  for (std::size_t index = 0; index < project.teachers.size(); ++index) {
    const auto& teacher = project.teachers[index];
    const std::string path = "/teachers/" + std::to_string(index);
    validate_references(teacher.qualified_subjects, subject_ids, path + "/qualified_subjects",
                        teacher.id.value(), "subject", result);
    validate_references(teacher.unavailable_slots, slot_ids, path + "/unavailable_slots",
                        teacher.id.value(), "slot", result);
    if (teacher.maximum_weekly_load < 0 ||
        (teacher.maximum_daily_load && *teacher.maximum_daily_load < 0)) {
      add_error(result, "teacher.negative_load", path, "Teacher load limits cannot be negative.",
                teacher.id.value(), "Use non-negative load limits.");
    }
  }

  for (std::size_t index = 0; index < project.student_groups.size(); ++index) {
    const auto& group = project.student_groups[index];
    validate_references(group.allowed_slots, slot_ids,
                        "/student_groups/" + std::to_string(index) + "/allowed_slots",
                        group.id.value(), "slot", result);
  }

  for (std::size_t index = 0; index < project.rooms.size(); ++index) {
    const auto& room = project.rooms[index];
    validate_references(room.unavailable_slots, slot_ids,
                        "/rooms/" + std::to_string(index) + "/unavailable_slots", room.id.value(),
                        "slot", result);
    if (room.capacity < 0) {
      add_error(result, "room.negative_capacity", "/rooms/" + std::to_string(index) + "/capacity",
                "Room capacity cannot be negative.", room.id.value(),
                "Use a non-negative capacity.");
    }
  }

  for (std::size_t index = 0; index < project.meetings.size(); ++index) {
    const auto& meeting = project.meetings[index];
    const std::string path = "/meetings/" + std::to_string(index);
    if (!contains(subject_ids, meeting.subject)) {
      add_error(result, "project.unknown_reference", path + "/subject",
                "Referenced subject does not exist.", meeting.id.value(),
                "Use a subject declared in this project.");
    }
    const domain::Subject* meeting_subject = find_entity(project.subjects, meeting.subject);
    if (meeting_subject != nullptr &&
        meeting.duration_in_periods != meeting_subject->required_consecutive_periods) {
      add_error(result, "meeting.subject_duration_mismatch", path + "/duration_in_periods",
                "Meeting duration does not match the subject's consecutive-period requirement.",
                meeting.id.value(), "Use the duration declared by the meeting subject.");
    }
    validate_references(meeting.groups, group_ids, path + "/groups", meeting.id.value(),
                        "student group", result);
    validate_references(meeting.allowed_start_slots, slot_ids, path + "/allowed_start_slots",
                        meeting.id.value(), "slot", result);
    if (meeting.groups.empty()) {
      add_error(result, "meeting.no_groups", path + "/groups",
                "Meeting must contain at least one student group.", meeting.id.value(),
                "Assign a student group.");
    }
    if (meeting.allowed_start_slots.empty()) {
      add_error(result, "meeting.no_allowed_slots", path + "/allowed_start_slots",
                "Meeting must have at least one allowed start slot.", meeting.id.value(),
                "Add an allowed start slot.");
    }
    if (meeting.duration_in_periods <= 0) {
      add_error(result, "meeting.invalid_duration", path + "/duration_in_periods",
                "Meeting duration must be positive.", meeting.id.value(),
                "Use a duration of at least one period.");
    }

    validate_lanes(meeting.teacher_requirements, path + "/teacher_requirements", meeting.id.value(),
                   result);
    validate_lanes(meeting.room_requirements, path + "/room_requirements", meeting.id.value(),
                   result);

    for (std::size_t requirement_index = 0; requirement_index < meeting.teacher_requirements.size();
         ++requirement_index) {
      const auto& requirement = meeting.teacher_requirements[requirement_index];
      const std::string requirement_path =
          path + "/teacher_requirements/" + std::to_string(requirement_index);
      if (requirement.fixed_teacher && !requirement.candidates.empty()) {
        add_error(result, "meeting.ambiguous_teacher_requirement", requirement_path,
                  "Teacher requirement cannot be both fixed and candidate-based.",
                  meeting.id.value(), "Keep either fixed_teacher or candidates.");
      }
      if (!requirement.fixed_teacher && requirement.candidates.empty()) {
        add_error(result, "meeting.empty_teacher_requirement", requirement_path,
                  "Teacher requirement must define a fixed teacher or candidates.",
                  meeting.id.value(), "Assign a fixed teacher or candidate list.");
      }
      if (requirement.fixed_teacher && !contains(teacher_ids, *requirement.fixed_teacher)) {
        add_error(result, "project.unknown_reference", requirement_path + "/fixed_teacher",
                  "Referenced teacher does not exist.", meeting.id.value(),
                  "Use a teacher declared in this project.");
      }
      validate_references(requirement.candidates, teacher_ids, requirement_path + "/candidates",
                          meeting.id.value(), "teacher", result);
      const auto validate_qualification = [&](const domain::TeacherId& teacher_id,
                                              const std::string& teacher_path) {
        const domain::Teacher* teacher = find_entity(project.teachers, teacher_id);
        if (teacher != nullptr && !contains(teacher->qualified_subjects, meeting.subject)) {
          add_error(result, "meeting.teacher_not_qualified", teacher_path,
                    "Teacher is not qualified for the meeting subject.", meeting.id.value(),
                    "Add the subject to the teacher qualifications or choose another teacher.");
        }
      };
      if (requirement.fixed_teacher) {
        validate_qualification(*requirement.fixed_teacher, requirement_path + "/fixed_teacher");
      }
      for (std::size_t candidate_index = 0; candidate_index < requirement.candidates.size();
           ++candidate_index) {
        validate_qualification(requirement.candidates[candidate_index],
                               requirement_path + "/candidates/" + std::to_string(candidate_index));
      }
    }

    for (std::size_t requirement_index = 0; requirement_index < meeting.room_requirements.size();
         ++requirement_index) {
      const auto& requirement = meeting.room_requirements[requirement_index];
      const std::string requirement_path =
          path + "/room_requirements/" + std::to_string(requirement_index);
      if (requirement.fixed_room && !requirement.candidates.empty()) {
        add_error(result, "meeting.ambiguous_room_requirement", requirement_path,
                  "Room requirement cannot be both fixed and candidate-based.", meeting.id.value(),
                  "Keep either fixed_room or candidates.");
      }
      if (!requirement.fixed_room && requirement.candidates.empty()) {
        add_error(result, "meeting.empty_room_requirement", requirement_path,
                  "Room requirement must define a fixed room or candidates.", meeting.id.value(),
                  "Assign a fixed room or candidate list.");
      }
      if (requirement.minimum_capacity < 0) {
        add_error(result, "meeting.negative_room_capacity", requirement_path + "/minimum_capacity",
                  "Minimum room capacity cannot be negative.", meeting.id.value(),
                  "Use zero for unspecified capacity or a positive minimum.");
      }
      if (requirement.fixed_room && !contains(room_ids, *requirement.fixed_room)) {
        add_error(result, "project.unknown_reference", requirement_path + "/fixed_room",
                  "Referenced room does not exist.", meeting.id.value(),
                  "Use a room declared in this project.");
      }
      validate_references(requirement.candidates, room_ids, requirement_path + "/candidates",
                          meeting.id.value(), "room", result);
    }

    const bool has_feasible_slot =
        std::ranges::any_of(meeting.allowed_start_slots, [&](const domain::SlotId& slot_id) {
          if (!contains(slot_ids, slot_id)) {
            return false;
          }
          const domain::Slot* slot = find_entity(project.calendar.slots, slot_id);
          if (slot == nullptr || meeting.duration_in_periods <= 0) {
            return false;
          }
          std::vector<domain::SlotId> occupied_slots;
          occupied_slots.reserve(static_cast<std::size_t>(meeting.duration_in_periods));
          for (int offset = 0; offset < meeting.duration_in_periods; ++offset) {
            const std::size_t period_index = slot->period_index + static_cast<std::size_t>(offset);
            const auto occupied =
                std::ranges::find_if(project.calendar.slots, [&](const domain::Slot& item) {
                  return item.day_index == slot->day_index && item.period_index == period_index;
                });
            if (occupied == project.calendar.slots.end()) {
              return false;
            }
            occupied_slots.push_back(occupied->id);
          }
          const bool subject_boundary_allowed =
              meeting_subject == nullptr ||
              std::ranges::all_of(meeting.groups, [&](const domain::StudentGroupId& group_id) {
                const domain::StudentGroup* group = find_entity(project.student_groups, group_id);
                if (group == nullptr || slot == nullptr) {
                  return false;
                }
                std::vector<std::size_t> day_periods;
                for (const domain::SlotId& group_slot_id : group->allowed_slots) {
                  const domain::Slot* group_slot =
                      find_entity(project.calendar.slots, group_slot_id);
                  if (group_slot != nullptr && group_slot->day_index == slot->day_index) {
                    day_periods.push_back(group_slot->period_index);
                  }
                }
                if (day_periods.empty()) {
                  return false;
                }
                const auto [first, last] = std::ranges::minmax_element(day_periods);
                return (!meeting_subject->forbid_first_period || slot->period_index != *first) &&
                       (!meeting_subject->forbid_last_period || slot->period_index != *last);
              });
          const bool groups_available =
              std::ranges::all_of(meeting.groups, [&](const domain::StudentGroupId& group_id) {
                const domain::StudentGroup* group = find_entity(project.student_groups, group_id);
                return group != nullptr &&
                       std::ranges::all_of(occupied_slots, [&](const domain::SlotId& occupied) {
                         return contains(group->allowed_slots, occupied);
                       });
              });
          const bool teachers_available = std::ranges::all_of(
              meeting.teacher_requirements, [&](const domain::TeacherRequirement& requirement) {
                const auto teacher_available = [&](const domain::TeacherId& teacher_id) {
                  const domain::Teacher* teacher = find_entity(project.teachers, teacher_id);
                  return teacher != nullptr &&
                         contains(teacher->qualified_subjects, meeting.subject) &&
                         std::ranges::none_of(occupied_slots, [&](const domain::SlotId& occupied) {
                           return contains(teacher->unavailable_slots, occupied);
                         });
                };
                if (requirement.fixed_teacher) {
                  return teacher_available(*requirement.fixed_teacher);
                }
                return std::ranges::any_of(requirement.candidates, teacher_available);
              });
          const bool rooms_available = std::ranges::all_of(
              meeting.room_requirements, [&](const domain::RoomRequirement& requirement) {
                const auto room_available = [&](const domain::RoomId& room_id) {
                  const domain::Room* room = find_entity(project.rooms, room_id);
                  return room != nullptr &&
                         (requirement.minimum_capacity <= 0 ||
                          room->capacity >= requirement.minimum_capacity) &&
                         std::ranges::none_of(occupied_slots,
                                              [&](const domain::SlotId& occupied) {
                                                return contains(room->unavailable_slots, occupied);
                                              }) &&
                         has_required_features(*room, requirement.required_features);
                };
                if (requirement.fixed_room) {
                  return room_available(*requirement.fixed_room);
                }
                return std::ranges::any_of(requirement.candidates, room_available);
              });
          return subject_boundary_allowed && groups_available && teachers_available &&
                 rooms_available;
        });
    if (!meeting.allowed_start_slots.empty() && !has_feasible_slot) {
      add_error(result, "meeting.no_feasible_start_slot", path + "/allowed_start_slots",
                "No allowed start slot satisfies group, teacher, and room availability.",
                meeting.id.value(),
                "Expand the meeting domain or relax the conflicting resource availability.");
    }
  }

  return result;
}

}  // namespace schedmesh::validation
