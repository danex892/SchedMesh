#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "schedmesh/domain/project.h"
#include "schedmesh/validation/diagnostics.h"

namespace schedmesh::io {

struct ProjectReadResult {
  std::optional<domain::Project> project;
  validation::ValidationResult validation;

  [[nodiscard]] bool ok() const noexcept { return project.has_value() && validation.ok(); }
};

[[nodiscard]] std::string write_project_json(const domain::Project& project);
[[nodiscard]] ProjectReadResult read_project_json(std::string_view input);

}  // namespace schedmesh::io
