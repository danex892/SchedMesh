#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "schedmesh/domain/project.h"
#include "schedmesh/domain/schedule.h"

namespace schedmesh::import {

struct XhsttImportResult {
  std::optional<domain::Project> project;
  std::optional<domain::Schedule> reference_schedule;
  std::vector<std::string> warnings;
  std::string error;

  [[nodiscard]] bool ok() const noexcept { return project.has_value() && error.empty(); }
};

[[nodiscard]] XhsttImportResult import_xhstt(std::string_view contents);

}  // namespace schedmesh::import
