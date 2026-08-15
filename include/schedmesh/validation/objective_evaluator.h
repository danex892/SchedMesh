#pragma once

#include <cstdint>

#include "schedmesh/domain/project.h"
#include "schedmesh/domain/schedule.h"

namespace schedmesh::validation {

struct ObjectiveBreakdown {
  std::int64_t teacher_idle_periods{};
  std::int64_t group_idle_periods{};
  std::int64_t late_period_load{};
  std::int64_t last_day_load{};

  [[nodiscard]] std::int64_t total() const noexcept;
};

class ObjectiveEvaluator {
 public:
  [[nodiscard]] ObjectiveBreakdown evaluate(const domain::Project& project,
                                            const domain::Schedule& schedule) const;
};

}  // namespace schedmesh::validation
