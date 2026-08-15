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
      if (requirement.fixed_room && !contains(room_ids, *requirement.fixed_room)) {
        add_error(result, "project.unknown_reference", requirement_path + "/fixed_room",
                  "Referenced room does not exist.", meeting.id.value(),
                  "Use a room declared in this project.");
      }
      validate_references(requirement.candidates, room_ids, requirement_path + "/candidates",
                          meeting.id.value(), "room", result);
    }
  }

  return result;
}

}  // namespace schedmesh::validation
