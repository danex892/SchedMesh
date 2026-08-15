#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include "schedmesh/domain/ids.h"

namespace schedmesh::domain {

struct Day {
  std::string id;
  std::string display_name;
  int ordinal{};

  auto operator<=>(const Day&) const = default;
};

struct Period {
  std::string id;
  int ordinal{};
  std::optional<std::chrono::minutes> start_time;
  std::optional<std::chrono::minutes> end_time;

  auto operator<=>(const Period&) const = default;
};

struct Slot {
  SlotId id;
  std::size_t day_index{};
  std::size_t period_index{};

  auto operator<=>(const Slot&) const = default;
};

struct Calendar {
  std::vector<Day> days;
  std::vector<Period> periods;
  std::vector<Slot> slots;

  auto operator<=>(const Calendar&) const = default;
};

[[nodiscard]] Calendar make_calendar(std::vector<Day> days, std::vector<Period> periods);

}  // namespace schedmesh::domain
