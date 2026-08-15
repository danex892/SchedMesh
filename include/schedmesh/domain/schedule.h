#pragma once

#include <vector>

#include "schedmesh/domain/ids.h"

namespace schedmesh::domain {

struct ScheduledMeeting {
  MeetingId meeting;
  SlotId start_slot;
  std::vector<TeacherId> teachers;
  std::vector<RoomId> rooms;
};

struct Schedule {
  std::vector<ScheduledMeeting> meetings;
};

}  // namespace schedmesh::domain
