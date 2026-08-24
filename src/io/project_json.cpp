#include "schedmesh/io/project_json.h"

#include <chrono>
#include <cstdint>
#include <iterator>
#include <nlohmann/json.hpp>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "schedmesh/validation/project_validator.h"

namespace schedmesh::io {
namespace {

using Json = nlohmann::json;

template <typename Id>
Json id_json(const Id& id) {
  return id.value();
}

template <typename Id>
Json ids_json(const std::vector<Id>& ids) {
  Json output = Json::array();
  for (const auto& id : ids) {
    output.push_back(id_json(id));
  }
  return output;
}

template <typename Id>
Id read_id(const Json& value) {
  return Id{value.get<std::string>()};
}

template <typename Id>
std::vector<Id> read_ids(const Json& values) {
  std::vector<Id> output;
  output.reserve(values.size());
  for (const auto& value : values) {
    output.push_back(read_id<Id>(value));
  }
  return output;
}

Json optional_minutes_json(const std::optional<std::chrono::minutes>& value) {
  return value ? Json(value->count()) : Json(nullptr);
}

std::optional<std::chrono::minutes> read_optional_minutes(const Json& value) {
  if (value.is_null()) {
    return std::nullopt;
  }
  return std::chrono::minutes{value.get<std::int64_t>()};
}

Json calendar_json(const domain::Calendar& calendar) {
  Json days = Json::array();
  for (const auto& day : calendar.days) {
    days.push_back(
        Json{{"display_name", day.display_name}, {"id", day.id}, {"ordinal", day.ordinal}});
  }

  Json periods = Json::array();
  for (const auto& period : calendar.periods) {
    periods.push_back(Json{{"end_time_minutes", optional_minutes_json(period.end_time)},
                           {"id", period.id},
                           {"ordinal", period.ordinal},
                           {"start_time_minutes", optional_minutes_json(period.start_time)}});
  }

  Json slots = Json::array();
  for (const auto& slot : calendar.slots) {
    slots.push_back(Json{{"day_index", slot.day_index},
                         {"id", id_json(slot.id)},
                         {"period_index", slot.period_index}});
  }
  return Json{
      {"days", std::move(days)}, {"periods", std::move(periods)}, {"slots", std::move(slots)}};
}

domain::Calendar read_calendar(const Json& value) {
  domain::Calendar calendar;
  for (const auto& item : value.at("days")) {
    calendar.days.push_back({.id = item.at("id").get<std::string>(),
                             .display_name = item.at("display_name").get<std::string>(),
                             .ordinal = item.at("ordinal").get<int>()});
  }
  for (const auto& item : value.at("periods")) {
    calendar.periods.push_back({.id = item.at("id").get<std::string>(),
                                .ordinal = item.at("ordinal").get<int>(),
                                .start_time = read_optional_minutes(item.at("start_time_minutes")),
                                .end_time = read_optional_minutes(item.at("end_time_minutes"))});
  }
  for (const auto& item : value.at("slots")) {
    calendar.slots.push_back({.id = read_id<domain::SlotId>(item.at("id")),
                              .day_index = item.at("day_index").get<std::size_t>(),
                              .period_index = item.at("period_index").get<std::size_t>()});
  }
  return calendar;
}

Json subjects_json(const std::vector<domain::Subject>& subjects) {
  Json output = Json::array();
  for (const auto& subject : subjects) {
    Json item{{"conflicting_subjects", ids_json(subject.conflicting_subjects)},
              {"display_name", subject.display_name},
              {"forbid_first_period", subject.forbid_first_period},
              {"forbid_last_period", subject.forbid_last_period},
              {"id", id_json(subject.id)},
              {"required_consecutive_periods", subject.required_consecutive_periods}};
    if (subject.maximum_occurrences_per_day) {
      item["maximum_occurrences_per_day"] = *subject.maximum_occurrences_per_day;
    }
    output.push_back(std::move(item));
  }
  return output;
}

Json teachers_json(const std::vector<domain::Teacher>& teachers) {
  Json output = Json::array();
  for (const auto& teacher : teachers) {
    output.push_back(
        Json{{"display_name", teacher.display_name},
             {"id", id_json(teacher.id)},
             {"maximum_daily_load",
              teacher.maximum_daily_load ? Json(*teacher.maximum_daily_load) : Json(nullptr)},
             {"maximum_weekly_load", teacher.maximum_weekly_load},
             {"qualified_subjects", ids_json(teacher.qualified_subjects)},
             {"unavailable_slots", ids_json(teacher.unavailable_slots)}});
  }
  return output;
}

Json groups_json(const std::vector<domain::StudentGroup>& groups) {
  Json output = Json::array();
  for (const auto& group : groups) {
    output.push_back(
        Json{{"allowed_slots", ids_json(group.allowed_slots)},
             {"allow_repeated_subjects_per_day", group.allow_repeated_subjects_per_day},
             {"display_name", group.display_name},
             {"grade", group.grade},
             {"id", id_json(group.id)}});
  }
  return output;
}

Json rooms_json(const std::vector<domain::Room>& rooms) {
  Json output = Json::array();
  for (const auto& room : rooms) {
    output.push_back(Json{{"capacity", room.capacity},
                          {"display_name", room.display_name},
                          {"features", room.features},
                          {"id", id_json(room.id)},
                          {"unavailable_slots", ids_json(room.unavailable_slots)}});
  }
  return output;
}

Json teacher_requirements_json(const std::vector<domain::TeacherRequirement>& requirements) {
  Json output = Json::array();
  for (const auto& requirement : requirements) {
    output.push_back(
        Json{{"candidates", ids_json(requirement.candidates)},
             {"fixed_teacher",
              requirement.fixed_teacher ? id_json(*requirement.fixed_teacher) : Json(nullptr)},
             {"lane", requirement.lane}});
  }
  return output;
}

Json room_requirements_json(const std::vector<domain::RoomRequirement>& requirements) {
  Json output = Json::array();
  for (const auto& requirement : requirements) {
    output.push_back(Json{
        {"candidates", ids_json(requirement.candidates)},
        {"fixed_room", requirement.fixed_room ? id_json(*requirement.fixed_room) : Json(nullptr)},
        {"lane", requirement.lane},
        {"minimum_capacity", requirement.minimum_capacity},
        {"required_features", requirement.required_features}});
  }
  return output;
}

Json meetings_json(const std::vector<domain::Meeting>& meetings) {
  Json output = Json::array();
  for (const auto& meeting : meetings) {
    Json item{{"allowed_start_slots", ids_json(meeting.allowed_start_slots)},
              {"distribution_key", meeting.distribution_key},
              {"duration_in_periods", meeting.duration_in_periods},
              {"groups", ids_json(meeting.groups)},
              {"id", id_json(meeting.id)},
              {"room_requirements", room_requirements_json(meeting.room_requirements)},
              {"subject", id_json(meeting.subject)},
              {"teacher_requirements", teacher_requirements_json(meeting.teacher_requirements)}};
    if (!meeting.simultaneity_keys.empty()) {
      item["simultaneity_keys"] = meeting.simultaneity_keys;
    }
    if (!meeting.resource_lanes_aligned) {
      item["resource_lanes_aligned"] = false;
    }
    output.push_back(std::move(item));
  }
  return output;
}

domain::Project read_project(const Json& root) {
  domain::Project project;
  project.schema_version = root.at("schema_version").get<std::uint32_t>();
  project.metadata = {.id = root.at("metadata").at("id").get<std::string>(),
                      .display_name = root.at("metadata").at("display_name").get<std::string>()};
  project.calendar = read_calendar(root.at("calendar"));

  for (const auto& item : root.at("subjects")) {
    project.subjects.push_back(
        {.id = read_id<domain::SubjectId>(item.at("id")),
         .display_name = item.at("display_name").get<std::string>(),
         .required_consecutive_periods = item.at("required_consecutive_periods").get<int>(),
         .maximum_occurrences_per_day =
             item.contains("maximum_occurrences_per_day")
                 ? std::optional<int>{item.at("maximum_occurrences_per_day").get<int>()}
                 : std::nullopt,
         .forbid_first_period = item.at("forbid_first_period").get<bool>(),
         .forbid_last_period = item.at("forbid_last_period").get<bool>(),
         .conflicting_subjects = read_ids<domain::SubjectId>(item.at("conflicting_subjects"))});
  }
  for (const auto& item : root.at("teachers")) {
    project.teachers.push_back(
        {.id = read_id<domain::TeacherId>(item.at("id")),
         .display_name = item.at("display_name").get<std::string>(),
         .qualified_subjects = read_ids<domain::SubjectId>(item.at("qualified_subjects")),
         .unavailable_slots = read_ids<domain::SlotId>(item.at("unavailable_slots")),
         .maximum_weekly_load = item.at("maximum_weekly_load").get<int>(),
         .maximum_daily_load = item.at("maximum_daily_load").is_null()
                                   ? std::nullopt
                                   : std::optional<int>{item.at("maximum_daily_load").get<int>()}});
  }
  for (const auto& item : root.at("student_groups")) {
    project.student_groups.push_back(
        {.id = read_id<domain::StudentGroupId>(item.at("id")),
         .display_name = item.at("display_name").get<std::string>(),
         .grade = item.at("grade").get<int>(),
         .allowed_slots = read_ids<domain::SlotId>(item.at("allowed_slots")),
         .allow_repeated_subjects_per_day =
             item.at("allow_repeated_subjects_per_day").get<bool>()});
  }
  for (const auto& item : root.at("rooms")) {
    project.rooms.push_back(
        {.id = read_id<domain::RoomId>(item.at("id")),
         .display_name = item.at("display_name").get<std::string>(),
         .capacity = item.at("capacity").get<int>(),
         .features = item.at("features").get<std::set<std::string>>(),
         .unavailable_slots = read_ids<domain::SlotId>(item.at("unavailable_slots"))});
  }
  for (const auto& item : root.at("meetings")) {
    domain::Meeting meeting{
        .id = read_id<domain::MeetingId>(item.at("id")),
        .subject = read_id<domain::SubjectId>(item.at("subject")),
        .groups = read_ids<domain::StudentGroupId>(item.at("groups")),
        .allowed_start_slots = read_ids<domain::SlotId>(item.at("allowed_start_slots")),
        .duration_in_periods = item.at("duration_in_periods").get<int>(),
        .distribution_key = item.at("distribution_key").get<std::string>(),
        .simultaneity_keys = item.value("simultaneity_keys", std::vector<std::string>{}),
        .resource_lanes_aligned = item.value("resource_lanes_aligned", true)};
    for (const auto& requirement : item.at("teacher_requirements")) {
      meeting.teacher_requirements.push_back(
          {.fixed_teacher = requirement.at("fixed_teacher").is_null()
                                ? std::nullopt
                                : std::optional<domain::TeacherId>{read_id<domain::TeacherId>(
                                      requirement.at("fixed_teacher"))},
           .candidates = read_ids<domain::TeacherId>(requirement.at("candidates")),
           .lane = requirement.at("lane").get<int>()});
    }
    for (const auto& requirement : item.at("room_requirements")) {
      meeting.room_requirements.push_back(
          {.fixed_room = requirement.at("fixed_room").is_null()
                             ? std::nullopt
                             : std::optional<domain::RoomId>{read_id<domain::RoomId>(
                                   requirement.at("fixed_room"))},
           .candidates = read_ids<domain::RoomId>(requirement.at("candidates")),
           .required_features = requirement.at("required_features").get<std::set<std::string>>(),
           .minimum_capacity = requirement.value("minimum_capacity", 0),
           .lane = requirement.at("lane").get<int>()});
    }
    project.meetings.push_back(std::move(meeting));
  }
  project.preferences.minimize_last_day_load =
      root.at("preferences").at("minimize_last_day_load").get<bool>();
  return project;
}

void report_unknown_top_level_fields(const Json& root, validation::ValidationResult& validation) {
  static const std::set<std::string> known_fields = {"calendar",       "meetings", "metadata",
                                                     "preferences",    "rooms",    "schema_version",
                                                     "student_groups", "subjects", "teachers"};
  for (const auto& [key, value] : root.items()) {
    static_cast<void>(value);
    if (!known_fields.contains(key)) {
      validation.diagnostics.push_back(
          {.code = "json.unknown_field",
           .severity = validation::DiagnosticSeverity::kError,
           .path = "/" + key,
           .message = "Unknown top-level field in schema version 1.",
           .suggested_action = "Remove the field or migrate with a compatible schema reader."});
    }
  }
}

}  // namespace

std::string write_project_json(const domain::Project& project) {
  const Json root{
      {"calendar", calendar_json(project.calendar)},
      {"meetings", meetings_json(project.meetings)},
      {"metadata",
       Json{{"display_name", project.metadata.display_name}, {"id", project.metadata.id}}},
      {"preferences", Json{{"minimize_last_day_load", project.preferences.minimize_last_day_load}}},
      {"rooms", rooms_json(project.rooms)},
      {"schema_version", project.schema_version},
      {"student_groups", groups_json(project.student_groups)},
      {"subjects", subjects_json(project.subjects)},
      {"teachers", teachers_json(project.teachers)}};
  return root.dump(2) + "\n";
}

ProjectReadResult read_project_json(std::string_view input) {
  ProjectReadResult result;
  try {
    const Json root = Json::parse(input);
    if (!root.is_object()) {
      result.validation.diagnostics.push_back(
          {.code = "json.invalid_root",
           .severity = validation::DiagnosticSeverity::kError,
           .path = "/",
           .message = "Project JSON root must be an object.",
           .suggested_action = "Wrap project fields in a JSON object."});
      return result;
    }
    report_unknown_top_level_fields(root, result.validation);
    result.project = read_project(root);
    auto structural = validation::ProjectValidator{}.validate(*result.project);
    result.validation.diagnostics.insert(result.validation.diagnostics.end(),
                                         std::make_move_iterator(structural.diagnostics.begin()),
                                         std::make_move_iterator(structural.diagnostics.end()));
  } catch (const Json::exception& error) {
    result.project.reset();
    result.validation.diagnostics.push_back(
        {.code = "json.invalid_document",
         .severity = validation::DiagnosticSeverity::kError,
         .path = "/",
         .message = error.what(),
         .suggested_action = "Correct the JSON syntax, required fields, and value types."});
  }
  return result;
}

}  // namespace schedmesh::io
