#include "schedmesh/import/legacy_project.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <map>
#include <set>
#include <string_view>
#include <utility>

namespace schedmesh::import {
namespace {

struct Column {
  std::size_t source_index{};
  int session{};
  domain::StudentGroupId group_id;
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

int grade_of(std::string_view group_name) {
  int grade{};
  const auto [end, error] =
      std::from_chars(group_name.data(), group_name.data() + group_name.size(), grade);
  return error == std::errc{} && end != group_name.data() ? grade : 0;
}

std::vector<domain::SlotId> allowed_slots(const domain::Project& project, const LegacySettings& settings,
                                          int session) {
  const std::size_t first_period =
      settings.sessions == 1 ? 0U : static_cast<std::size_t>((session - 1) *
                                                             (settings.maximum_lessons_per_session - 1));
  const std::size_t end_period = first_period + settings.maximum_lessons_per_session;
  std::vector<domain::SlotId> result;
  for (const domain::Slot& slot : project.calendar.slots) {
    if (slot.period_index >= first_period && slot.period_index < end_period) {
      result.push_back(slot.id);
    }
  }
  return result;
}

}  // namespace

bool LegacyProjectImportResult::ok() const noexcept {
  return project.has_value() && report.ok();
}

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
  if (timetable[0].size() != width) {
    add_error(result.report, "legacy.timetable.header_width_mismatch", "/timetable/rows/0",
              "Shifts and Group header rows have different column counts.",
              "Add or remove cells so both rows describe the same group columns.");
    return result;
  }

  domain::Project project{.metadata = {.id = "legacy-import", .display_name = "Legacy import"}};
  std::vector<domain::Day> days;
  for (int index = 0; index < settings.days; ++index) {
    days.push_back({.id = "day-" + std::to_string(index + 1),
                    .display_name = settings.day_names[index],
                    .ordinal = index});
  }
  const int period_count = settings.sessions == 1 ? settings.maximum_lessons_per_session
                                                   : settings.maximum_lessons_per_session * 2 - 1;
  std::vector<domain::Period> periods;
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
    const domain::StudentGroupId group_id{
        "group-" + component + (occurrence == 1 ? "" : "-" + std::to_string(occurrence))};
    project.student_groups.push_back({.id = group_id,
                                      .display_name = group_name,
                                      .grade = grade_of(group_name),
                                      .allowed_slots = allowed_slots(project, settings, *session)});
    columns.push_back({.source_index = source_index, .session = *session, .group_id = group_id});
    ++result.report.consumed_fields;
  }

  std::map<std::string, std::size_t, std::less<>> teacher_indices;
  std::map<std::string, domain::SubjectId, std::less<>> subject_ids;
  std::map<std::string, std::vector<std::size_t>, std::less<>> meeting_batches;
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
      project.teachers.push_back({.id = domain::TeacherId{"teacher-" + stable_component(current_teacher)},
                                  .display_name = current_teacher});
    }
    domain::Teacher& teacher = project.teachers[teacher_index];
    const auto [subject_iterator, inserted] = subject_ids.try_emplace(
        subject_name, domain::SubjectId{"subject-" + stable_component(subject_name)});
    if (inserted) {
      project.subjects.push_back({.id = subject_iterator->second, .display_name = subject_name});
    }
    const domain::SubjectId subject_id = subject_iterator->second;
    if (std::ranges::find(teacher.qualified_subjects, subject_id) == teacher.qualified_subjects.end()) {
      teacher.qualified_subjects.push_back(subject_id);
    }

    for (const Column& column : columns) {
      if (column.source_index >= row.size() || trim(row[column.source_index]).empty()) {
        continue;
      }
      const std::vector<std::string> profiles = split_profiles(row[column.source_index]);
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
        const std::string batch_key = column.group_id.value() + "\n" + subject_id.value() + "\n" +
                                      std::to_string(profile) + "\n" + std::to_string(*hours);
        auto& batch = meeting_batches[batch_key];
        if (batch.empty()) {
          for (int occurrence = 0; occurrence < *hours; ++occurrence) {
            const std::size_t meeting_number = project.meetings.size() + 1;
            project.meetings.push_back(
                {.id = domain::MeetingId{"meeting-" + std::to_string(meeting_number)},
                 .subject = subject_id,
                 .groups = {column.group_id},
                 .teacher_requirements = {{.fixed_teacher = teacher.id, .lane = 0}},
                 .allowed_start_slots = allowed_slots(project, settings, column.session),
                 .distribution_key = column.group_id.value() + "-" + subject_id.value() +
                                     (profiles.size() == 1
                                          ? ""
                                          : "-profile-" + std::to_string(profile + 1))});
            batch.push_back(project.meetings.size() - 1);
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

  if (result.report.ok()) {
    result.project = std::move(project);
  }
  return result;
}

}  // namespace schedmesh::import
