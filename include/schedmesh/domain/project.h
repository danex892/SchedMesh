#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "schedmesh/domain/calendar.h"
#include "schedmesh/domain/entities.h"
#include "schedmesh/domain/meeting.h"

namespace schedmesh::domain {

inline constexpr std::uint32_t kCurrentSchemaVersion = 1;

struct ProjectMetadata {
  std::string id;
  std::string display_name;
};

struct Project {
  std::uint32_t schema_version{kCurrentSchemaVersion};
  ProjectMetadata metadata;
  Calendar calendar;
  std::vector<Subject> subjects;
  std::vector<Teacher> teachers;
  std::vector<StudentGroup> student_groups;
  std::vector<Room> rooms;
  std::vector<Meeting> meetings;
};

}  // namespace schedmesh::domain
