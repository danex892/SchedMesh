#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "schedmesh/import/legacy_text.h"

namespace schedmesh::import {

struct LegacySettings {
  int days{};
  int maximum_lessons_per_session{};
  int sessions{1};
  bool last_day_short{};
  std::string input_file;
  std::string classrooms_file;
  std::optional<std::string> methodical_days_file;
  std::string physical_culture_name;
  std::vector<std::string> day_names;
  std::vector<std::string> double_lessons;
  std::vector<std::string> not_first_or_last;
  std::vector<std::string> entire_course_per_day;
  std::vector<std::pair<std::string, std::string>> conflicts;
};

struct LegacySettingsReadResult {
  std::optional<LegacySettings> settings;
  MigrationReport report;

  [[nodiscard]] bool ok() const noexcept;
};

[[nodiscard]] LegacySettingsReadResult decode_legacy_settings(const LegacyConfig& config);

}  // namespace schedmesh::import
