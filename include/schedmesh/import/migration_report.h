#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace schedmesh::import {

enum class MigrationSeverity { kError, kWarning };

struct MigrationDiagnostic {
  MigrationSeverity severity{MigrationSeverity::kError};
  std::string code;
  std::string path;
  std::string message;
  std::string suggested_action;
};

struct MigrationReport {
  std::vector<MigrationDiagnostic> diagnostics;
  std::size_t source_records{};
  std::size_t consumed_fields{};
  std::size_t ignored_fields{};

  [[nodiscard]] bool ok() const noexcept;
};

}  // namespace schedmesh::import
