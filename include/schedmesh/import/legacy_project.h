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

}  // namespace schedmesh::import
