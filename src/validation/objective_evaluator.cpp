#include "schedmesh/validation/objective_evaluator.h"

#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace schedmesh::validation {
namespace {

template <typename Entity, typename EntityId>
const Entity* find_entity(const std::vector<Entity>& entities, const EntityId& id) {
  const auto found =
      std::ranges::find_if(entities, [&](const Entity& entity) { return entity.id == id; });
  return found == entities.end() ? nullptr : &*found;
}

using DailyPeriods = std::map<std::pair<std::string, std::size_t>, std::vector<std::size_t>>;

std::int64_t idle_periods(DailyPeriods periods_by_resource) {
  std::int64_t result{};
  for (auto& [key, periods] : periods_by_resource) {
    static_cast<void>(key);
    std::ranges::sort(periods);
    const auto unique_end = std::ranges::unique(periods).begin();
    periods.erase(unique_end, periods.end());
    if (periods.size() > 1) {
      result += static_cast<std::int64_t>(periods.back() - periods.front() + 1 - periods.size());
    }
  }
  return result;
}

}  // namespace

std::int64_t ObjectiveBreakdown::total() const noexcept {
  return teacher_idle_periods + group_idle_periods + late_period_load + last_day_load;
}

ObjectiveBreakdown ObjectiveEvaluator::evaluate(const domain::Project& project,
                                                const domain::Schedule& schedule) const {
  ObjectiveBreakdown result;
  DailyPeriods teacher_periods;
  DailyPeriods group_periods;
  for (const domain::ScheduledMeeting& assignment : schedule.meetings) {
    const domain::Meeting* meeting = find_entity(project.meetings, assignment.meeting);
    const domain::Slot* start = find_entity(project.calendar.slots, assignment.start_slot);
    if (meeting == nullptr || start == nullptr) {
      continue;
    }
    for (int offset = 0; offset < meeting->duration_in_periods; ++offset) {
      const std::size_t period = start->period_index + static_cast<std::size_t>(offset);
      result.late_period_load += static_cast<std::int64_t>(period);
      if (project.preferences.minimize_last_day_load &&
          start->day_index + 1 == project.calendar.days.size()) {
        ++result.last_day_load;
      }
      for (const domain::TeacherId& teacher : assignment.teachers) {
        teacher_periods[{teacher.value(), start->day_index}].push_back(period);
      }
      for (const domain::StudentGroupId& group : meeting->groups) {
        group_periods[{group.value(), start->day_index}].push_back(period);
      }
    }
  }
  result.teacher_idle_periods = idle_periods(std::move(teacher_periods));
  result.group_idle_periods = idle_periods(std::move(group_periods));
  return result;
}

}  // namespace schedmesh::validation
