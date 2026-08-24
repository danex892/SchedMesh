#pragma once

#include <optional>
#include <set>
#include <string>
#include <vector>

#include "schedmesh/domain/ids.h"

namespace schedmesh::domain {

struct TeacherRequirement {
  std::optional<TeacherId> fixed_teacher;
  std::vector<TeacherId> candidates;
  int lane{};
};

struct RoomRequirement {
  std::optional<RoomId> fixed_room;
  std::vector<RoomId> candidates;
  std::set<std::string> required_features;
  int minimum_capacity{};
  int lane{};
};

struct Meeting {
  MeetingId id;
  SubjectId subject;
  std::vector<StudentGroupId> groups;
  std::vector<TeacherRequirement> teacher_requirements;
  std::vector<RoomRequirement> room_requirements;
  std::vector<SlotId> allowed_start_slots;
  int duration_in_periods{1};
  std::string distribution_key;
  std::vector<std::string> simultaneity_keys;
  bool resource_lanes_aligned{true};
};

}  // namespace schedmesh::domain
