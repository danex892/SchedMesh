#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "schedmesh/domain/schedule.h"

namespace schedmesh::io {

struct ScheduleReadResult {
  std::optional<domain::Schedule> schedule;
  std::string error;

  [[nodiscard]] bool ok() const noexcept { return schedule.has_value() && error.empty(); }
};

[[nodiscard]] std::string write_schedule_json(const domain::Schedule& schedule);
[[nodiscard]] ScheduleReadResult read_schedule_json(std::string_view input);

}  // namespace schedmesh::io
