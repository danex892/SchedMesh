#pragma once

#include <cstddef>
#include <vector>

#include "schedmesh/domain/project.h"

namespace schedmesh::validation {

struct RoomPoolBound {
  std::vector<domain::RoomId> rooms;
  int required_periods{};
  int available_room_periods{};

  [[nodiscard]] bool overloaded() const noexcept;
};

struct RoomAuditStatistics {
  std::size_t rooms{};
  std::size_t meetings{};
  std::size_t meetings_without_rooms{};
  std::size_t room_lanes{};
  std::size_t fixed_room_lanes{};
  std::size_t alternative_room_lanes{};
  std::size_t feature_room_lanes{};
  std::size_t capacity_room_lanes{};
};

struct RoomAuditReport {
  RoomAuditStatistics statistics;
  std::vector<RoomPoolBound> pool_bounds;

  [[nodiscard]] bool has_proven_capacity_overload() const noexcept;
};

[[nodiscard]] RoomAuditReport audit_rooms(const domain::Project& project);

}  // namespace schedmesh::validation
