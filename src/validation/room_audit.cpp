#include "schedmesh/validation/room_audit.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>

namespace schedmesh::validation {
namespace {

const domain::Room* find_room(const domain::Project& project, const domain::RoomId& room_id) {
  const auto room = std::ranges::find_if(
      project.rooms, [&](const domain::Room& candidate) { return candidate.id == room_id; });
  return room == project.rooms.end() ? nullptr : &*room;
}

bool is_eligible(const domain::Room& room, const domain::RoomRequirement& requirement) {
  return (requirement.minimum_capacity <= 0 || room.capacity >= requirement.minimum_capacity) &&
         std::ranges::all_of(requirement.required_features, [&](const std::string& feature) {
           return room.features.contains(feature);
         });
}

std::vector<domain::RoomId> eligible_rooms(const domain::Project& project,
                                           const domain::RoomRequirement& requirement) {
  std::vector<domain::RoomId> rooms = requirement.candidates;
  if (requirement.fixed_room) {
    rooms = {*requirement.fixed_room};
  }
  std::erase_if(rooms, [&](const domain::RoomId& room_id) {
    const domain::Room* room = find_room(project, room_id);
    return room == nullptr || !is_eligible(*room, requirement);
  });
  std::ranges::sort(rooms);
  rooms.erase(std::ranges::unique(rooms).begin(), rooms.end());
  return rooms;
}

int available_periods(const domain::Project& project, const std::vector<domain::RoomId>& room_ids) {
  int result{};
  for (const domain::RoomId& room_id : room_ids) {
    const domain::Room* room = find_room(project, room_id);
    if (room == nullptr) {
      continue;
    }
    const auto unavailable = std::ranges::count_if(project.calendar.slots, [&](const auto& slot) {
      return std::ranges::find(room->unavailable_slots, slot.id) != room->unavailable_slots.end();
    });
    result += static_cast<int>(project.calendar.slots.size()) - static_cast<int>(unavailable);
  }
  return result;
}

}  // namespace

bool RoomPoolBound::overloaded() const noexcept {
  return required_periods > available_room_periods;
}

bool RoomAuditReport::has_proven_capacity_overload() const noexcept {
  return std::ranges::any_of(pool_bounds, &RoomPoolBound::overloaded);
}

RoomAuditReport audit_rooms(const domain::Project& project) {
  RoomAuditReport report{
      .statistics = {.rooms = project.rooms.size(), .meetings = project.meetings.size()},
      .pool_bounds = {}};
  std::map<std::vector<domain::RoomId>, int> required_periods_by_pool;
  for (const domain::Meeting& meeting : project.meetings) {
    if (meeting.room_requirements.empty()) {
      ++report.statistics.meetings_without_rooms;
    }
    for (const domain::RoomRequirement& requirement : meeting.room_requirements) {
      ++report.statistics.room_lanes;
      report.statistics.fixed_room_lanes += requirement.fixed_room.has_value() ? 1U : 0U;
      report.statistics.alternative_room_lanes += requirement.candidates.size() > 1U ? 1U : 0U;
      report.statistics.feature_room_lanes += requirement.required_features.empty() ? 0U : 1U;
      report.statistics.capacity_room_lanes += requirement.minimum_capacity > 0 ? 1U : 0U;
      required_periods_by_pool[eligible_rooms(project, requirement)] += meeting.duration_in_periods;
    }
  }
  report.pool_bounds.reserve(required_periods_by_pool.size());
  for (const auto& [rooms, required_periods] : required_periods_by_pool) {
    report.pool_bounds.push_back({.rooms = rooms,
                                  .required_periods = required_periods,
                                  .available_room_periods = available_periods(project, rooms)});
  }
  return report;
}

}  // namespace schedmesh::validation
