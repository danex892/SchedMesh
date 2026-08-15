#include "schedmesh/import/legacy_settings.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <string_view>
#include <utility>

namespace schedmesh::import {
namespace {

constexpr int kMaximumDays = 31;
constexpr int kMaximumLessonsPerSession = 48;

constexpr std::array<std::string_view, 15> kRuntimeOnlyKeys = {
    "steps",       "debug",          "debug_fstpl",  "debug_file", "checkday",
    "bugday",      "bugclass",       "reset_days",   "errors_limit", "randtype",
    "random_seed", "threads",        "improve_timetable", "tofile", "output_file"};

void add_diagnostic(MigrationReport& report, MigrationSeverity severity, std::string code,
                    std::string path, std::string message, std::string action) {
  report.diagnostics.push_back({.severity = severity,
                                .code = std::move(code),
                                .path = std::move(path),
                                .message = std::move(message),
                                .suggested_action = std::move(action)});
}

std::vector<std::string> split(std::string_view value, char delimiter) {
  std::vector<std::string> values;
  std::size_t begin = 0;
  while (begin <= value.size()) {
    const std::size_t end = value.find(delimiter, begin);
    const std::string_view part = value.substr(begin, end - begin);
    const std::size_t first = part.find_first_not_of(" \t\r\n");
    const std::size_t last = part.find_last_not_of(" \t\r\n");
    if (first != std::string_view::npos) {
      values.emplace_back(part.substr(first, last - first + 1));
    }
    if (end == std::string_view::npos) {
      break;
    }
    begin = end + 1;
  }
  return values;
}

bool read_integer(const LegacyConfig& config, std::string_view key, int minimum, int maximum,
                  int& destination, MigrationReport& report) {
  const auto iterator = config.values.find(key);
  if (iterator == config.values.end()) {
    add_diagnostic(report, MigrationSeverity::kError, "legacy.config.required_key_missing",
                   "/config/" + std::string(key), "Required configuration key is missing.",
                   "Add the key to settings.conf.");
    return false;
  }
  const std::string& value = iterator->second;
  int parsed{};
  const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), parsed);
  if (error != std::errc{} || end != value.data() + value.size() || parsed < minimum ||
      parsed > maximum) {
    add_diagnostic(report, MigrationSeverity::kError, "legacy.config.invalid_integer",
                   "/config/" + std::string(key),
                   "Value '" + value + "' is not an integer in the supported range.",
                   "Use a value from " + std::to_string(minimum) + " to " +
                       std::to_string(maximum) + ".");
    return false;
  }
  destination = parsed;
  ++report.consumed_fields;
  return true;
}

std::string read_required_string(const LegacyConfig& config, std::string_view key,
                                 MigrationReport& report) {
  const auto iterator = config.values.find(key);
  if (iterator == config.values.end() || iterator->second.empty()) {
    add_diagnostic(report, MigrationSeverity::kError, "legacy.config.required_key_missing",
                   "/config/" + std::string(key), "Required configuration value is missing.",
                   "Set a non-empty value in settings.conf.");
    return {};
  }
  ++report.consumed_fields;
  return iterator->second;
}

}  // namespace

bool LegacySettingsReadResult::ok() const noexcept {
  return settings.has_value() && report.ok();
}

LegacySettingsReadResult decode_legacy_settings(const LegacyConfig& config) {
  LegacySettingsReadResult result;
  LegacySettings settings;
  result.report.source_records = config.values.size();

  read_integer(config, "days", 1, kMaximumDays, settings.days, result.report);
  read_integer(config, "maxlessons", 1, kMaximumLessonsPerSession,
               settings.maximum_lessons_per_session, result.report);
  read_integer(config, "sessions", 1, 2, settings.sessions, result.report);
  int last_day_short = 0;
  if (read_integer(config, "last_day_short", 0, 1, last_day_short, result.report)) {
    settings.last_day_short = last_day_short != 0;
  }
  settings.input_file = read_required_string(config, "file", result.report);
  settings.classrooms_file = read_required_string(config, "classrooms_file", result.report);
  settings.physical_culture_name =
      read_required_string(config, "physical_culture_name", result.report);
  settings.day_names = split(read_required_string(config, "days_of_the_week", result.report), '/');

  const auto read_list = [&](std::string_view key, std::vector<std::string>& destination) {
    const auto iterator = config.values.find(key);
    if (iterator != config.values.end()) {
      destination = split(iterator->second, '/');
      ++result.report.consumed_fields;
    }
  };
  read_list("double_lessons", settings.double_lessons);
  read_list("not_first_last", settings.not_first_or_last);
  read_list("entire_course_per_day", settings.entire_course_per_day);

  if (const auto iterator = config.values.find("methodical_days_file");
      iterator != config.values.end() && !iterator->second.empty()) {
    settings.methodical_days_file = iterator->second;
    ++result.report.consumed_fields;
  }
  if (const auto iterator = config.values.find("conflicts"); iterator != config.values.end()) {
    for (const std::string& pair : split(iterator->second, ',')) {
      std::vector<std::string> subjects = split(pair, '/');
      if (subjects.size() != 2) {
        add_diagnostic(result.report, MigrationSeverity::kError,
                       "legacy.config.invalid_conflict_pair", "/config/conflicts",
                       "Conflict entry '" + pair + "' does not contain exactly two subjects.",
                       "Use comma-separated pairs in the form 'Subject A / Subject B'.");
      } else {
        settings.conflicts.emplace_back(std::move(subjects[0]), std::move(subjects[1]));
      }
    }
    ++result.report.consumed_fields;
  }

  for (const auto& [key, value] : config.values) {
    (void)value;
    if (std::ranges::find(kRuntimeOnlyKeys, key) != kRuntimeOnlyKeys.end()) {
      ++result.report.ignored_fields;
    }
  }
  const std::size_t accounted = result.report.consumed_fields + result.report.ignored_fields;
  if (accounted < config.values.size()) {
    for (const auto& [key, value] : config.values) {
      (void)value;
      const bool known = key == "days" || key == "maxlessons" || key == "sessions" ||
                         key == "last_day_short" || key == "file" ||
                         key == "classrooms_file" || key == "methodical_days_file" ||
                         key == "physical_culture_name" || key == "days_of_the_week" ||
                         key == "double_lessons" || key == "not_first_last" ||
                         key == "entire_course_per_day" || key == "conflicts" ||
                         std::ranges::find(kRuntimeOnlyKeys, key) != kRuntimeOnlyKeys.end();
      if (!known) {
        ++result.report.ignored_fields;
        add_diagnostic(result.report, MigrationSeverity::kWarning, "legacy.config.unknown_key",
                       "/config/" + key, "Unknown configuration key was not imported.",
                       "Remove the key or add an explicit migration rule.");
      }
    }
  }

  if (settings.day_names.size() < static_cast<std::size_t>(settings.days)) {
    add_diagnostic(result.report, MigrationSeverity::kError, "legacy.config.too_few_day_names",
                   "/config/days_of_the_week",
                   "The day-name list is shorter than the configured day count.",
                   "Provide at least one name for every configured day.");
  }
  if (result.report.ok()) {
    result.settings = std::move(settings);
  }
  return result;
}

}  // namespace schedmesh::import
