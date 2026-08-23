#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <stop_token>

#include "schedmesh/domain/project.h"
#include "schedmesh/domain/schedule.h"
#include "schedmesh/validation/diagnostics.h"

namespace schedmesh::solver {

enum class SolveStatus {
  kInvalidParameters,
  kInvalidProject,
  kFeasible,
  kOptimal,
  kInfeasible,
  kTimeLimit,
  kCancelled,
  kSolverError,
};

struct SolveParameters {
  std::chrono::milliseconds time_limit{std::chrono::minutes{5}};
  int worker_count{1};
  std::int32_t random_seed{1};
};

struct SolveRequest {
  const domain::Project& project;
  SolveParameters parameters;
  std::stop_token cancellation;
};

struct SolveStatistics {
  std::chrono::milliseconds elapsed{};
  std::int64_t branches{};
  std::int64_t conflicts{};
  double best_objective{};
  double best_bound{};
};

struct SolveResult {
  SolveStatus status{SolveStatus::kSolverError};
  std::optional<domain::Schedule> schedule;
  validation::ValidationResult diagnostics;
  SolveStatistics statistics;
};

[[nodiscard]] SolveResult solve(const SolveRequest& request);

}  // namespace schedmesh::solver
