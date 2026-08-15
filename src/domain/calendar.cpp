#include "schedmesh/domain/calendar.h"

#include <string>
#include <utility>

namespace schedmesh::domain {

Calendar make_calendar(std::vector<Day> days, std::vector<Period> periods) {
  Calendar calendar{.days = std::move(days), .periods = std::move(periods), .slots = {}};
  calendar.slots.reserve(calendar.days.size() * calendar.periods.size());

  for (std::size_t day_index = 0; day_index < calendar.days.size(); ++day_index) {
    for (std::size_t period_index = 0; period_index < calendar.periods.size(); ++period_index) {
      const std::string stable_id =
          "slot-" + calendar.days[day_index].id + "-" + calendar.periods[period_index].id;
      calendar.slots.push_back(
          Slot{.id = SlotId{stable_id}, .day_index = day_index, .period_index = period_index});
    }
  }

  return calendar;
}

}  // namespace schedmesh::domain
