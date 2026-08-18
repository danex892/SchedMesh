#pragma once

#include <vector>

#include "schedmesh/domain/project.h"
#include "schedmesh/validation/diagnostics.h"

namespace schedmesh::solver {

struct CandidateStart {
  domain::SlotId start_slot;
  std::vector<domain::SlotId> occupied_slots;
  std::vector<std::vector<domain::TeacherId>> eligible_teachers_by_lane;
  std::vector<std::vector<domain::RoomId>> eligible_rooms_by_lane;
};

struct MeetingCandidates {
  domain::MeetingId meeting;
  std::vector<CandidateStart> starts;
};

struct CandidatePreprocessingResult {
  std::vector<MeetingCandidates> meetings;
  validation::ValidationResult diagnostics;

  [[nodiscard]] bool ok() const noexcept;
};

class CandidatePreprocessor {
 public:
  [[nodiscard]] CandidatePreprocessingResult preprocess(const domain::Project& project) const;
};

}  // namespace schedmesh::solver
