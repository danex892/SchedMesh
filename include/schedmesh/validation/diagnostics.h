#pragma once

#include <string>
#include <vector>

namespace schedmesh::validation {

enum class DiagnosticSeverity { kError, kWarning };

struct Diagnostic {
  std::string code;
  DiagnosticSeverity severity{DiagnosticSeverity::kError};
  std::string path;
  std::string message;
  std::string entity_id;
  std::string suggested_action;
};

struct ValidationResult {
  std::vector<Diagnostic> diagnostics;

  [[nodiscard]] bool ok() const noexcept;
};

}  // namespace schedmesh::validation
