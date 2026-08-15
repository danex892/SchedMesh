#pragma once

#include <optional>

#include "schedmesh/domain/project.h"
#include "schedmesh/import/legacy_settings.h"
#include "schedmesh/import/legacy_text.h"

namespace schedmesh::import {

struct LegacyProjectImportResult {
  std::optional<domain::Project> project;
  MigrationReport report;

  [[nodiscard]] bool ok() const noexcept;
};

[[nodiscard]] LegacyProjectImportResult import_legacy_timetable(const LegacySettings& settings,
                                                                const CsvTable& timetable);

[[nodiscard]] LegacyProjectImportResult import_legacy_resources(
    domain::Project project, const CsvTable& classrooms,
    const std::optional<CsvTable>& methodical_days = std::nullopt);

}  // namespace schedmesh::import
