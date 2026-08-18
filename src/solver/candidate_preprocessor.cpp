#include "schedmesh/solver/candidate_preprocessor.h"

#include <algorithm>
#include <set>
#include <string>
#include <utility>

#include "schedmesh/validation/project_validator.h"

namespace schedmesh::solver {
namespace {

template <typename Entity, typename EntityId>
const Entity* find_entity(const std::vector<Entity>& entities, const EntityId& id) {
  const auto found =
      std::ranges::find_if(entities, [&](const Entity& entity) { return entity.id == id; });
  return found == entities.end() ? nullptr : &*found;
}

template <typename Value>
bool contains(const std::vector<Value>& values, const Value& value) {
  return std::ranges::find(values, value) != values.end();
}

bool has_required_features(const domain::Room& room,
                           const std::set<std::string>& required_features) {
  return std::ranges::all_of(required_features, [&](const std::string& feature) {
    return room.features.contains(feature);
  });
}

std::vector<domain::SlotId> occupied_slots(const domain::Project& project,
                                           const domain::Meeting& meeting,
                                           const domain::Slot& start) {
  std::vector<domain::SlotId> result;
  result.reserve(static_cast<std::size_t>(meeting.duration_in_periods));
  for (int offset = 0; offset < meeting.duration_in_periods; ++offset) {
    const std::size_t period = start.period_index + static_cast<std::size_t>(offset);
    const auto slot = std::ranges::find_if(project.calendar.slots, [&](const domain::Slot& item) {
      return item.day_index == start.day_index && item.period_index == period;
    });
    if (slot == project.calendar.slots.end()) {
      return {};
    }
    result.push_back(slot->id);
  }
  return result;
}

bool groups_allow(const domain::Project& project, const domain::Meeting& meeting,
                  const std::vector<domain::SlotId>& slots) {
  return std::ranges::all_of(meeting.groups, [&](const domain::StudentGroupId& group_id) {
    const domain::StudentGroup* group = find_entity(project.student_groups, group_id);
    return group != nullptr && std::ranges::all_of(slots, [&](const domain::SlotId& slot) {
             return contains(group->allowed_slots, slot);
           });
  });
}

bool subject_allows_start(const domain::Project& project, const domain::Meeting& meeting,
                          const domain::Slot& start) {
  const domain::Subject* subject = find_entity(project.subjects, meeting.subject);
  if (subject == nullptr || (!subject->forbid_first_period && !subject->forbid_last_period)) {
    return subject != nullptr;
  }
  return std::ranges::all_of(meeting.groups, [&](const domain::StudentGroupId& group_id) {
    const domain::StudentGroup* group = find_entity(project.student_groups, group_id);
    if (group == nullptr) {
      return false;
    }
    std::vector<std::size_t> periods;
    for (const domain::SlotId& slot_id : group->allowed_slots) {
      const domain::Slot* slot = find_entity(project.calendar.slots, slot_id);
      if (slot != nullptr && slot->day_index == start.day_index) {
        periods.push_back(slot->period_index);
      }
    }
    if (periods.empty()) {
      return false;
    }
    const auto [first, last] = std::ranges::minmax_element(periods);
    return (!subject->forbid_first_period || start.period_index != *first) &&
           (!subject->forbid_last_period || start.period_index != *last);
  });
}

std::vector<domain::TeacherId> eligible_teachers(const domain::Project& project,
                                                 const domain::Meeting& meeting,
                                                 const domain::TeacherRequirement& requirement,
                                                 const std::vector<domain::SlotId>& slots) {
  std::vector<domain::TeacherId> candidates = requirement.candidates;
  if (requirement.fixed_teacher) {
    candidates = {*requirement.fixed_teacher};
  }
  std::erase_if(candidates, [&](const domain::TeacherId& teacher_id) {
    const domain::Teacher* teacher = find_entity(project.teachers, teacher_id);
    return teacher == nullptr || !contains(teacher->qualified_subjects, meeting.subject) ||
           std::ranges::any_of(slots, [&](const domain::SlotId& slot) {
             return contains(teacher->unavailable_slots, slot);
           });
  });
  return candidates;
}

std::vector<domain::RoomId> eligible_rooms(const domain::Project& project,
                                           const domain::RoomRequirement& requirement,
                                           const std::vector<domain::SlotId>& slots) {
  std::vector<domain::RoomId> candidates = requirement.candidates;
  if (requirement.fixed_room) {
    candidates = {*requirement.fixed_room};
  }
  std::erase_if(candidates, [&](const domain::RoomId& room_id) {
    const domain::Room* room = find_entity(project.rooms, room_id);
    return room == nullptr || !has_required_features(*room, requirement.required_features) ||
           std::ranges::any_of(slots, [&](const domain::SlotId& slot) {
             return contains(room->unavailable_slots, slot);
           });
  });
  return candidates;
}

}  // namespace

bool CandidatePreprocessingResult::ok() const noexcept { return diagnostics.ok(); }

CandidatePreprocessingResult CandidatePreprocessor::preprocess(
    const domain::Project& project) const {
  CandidatePreprocessingResult result;
  result.diagnostics = validation::ProjectValidator{}.validate(project);
  if (!result.diagnostics.ok()) {
    return result;
  }

  result.meetings.reserve(project.meetings.size());
  for (const domain::Meeting& meeting : project.meetings) {
    MeetingCandidates meeting_candidates{.meeting = meeting.id, .starts = {}};
    for (const domain::SlotId& start_id : meeting.allowed_start_slots) {
      const domain::Slot* start = find_entity(project.calendar.slots, start_id);
      if (start == nullptr) {
        continue;
      }
      CandidateStart candidate{.start_slot = start_id,
                               .occupied_slots = occupied_slots(project, meeting, *start),
                               .eligible_teachers_by_lane = {},
                               .eligible_rooms_by_lane = {}};
      if (!subject_allows_start(project, meeting, *start) || candidate.occupied_slots.empty() ||
          !groups_allow(project, meeting, candidate.occupied_slots)) {
        continue;
      }
      for (const domain::TeacherRequirement& requirement : meeting.teacher_requirements) {
        candidate.eligible_teachers_by_lane.push_back(
            eligible_teachers(project, meeting, requirement, candidate.occupied_slots));
      }
      for (const domain::RoomRequirement& requirement : meeting.room_requirements) {
        candidate.eligible_rooms_by_lane.push_back(
            eligible_rooms(project, requirement, candidate.occupied_slots));
      }
      const bool empty_teacher_lane = std::ranges::any_of(
          candidate.eligible_teachers_by_lane, [](const auto& lane) { return lane.empty(); });
      const bool empty_room_lane = std::ranges::any_of(
          candidate.eligible_rooms_by_lane, [](const auto& lane) { return lane.empty(); });
      if (!empty_teacher_lane && !empty_room_lane) {
        meeting_candidates.starts.push_back(std::move(candidate));
      }
    }
    result.meetings.push_back(std::move(meeting_candidates));
  }
  return result;
}

}  // namespace schedmesh::solver
