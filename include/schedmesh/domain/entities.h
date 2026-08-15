#pragma once

#include <optional>
#include <set>
#include <string>
#include <vector>

#include "schedmesh/domain/ids.h"

namespace schedmesh::domain {

struct Subject {
  SubjectId id;
  std::string display_name;
};

struct Teacher {
  TeacherId id;
  std::string display_name;
  std::vector<SubjectId> qualified_subjects;
  std::vector<SlotId> unavailable_slots;
  int maximum_weekly_load{};
  std::optional<int> maximum_daily_load;
};

struct StudentGroup {
  StudentGroupId id;
  std::string display_name;
  int grade{};
  std::vector<SlotId> allowed_slots;
};

struct Room {
  RoomId id;
  std::string display_name;
  int capacity{};
  std::set<std::string> features;
  std::vector<SlotId> unavailable_slots;
};

}  // namespace schedmesh::domain
