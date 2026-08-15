#pragma once

#include "schedmesh/domain/project.h"
#include "schedmesh/domain/schedule.h"
#include "schedmesh/validation/diagnostics.h"

namespace schedmesh::validation {

class ScheduleValidator {
 public:
  [[nodiscard]] ValidationResult validate(const domain::Project& project,
                                          const domain::Schedule& schedule) const;
};

}  // namespace schedmesh::validation
