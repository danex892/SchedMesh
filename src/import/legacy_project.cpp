#include "schedmesh/import/legacy_project.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <map>
#include <set>
#include <string_view>
#include <utility>

namespace schedmesh::import {
namespace {

struct Column {
  std::size_t source_index{};
  int session{};
  std::vector<domain::StudentGroupId> group_ids;
};

std::string trim(std::string_view value) {
  const std::size_t first = value.find_first_not_of(" \t\r\n");
  if (first == std::string_view::npos) {
    return {};
  }
  const std::size_t last = value.find_last_not_of(" \t\r\n");
  return std::string(value.substr(first, last - first + 1));
}

std::string stable_component(std::string_view value) {
  std::string result;
  bool separator = false;
  for (const unsigned char character : value) {
    if (std::isalnum(character) != 0) {
      if (separator && !result.empty()) {
        result.push_back('-');
      }
      result.push_back(static_cast<char>(std::tolower(character)));
      separator = false;
    } else {
      separator = true;
    }
  }
  return result.empty() ? "unnamed" : result;
}

void add_error(MigrationReport& report, std::string code, std::string path, std::string message,
               std::string action) {
  report.diagnostics.push_back({.severity = MigrationSeverity::kError,
                                .code = std::move(code),
                                .path = std::move(path),
                                .message = std::move(message),
                                .suggested_action = std::move(action)});
}

std::optional<int> parse_nonnegative_integer(std::string_view value) {
  const std::string normalized = trim(value);
  int parsed{};
  const auto [end, error] =
      std::from_chars(normalized.data(), normalized.data() + normalized.size(), parsed);
  if (normalized.empty() || error != std::errc{} || end != normalized.data() + normalized.size() ||
      parsed < 0) {
    return std::nullopt;
  }
  return parsed;
}

std::vector<std::string> split_profiles(std::string_view value) {
  std::vector<std::string> result;
  std::size_t begin = 0;
  do {
    const std::size_t end = value.find('/', begin);
    result.push_back(trim(value.substr(begin, end - begin)));
    if (end == std::string_view::npos) {
      break;
    }
    begin = end + 1;
  } while (true);
  return result;
}

std::size_t profile_count(const CsvTable& timetable, std::size_t source_index) {
  std::size_t count = 1;
  for (std::size_t row_index = 4; row_index < timetable.size(); ++row_index) {
    if (source_index < timetable[row_index].size()) {
      count = std::max(count, split_profiles(timetable[row_index][source_index]).size());
    }
  }
  return count;
}

bool teachers_overlap(const domain::Meeting& first, const domain::Meeting& second) {
  return std::ranges::any_of(first.teacher_requirements, [&](const auto& first_requirement) {
    return first_requirement.fixed_teacher &&
           std::ranges::any_of(second.teacher_requirements, [&](const auto& second_requirement) {
             return second_requirement.fixed_teacher == first_requirement.fixed_teacher;
           });
  });
}

std::vector<std::string> split_semicolon_list(std::string_view value) {
  std::vector<std::string> result;
  std::size_t begin = 0;
  do {
    const std::size_t end = value.find(';', begin);
    const std::string item = trim(value.substr(begin, end - begin));
    if (!item.empty()) {
      result.push_back(item);
    }
    if (end == std::string_view::npos) {
      break;
    }
    begin = end + 1;
  } while (true);
  return result;
}

int grade_of(std::string_view group_name) {
  int grade{};
  const auto [end, error] =
      std::from_chars(group_name.data(), group_name.data() + group_name.size(), grade);
  return error == std::errc{} && end != group_name.data() ? grade : 0;
}

std::vector<domain::SlotId> allowed_slots(const domain::Project& project,
                                          const LegacySettings& settings, int session) {
  const std::size_t first_period =
      settings.sessions == 1
          ? 0U
          : static_cast<std::size_t>((session - 1) * (settings.maximum_lessons_per_session - 1));
  const std::size_t end_period = first_period + settings.maximum_lessons_per_session;
  std::vector<domain::SlotId> result;
  for (const domain::Slot& slot : project.calendar.slots) {
    if (slot.period_index >= first_period && slot.period_index < end_period) {
      result.push_back(slot.id);
    }
  }
  return result;
}

std::vector<domain::SlotId> subject_allowed_slots(const domain::Project& project,
                                                  const LegacySettings& settings, int session,
                                                  const domain::Subject& subject) {
  std::vector<domain::SlotId> result = allowed_slots(project, settings, session);
  const std::size_t first_period =
      settings.sessions == 1
          ? 0U
          : static_cast<std::size_t>((session - 1) * (settings.maximum_lessons_per_session - 1));
  const std::size_t last_period =
      first_period + static_cast<std::size_t>(settings.maximum_lessons_per_session - 1);
  std::erase_if(result, [&](const domain::SlotId& slot_id) {
    const auto slot = std::ranges::find_if(
        project.calendar.slots, [&](const domain::Slot& item) { return item.id == slot_id; });
    return slot != project.calendar.slots.end() &&
           ((slot->period_index + static_cast<std::size_t>(subject.required_consecutive_periods) >
             last_period + 1) ||
            (subject.forbid_first_period && slot->period_index == first_period) ||
            (subject.forbid_last_period && slot->period_index == last_period));
  });
  return result;
}

}  // namespace

bool LegacyProjectImportResult::ok() const noexcept { return project.has_value() && report.ok(); }

LegacyProjectImportResult import_legacy_timetable(const LegacySettings& settings,
                                                  const CsvTable& timetable) {
  LegacyProjectImportResult result;
  result.report.source_records = timetable.size();
  if (timetable.size() < 4 || timetable[0].size() < 3 || timetable[1].size() < 3) {
    add_error(result.report, "legacy.timetable.missing_headers", "/timetable",
              "Timetable must contain Shifts, Group, Double lessons, and Teacher header rows.",
              "Restore the four legacy header rows before importing.");
    return result;
  }
  const std::size_t width = timetable[1].size();
  if (trim(timetable[0][1]) != "Shifts" || trim(timetable[1][1]) != "Group") {
    add_error(result.report, "legacy.timetable.invalid_headers", "/timetable/rows/0",
              "The first two rows are not the expected Shifts and Group headers.",
              "Use the original legacy timetable matrix layout.");
    return result;
  }
  if (timetable[0].size() != width || timetable[2].size() != width) {
    add_error(result.report, "legacy.timetable.header_width_mismatch", "/timetable/rows/0",
              "Shifts, Group, and Double lessons header rows have different column counts.",
              "Add or remove cells so all header rows describe the same group columns.");
    return result;
  }

  domain::Project project{.metadata = {.id = "legacy-import", .display_name = "Legacy import"}};
  project.preferences.minimize_last_day_load = settings.last_day_short;
  std::vector<domain::Day> days;
  days.reserve(static_cast<std::size_t>(settings.days));
  for (int index = 0; index < settings.days; ++index) {
    days.push_back({.id = "day-" + std::to_string(index + 1),
                    .display_name = settings.day_names[index],
                    .ordinal = index});
  }
  const int period_count = settings.sessions == 1 ? settings.maximum_lessons_per_session
                                                  : (settings.maximum_lessons_per_session * 2) - 1;
  std::vector<domain::Period> periods;
  periods.reserve(static_cast<std::size_t>(period_count));
  for (int index = 0; index < period_count; ++index) {
    periods.push_back({.id = "period-" + std::to_string(index + 1), .ordinal = index});
  }
  project.calendar = domain::make_calendar(std::move(days), std::move(periods));

  std::vector<Column> columns;
  std::map<std::string, int, std::less<>> group_occurrences;
  for (std::size_t source_index = 2; source_index < width; ++source_index) {
    const std::string group_name = trim(timetable[1][source_index]);
    const std::optional<int> session = parse_nonnegative_integer(timetable[0][source_index]);
    if (group_name.empty() || !session || *session < 1 || *session > settings.sessions) {
      add_error(result.report, "legacy.timetable.invalid_group_column",
                "/timetable/columns/" + std::to_string(source_index),
                "Group name or session number is invalid.",
                "Provide a group name and a session from 1 to the configured session count.");
      continue;
    }
    const std::string component = stable_component(group_name);
    const int occurrence = ++group_occurrences[component];
    const domain::StudentGroupId base_group_id{
        "group-" + component + (occurrence == 1 ? "" : "-" + std::to_string(occurrence))};
    const std::size_t profiles = profile_count(timetable, source_index);
    if (profiles > 2) {
      add_error(result.report, "legacy.timetable.unsupported_profile_count",
                "/timetable/columns/" + std::to_string(source_index),
                "A group column contains more than two profile lanes.",
                "Split the curriculum into at most two legacy profile lanes.");
      continue;
    }
    const std::string repeated_subjects = trim(timetable[2][source_index]);
    if (!repeated_subjects.empty() && repeated_subjects != "0" && repeated_subjects != "1") {
      add_error(result.report, "legacy.timetable.invalid_repeated_subject_policy",
                "/timetable/rows/2/columns/" + std::to_string(source_index),
                "Double lessons group flag must be empty, 0, or 1.",
                "Use 1 to allow repeated subjects on the same day, otherwise 0 or empty.");
      continue;
    }
    std::vector<domain::StudentGroupId> group_ids;
    group_ids.reserve(profiles);
    for (std::size_t profile = 0; profile < profiles; ++profile) {
      const domain::StudentGroupId group_id{
          base_group_id.value() + (profiles == 1 ? "" : "-profile-" + std::to_string(profile + 1))};
      project.student_groups.push_back(
          {.id = group_id,
           .display_name =
               group_name + (profiles == 1 ? "" : " / profile " + std::to_string(profile + 1)),
           .grade = grade_of(group_name),
           .allowed_slots = allowed_slots(project, settings, *session),
           .allow_repeated_subjects_per_day = repeated_subjects == "1"});
      group_ids.push_back(group_id);
    }
    columns.push_back(
        {.source_index = source_index, .session = *session, .group_ids = std::move(group_ids)});
    ++result.report.consumed_fields;
  }

  std::map<std::string, std::size_t, std::less<>> teacher_indices;
  std::map<std::string, int, std::less<>> teacher_occurrences;
  std::map<std::string, domain::SubjectId, std::less<>> subject_ids;
  std::map<std::string, int, std::less<>> subject_occurrences;
  std::map<std::string, std::vector<std::size_t>, std::less<>> meeting_batches;
  std::map<std::size_t, std::vector<std::vector<std::size_t>>> profile_meetings;
  std::string current_teacher;
  for (std::size_t row_index = 4; row_index < timetable.size(); ++row_index) {
    const CsvRow& row = timetable[row_index];
    if (row.size() > width) {
      add_error(result.report, "legacy.timetable.too_many_columns",
                "/timetable/rows/" + std::to_string(row_index),
                "Data row has more cells than the Group header.",
                "Remove trailing cells or restore the matching group header.");
      continue;
    }
    if (!row.empty() && !trim(row[0]).empty()) {
      current_teacher = trim(row[0]);
    }
    const std::string subject_name = row.size() > 1 ? trim(row[1]) : std::string{};
    const bool has_hours = std::ranges::any_of(columns, [&](const Column& column) {
      return column.source_index < row.size() && !trim(row[column.source_index]).empty();
    });
    if (!has_hours) {
      continue;
    }
    if (current_teacher.empty() || subject_name.empty()) {
      add_error(result.report, "legacy.timetable.missing_teacher_or_subject",
                "/timetable/rows/" + std::to_string(row_index),
                "A row with weekly hours has no teacher or subject.",
                "Set the teacher in this or a preceding row and provide the subject name.");
      continue;
    }

    std::size_t teacher_index{};
    if (const auto found = teacher_indices.find(current_teacher); found != teacher_indices.end()) {
      teacher_index = found->second;
    } else {
      teacher_index = project.teachers.size();
      teacher_indices.emplace(current_teacher, teacher_index);
      const std::string component = stable_component(current_teacher);
      const int occurrence = ++teacher_occurrences[component];
      project.teachers.push_back(
          {.id = domain::TeacherId{"teacher-" + component +
                                   (occurrence == 1 ? "" : "-" + std::to_string(occurrence))},
           .display_name = current_teacher});
    }
    domain::Teacher& teacher = project.teachers[teacher_index];
    auto subject_iterator = subject_ids.find(subject_name);
    if (subject_iterator == subject_ids.end()) {
      const std::string component = stable_component(subject_name);
      const int occurrence = ++subject_occurrences[component];
      const domain::SubjectId subject_id{"subject-" + component +
                                         (occurrence == 1 ? "" : "-" + std::to_string(occurrence))};
      subject_iterator = subject_ids.emplace(subject_name, subject_id).first;
      const bool forbid_boundary = std::ranges::find(settings.not_first_or_last, subject_name) !=
                                   settings.not_first_or_last.end();
      const int required_consecutive_periods =
          std::ranges::find(settings.double_lessons, subject_name) != settings.double_lessons.end()
              ? 2
              : 1;
      project.subjects.push_back({.id = subject_id,
                                  .display_name = subject_name,
                                  .required_consecutive_periods = required_consecutive_periods,
                                  .forbid_first_period = forbid_boundary,
                                  .forbid_last_period = forbid_boundary});
    }
    const domain::SubjectId subject_id = subject_iterator->second;
    const domain::Subject& subject = *std::ranges::find_if(
        project.subjects, [&](const domain::Subject& item) { return item.id == subject_id; });
    if (std::ranges::find(teacher.qualified_subjects, subject_id) ==
        teacher.qualified_subjects.end()) {
      teacher.qualified_subjects.push_back(subject_id);
    }

    for (const Column& column : columns) {
      if (column.source_index >= row.size() || trim(row[column.source_index]).empty()) {
        continue;
      }
      const std::vector<std::string> profiles = split_profiles(row[column.source_index]);
      if (profiles.size() != 1 && profiles.size() != column.group_ids.size()) {
        add_error(result.report, "legacy.timetable.inconsistent_profile_count",
                  "/timetable/rows/" + std::to_string(row_index) + "/columns/" +
                      std::to_string(column.source_index),
                  "Profile hours do not match the number of lanes used by the group column.",
                  "Use either one whole-group value or the same number of slash-separated values.");
        continue;
      }
      for (std::size_t profile = 0; profile < profiles.size(); ++profile) {
        const std::optional<int> hours = parse_nonnegative_integer(profiles[profile]);
        if (!hours) {
          add_error(result.report, "legacy.timetable.invalid_weekly_hours",
                    "/timetable/rows/" + std::to_string(row_index) + "/columns/" +
                        std::to_string(column.source_index),
                    "Weekly hours must be a non-negative integer or slash-separated integers.",
                    "Replace the cell with values such as '3' or '2/1'.");
          continue;
        }
        if (*hours == 0) {
          ++result.report.consumed_fields;
          continue;
        }
        if (*hours % subject.required_consecutive_periods != 0) {
          add_error(
              result.report, "legacy.timetable.incomplete_consecutive_block",
              "/timetable/rows/" + std::to_string(row_index) + "/columns/" +
                  std::to_string(column.source_index),
              "Weekly hours cannot be divided into the subject's required consecutive blocks.",
              "Use a weekly hour count divisible by " +
                  std::to_string(subject.required_consecutive_periods) + ".");
          continue;
        }
        const bool whole_group = profiles.size() == 1;
        const std::vector<domain::StudentGroupId> meeting_groups =
            whole_group ? column.group_ids
                        : std::vector<domain::StudentGroupId>{column.group_ids[profile]};
        const std::string lane_key =
            whole_group ? "whole" : "profile-" + std::to_string(profile + 1);
        const std::string batch_key = column.group_ids.front().value() + "\n" + subject_id.value() +
                                      "\n" + lane_key + "\n" + std::to_string(*hours);
        auto& batch = meeting_batches[batch_key];
        if (batch.empty()) {
          const int occurrence_count = *hours / subject.required_consecutive_periods;
          for (int occurrence = 0; occurrence < occurrence_count; ++occurrence) {
            const std::size_t meeting_number = project.meetings.size() + 1;
            project.meetings.push_back(
                {.id = domain::MeetingId{"meeting-" + std::to_string(meeting_number)},
                 .subject = subject_id,
                 .groups = meeting_groups,
                 .teacher_requirements = {{.fixed_teacher = teacher.id, .lane = 0}},
                 .allowed_start_slots =
                     subject_allowed_slots(project, settings, column.session, subject),
                 .duration_in_periods = subject.required_consecutive_periods,
                 .distribution_key = column.group_ids.front().value() + "-" + subject_id.value() +
                                     "-" + lane_key + "-weekly-" + std::to_string(*hours)});
            batch.push_back(project.meetings.size() - 1);
          }
          if (!whole_group) {
            auto& lanes = profile_meetings[column.source_index];
            if (lanes.empty()) {
              lanes.resize(column.group_ids.size());
            }
            lanes[profile].insert(lanes[profile].end(), batch.begin(), batch.end());
          }
        } else {
          for (const std::size_t meeting_index : batch) {
            auto& requirements = project.meetings[meeting_index].teacher_requirements;
            requirements.push_back(
                {.fixed_teacher = teacher.id, .lane = static_cast<int>(requirements.size())});
          }
        }
        teacher.maximum_weekly_load += *hours;
        ++result.report.consumed_fields;
      }
    }
  }

  for (const auto& [source_index, lanes] : profile_meetings) {
    if (lanes.size() != 2 || lanes[0].size() != lanes[1].size()) {
      add_error(result.report, "legacy.timetable.unbalanced_profile_hours",
                "/timetable/columns/" + std::to_string(source_index),
                "Profile lanes contain different weekly hour totals.",
                "Balance the two profile curricula before migration.");
      continue;
    }
    std::vector<int> right_matches(lanes[1].size(), -1);
    for (std::size_t left = 0; left < lanes[0].size(); ++left) {
      std::vector<bool> visited(lanes[1].size(), false);
      const auto augment = [&](const auto& self, std::size_t candidate) -> bool {
        for (std::size_t right = 0; right < lanes[1].size(); ++right) {
          if (visited[right] || teachers_overlap(project.meetings[lanes[0][candidate]],
                                                 project.meetings[lanes[1][right]])) {
            continue;
          }
          visited[right] = true;
          if (right_matches[right] < 0 ||
              self(self, static_cast<std::size_t>(right_matches[right]))) {
            right_matches[right] = static_cast<int>(candidate);
            return true;
          }
        }
        return false;
      };
      if (!augment(augment, left)) {
        add_error(result.report, "legacy.timetable.profile_pairing_impossible",
                  "/timetable/columns/" + std::to_string(source_index),
                  "Profile lessons cannot be paired without assigning one teacher twice.",
                  "Correct the profile staffing or split overloaded teacher assignments.");
        break;
      }
    }
    for (std::size_t right = 0; right < right_matches.size(); ++right) {
      if (right_matches[right] < 0) {
        continue;
      }
      const std::string key = "legacy-profile-column-" + std::to_string(source_index) + "-pair-" +
                              std::to_string(right + 1);
      project.meetings[lanes[0][static_cast<std::size_t>(right_matches[right])]]
          .simultaneity_keys.push_back(key);
      project.meetings[lanes[1][right]].simultaneity_keys.push_back(key);
    }
  }

  for (const auto& [first_name, second_name] : settings.conflicts) {
    const auto first = subject_ids.find(first_name);
    const auto second = subject_ids.find(second_name);
    if (first == subject_ids.end() || second == subject_ids.end()) {
      ++result.report.ignored_fields;
      std::string conflict_name = first_name;
      conflict_name.append(" / ");
      conflict_name.append(second_name);
      result.report.diagnostics.push_back(
          {.severity = MigrationSeverity::kWarning,
           .code = "legacy.conflicts.unknown_subject",
           .path = "/config/conflicts",
           .message =
               "Subject conflict references a subject absent from the timetable: " + conflict_name,
           .suggested_action = "Correct the configured subject names or remove the stale rule."});
      continue;
    }
    domain::Subject& first_subject = *std::ranges::find_if(
        project.subjects, [&](const domain::Subject& item) { return item.id == first->second; });
    domain::Subject& second_subject = *std::ranges::find_if(
        project.subjects, [&](const domain::Subject& item) { return item.id == second->second; });
    if (std::ranges::find(first_subject.conflicting_subjects, second_subject.id) ==
        first_subject.conflicting_subjects.end()) {
      first_subject.conflicting_subjects.push_back(second_subject.id);
    }
    if (std::ranges::find(second_subject.conflicting_subjects, first_subject.id) ==
        second_subject.conflicting_subjects.end()) {
      second_subject.conflicting_subjects.push_back(first_subject.id);
    }
    ++result.report.consumed_fields;
  }
  for (const std::string& subject_name : settings.entire_course_per_day) {
    ++result.report.ignored_fields;
    result.report.diagnostics.push_back(
        {.severity = MigrationSeverity::kWarning,
         .code = "legacy.entire_course_per_day.unimplemented_legacy_setting",
         .path = "/config/entire_course_per_day",
         .message =
             "Legacy loaded but never enforced entire_course_per_day for subject: " + subject_name,
         .suggested_action =
             "Define the intended scheduling policy explicitly before enabling it."});
  }

  if (result.report.ok()) {
    result.project = std::move(project);
  }
  return result;
}

LegacyProjectImportResult import_legacy_resources(domain::Project project,
                                                  const CsvTable& classrooms,
                                                  const std::optional<CsvTable>& methodical_days) {
  LegacyProjectImportResult result;
  result.report.source_records =
      classrooms.size() + (methodical_days ? methodical_days->size() : 0U);

  std::map<std::string, domain::TeacherId, std::less<>> teachers;
  for (const domain::Teacher& teacher : project.teachers) {
    teachers.emplace(teacher.display_name, teacher.id);
  }
  std::map<std::string, domain::RoomId, std::less<>> rooms;
  std::set<std::string, std::less<>> room_ids;
  for (const domain::Room& room : project.rooms) {
    rooms.emplace(room.display_name, room.id);
    room_ids.insert(room.id.value());
  }
  const auto ensure_room = [&](const std::string& display_name, const std::string& preferred_id,
                               const std::set<std::string>& features = {}) {
    if (const auto found = rooms.find(display_name); found != rooms.end()) {
      return found->second;
    }
    std::string unique_id = preferred_id;
    for (int occurrence = 2; room_ids.contains(unique_id); ++occurrence) {
      unique_id = preferred_id + "-" + std::to_string(occurrence);
    }
    domain::RoomId room_id{unique_id};
    rooms.emplace(display_name, room_id);
    room_ids.insert(unique_id);
    project.rooms.push_back({.id = room_id, .display_name = display_name, .features = features});
    return room_id;
  };

  if (classrooms.empty() || classrooms.front().size() < 2 ||
      trim(classrooms.front()[0]) != "Teacher" || trim(classrooms.front()[1]) != "Rooms") {
    add_error(result.report, "legacy.classrooms.invalid_header", "/classrooms/rows/0",
              "Classroom mapping must start with Teacher and Rooms columns.",
              "Restore the legacy classroom CSV header.");
  } else {
    for (std::size_t row_index = 1; row_index < classrooms.size(); ++row_index) {
      const CsvRow& row = classrooms[row_index];
      if (row.size() < 2) {
        add_error(result.report, "legacy.classrooms.missing_columns",
                  "/classrooms/rows/" + std::to_string(row_index),
                  "Classroom row must contain a teacher and room list.",
                  "Add both required cells or remove the incomplete row.");
        continue;
      }
      const std::string teacher_name = trim(row[0]);
      const auto teacher = teachers.find(teacher_name);
      if (teacher == teachers.end()) {
        result.report.diagnostics.push_back(
            {.severity = MigrationSeverity::kWarning,
             .code = "legacy.classrooms.unknown_teacher",
             .path = "/classrooms/rows/" + std::to_string(row_index) + "/teacher",
             .message = "Classroom mapping references a teacher absent from the timetable: " +
                        teacher_name,
             .suggested_action = "Correct the teacher name or add the teacher to the timetable."});
        continue;
      }
      const std::vector<std::string> room_names = split_semicolon_list(row[1]);
      if (room_names.empty()) {
        const std::string room_name = "Unmapped room for " + teacher_name;
        const domain::RoomId room_id =
            ensure_room(room_name, "room-unmapped-" + stable_component(teacher_name));
        for (domain::Meeting& meeting : project.meetings) {
          for (const domain::TeacherRequirement& requirement : meeting.teacher_requirements) {
            if (requirement.fixed_teacher == teacher->second) {
              meeting.room_requirements.push_back(
                  {.candidates = {room_id}, .lane = requirement.lane});
            }
          }
        }
        ++result.report.consumed_fields;
        result.report.diagnostics.push_back(
            {.severity = MigrationSeverity::kWarning,
             .code = "legacy.classrooms.unmapped_room_interpreted",
             .path = "/classrooms/rows/" + std::to_string(row_index) + "/rooms",
             .message = "An empty legacy room mapping was reconstructed as a teacher-specific "
                        "placeholder room.",
             .suggested_action =
                 "Replace the placeholder with the actual shared room when it becomes known."});
        continue;
      }
      if (room_names.size() == 1 && (room_names.front() == "S" || room_names.front() == "T")) {
        const bool is_gym = room_names.front() == "S";
        std::vector<domain::RoomId> candidates;
        if (is_gym) {
          for (int lane = 1; lane <= 2; ++lane) {
            const std::string room_name = "Legacy gym lane " + std::to_string(lane);
            const domain::RoomId room_id =
                ensure_room(room_name, "room-legacy-gym-" + std::to_string(lane), {"gym"});
            candidates.push_back(room_id);
          }
        } else {
          const std::string room_name = "Legacy technology room for " + teacher_name;
          const domain::RoomId room_id =
              ensure_room(room_name, "room-legacy-technology-" + stable_component(teacher_name),
                          {"technology"});
          candidates.push_back(room_id);
        }
        for (domain::Meeting& meeting : project.meetings) {
          for (const domain::TeacherRequirement& requirement : meeting.teacher_requirements) {
            if (requirement.fixed_teacher == teacher->second) {
              meeting.room_requirements.push_back(
                  {.candidates = candidates,
                   .required_features = {is_gym ? "gym" : "technology"},
                   .lane = requirement.lane});
            }
          }
        }
        ++result.report.consumed_fields;
        result.report.diagnostics.push_back(
            {.severity = MigrationSeverity::kWarning,
             .code = "legacy.classrooms.special_code_interpreted",
             .path = "/classrooms/rows/" + std::to_string(row_index) + "/rooms",
             .message = "Legacy room code '" + room_names.front() +
                        "' was reconstructed as explicit canonical room resources.",
             .suggested_action =
                 "Confirm the inferred facilities against the private school inventory."});
        continue;
      }

      std::vector<domain::RoomId> candidates;
      std::set<domain::RoomId> seen_candidates;
      for (const std::string& room_name : room_names) {
        const domain::RoomId room_id =
            ensure_room(room_name, "room-" + stable_component(room_name));
        if (seen_candidates.insert(room_id).second) {
          candidates.push_back(room_id);
        }
      }
      for (domain::Meeting& meeting : project.meetings) {
        for (const domain::TeacherRequirement& requirement : meeting.teacher_requirements) {
          if (requirement.fixed_teacher == teacher->second) {
            meeting.room_requirements.push_back(
                {.candidates = candidates, .lane = requirement.lane});
          }
        }
      }
      ++result.report.consumed_fields;
    }
  }

  if (methodical_days) {
    if (methodical_days->empty() || methodical_days->front().size() < 2 ||
        trim(methodical_days->front()[0]) != "Teacher") {
      add_error(result.report, "legacy.methodical_days.invalid_header", "/methodical_days/rows/0",
                "Methodical-day mapping must start with a Teacher column and a day-list column.",
                "Restore the legacy methodical-days CSV header.");
    } else {
      for (std::size_t row_index = 1; row_index < methodical_days->size(); ++row_index) {
        const CsvRow& row = (*methodical_days)[row_index];
        if (row.size() < 2) {
          add_error(result.report, "legacy.methodical_days.missing_columns",
                    "/methodical_days/rows/" + std::to_string(row_index),
                    "Methodical-day row must contain a teacher and day list.",
                    "Add both required cells or remove the incomplete row.");
          continue;
        }
        const std::string teacher_name = trim(row[0]);
        const auto teacher_id = teachers.find(teacher_name);
        if (teacher_id == teachers.end()) {
          result.report.diagnostics.push_back(
              {.severity = MigrationSeverity::kWarning,
               .code = "legacy.methodical_days.unknown_teacher",
               .path = "/methodical_days/rows/" + std::to_string(row_index) + "/teacher",
               .message =
                   "Methodical-day mapping references a teacher absent from the timetable: " +
                   teacher_name,
               .suggested_action =
                   "Correct the teacher name or add the teacher to the timetable."});
          continue;
        }
        domain::Teacher& teacher = *std::ranges::find_if(
            project.teachers,
            [&](const domain::Teacher& item) { return item.id == teacher_id->second; });
        for (const std::string& day_name : split_semicolon_list(row[1])) {
          const auto day = std::ranges::find_if(
              project.calendar.days,
              [&](const domain::Day& item) { return item.display_name == day_name; });
          if (day == project.calendar.days.end()) {
            add_error(result.report, "legacy.methodical_days.unknown_day",
                      "/methodical_days/rows/" + std::to_string(row_index) + "/days",
                      "Methodical-day mapping references an unknown day: " + day_name,
                      "Use a day name declared in days_of_the_week.");
            continue;
          }
          const auto day_index =
              static_cast<std::size_t>(std::distance(project.calendar.days.begin(), day));
          for (const domain::Slot& slot : project.calendar.slots) {
            if (slot.day_index == day_index) {
              teacher.unavailable_slots.push_back(slot.id);
            }
          }
          ++result.report.consumed_fields;
        }
      }
    }
  }

  if (result.report.ok()) {
    result.project = std::move(project);
  }
  return result;
}

}  // namespace schedmesh::import
