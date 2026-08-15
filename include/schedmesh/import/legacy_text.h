#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "schedmesh/import/migration_report.h"

namespace schedmesh::import {

using CsvRow = std::vector<std::string>;
using CsvTable = std::vector<CsvRow>;

struct CsvReadResult {
  std::optional<CsvTable> table;
  MigrationReport report;

  [[nodiscard]] bool ok() const noexcept;
};

struct LegacyConfig {
  std::map<std::string, std::string, std::less<>> values;
};

struct LegacyConfigReadResult {
  std::optional<LegacyConfig> config;
  MigrationReport report;

  [[nodiscard]] bool ok() const noexcept;
};

[[nodiscard]] CsvReadResult read_legacy_csv(std::string_view contents);
[[nodiscard]] LegacyConfigReadResult read_legacy_config(std::string_view contents);

}  // namespace schedmesh::import
