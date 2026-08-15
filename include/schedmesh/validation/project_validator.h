#pragma once

#include "schedmesh/domain/project.h"
#include "schedmesh/validation/diagnostics.h"

namespace schedmesh::validation {

class ProjectValidator {
 public:
  [[nodiscard]] ValidationResult validate(const domain::Project& project) const;
};

}  // namespace schedmesh::validation
