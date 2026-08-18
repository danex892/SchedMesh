#include "schedmesh/solver/solve.h"

#include <algorithm>
#include <chrono>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ortools/sat/cp_model.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "schedmesh/solver/candidate_preprocessor.h"
#include "schedmesh/validation/schedule_validator.h"

namespace schedmesh::solver {
namespace {

namespace sat = operations_research::sat;

template <typename ResourceId>
struct ResourceChoice {
  ResourceId resource;
  sat::BoolVar selected;
};

struct ModeVariables {
  sat::BoolVar selected;
  std::vector<std::vector<ResourceChoice<domain::TeacherId>>> teachers_by_lane;
  std::vector<std::vector<ResourceChoice<domain::RoomId>>> rooms_by_lane;
};

struct MeetingVariables {
  const MeetingCandidates* candidates{};
  std::vector<ModeVariables> modes;
};

std::string occupancy_key(std::string_view resource, const domain::SlotId& slot) {
  return std::string(resource) + "@" + slot.value();
}

template <typename Id>
void add_resource_choices(sat::CpModelBuilder& model, const sat::BoolVar& mode,
                          const std::vector<Id>& resources, std::string_view prefix,
                          std::vector<ResourceChoice<Id>>& choices) {
  std::vector<sat::BoolVar> selection;
  selection.reserve(resources.size());
  choices.reserve(resources.size());
  for (const Id& resource : resources) {
    sat::BoolVar selected = model.NewBoolVar().WithName(std::string(prefix) + resource.value());
    selection.push_back(selected);
    choices.push_back({.resource = resource, .selected = selected});
  }
  model.AddEquality(sat::LinearExpr::Sum(selection), mode);
}

template <typename Id>
void register_resource_occupancy(
    const std::vector<ResourceChoice<Id>>& choices,
    const std::vector<domain::SlotId>& occupied_slots,
    std::unordered_map<std::string, std::vector<sat::BoolVar>>& occupancy) {
  for (const auto& choice : choices) {
    for (const domain::SlotId& slot : occupied_slots) {
      occupancy[occupancy_key(choice.resource.value(), slot)].push_back(choice.selected);
    }
  }
}

void add_no_overlap(sat::CpModelBuilder& model,
                    const std::unordered_map<std::string, std::vector<sat::BoolVar>>& occupancy) {
  for (const auto& [key, variables] : occupancy) {
    static_cast<void>(key);
    if (variables.size() > 1) {
      model.AddLessOrEqual(sat::LinearExpr::Sum(variables), 1);
    }
  }
}

template <typename Id>
std::vector<Id> selected_resources(const std::vector<std::vector<ResourceChoice<Id>>>& lanes,
                                   const sat::CpSolverResponse& response) {
  std::vector<Id> resources;
  resources.reserve(lanes.size());
  for (const auto& lane : lanes) {
    const auto selected = std::ranges::find_if(lane, [&](const ResourceChoice<Id>& choice) {
      return sat::SolutionBooleanValue(response, choice.selected);
    });
    if (selected != lane.end()) {
      resources.push_back(selected->resource);
    }
  }
  return resources;
}

SolveStatus map_status(sat::CpSolverStatus status, bool cancelled) {
  if (cancelled) {
    return SolveStatus::kCancelled;
  }
  switch (status) {
    case sat::CpSolverStatus::OPTIMAL:
      return SolveStatus::kOptimal;
    case sat::CpSolverStatus::FEASIBLE:
      return SolveStatus::kFeasible;
    case sat::CpSolverStatus::INFEASIBLE:
      return SolveStatus::kInfeasible;
    case sat::CpSolverStatus::UNKNOWN:
      return SolveStatus::kTimeLimit;
    case sat::CpSolverStatus::MODEL_INVALID:
    default:
      return SolveStatus::kSolverError;
  }
}

}  // namespace

SolveResult solve(const SolveRequest& request) {
  const auto started = std::chrono::steady_clock::now();
  SolveResult result;
  CandidatePreprocessingResult candidates = CandidatePreprocessor{}.preprocess(request.project);
  if (!candidates.ok()) {
    result.status = SolveStatus::kInvalidProject;
    result.diagnostics = std::move(candidates.diagnostics);
    return result;
  }
  if (request.cancellation.stop_requested()) {
    result.status = SolveStatus::kCancelled;
    return result;
  }

  sat::CpModelBuilder model;
  std::vector<MeetingVariables> variables;
  variables.reserve(candidates.meetings.size());
  std::unordered_map<std::string, std::vector<sat::BoolVar>> group_occupancy;
  std::unordered_map<std::string, std::vector<sat::BoolVar>> teacher_occupancy;
  std::unordered_map<std::string, std::vector<sat::BoolVar>> room_occupancy;

  for (std::size_t meeting_index = 0; meeting_index < candidates.meetings.size(); ++meeting_index) {
    const MeetingCandidates& meeting_candidates = candidates.meetings[meeting_index];
    const domain::Meeting& meeting = request.project.meetings[meeting_index];
    MeetingVariables meeting_variables{.candidates = &meeting_candidates, .modes = {}};
    std::vector<sat::BoolVar> mode_selection;
    for (std::size_t mode_index = 0; mode_index < meeting_candidates.starts.size(); ++mode_index) {
      const CandidateStart& candidate = meeting_candidates.starts[mode_index];
      sat::BoolVar mode = model.NewBoolVar().WithName("meeting_" + meeting.id.value() + "_mode_" +
                                                      std::to_string(mode_index));
      ModeVariables mode_variables{.selected = mode, .teachers_by_lane = {}, .rooms_by_lane = {}};
      mode_selection.push_back(mode);
      for (const domain::StudentGroupId& group : meeting.groups) {
        for (const domain::SlotId& slot : candidate.occupied_slots) {
          group_occupancy[occupancy_key(group.value(), slot)].push_back(mode);
        }
      }
      for (std::size_t lane = 0; lane < candidate.eligible_teachers_by_lane.size(); ++lane) {
        mode_variables.teachers_by_lane.emplace_back();
        add_resource_choices(model, mode, candidate.eligible_teachers_by_lane[lane],
                             "teacher_" + meeting.id.value() + "_",
                             mode_variables.teachers_by_lane.back());
        register_resource_occupancy(mode_variables.teachers_by_lane.back(),
                                    candidate.occupied_slots, teacher_occupancy);
      }
      for (std::size_t lane = 0; lane < candidate.eligible_rooms_by_lane.size(); ++lane) {
        mode_variables.rooms_by_lane.emplace_back();
        add_resource_choices(model, mode, candidate.eligible_rooms_by_lane[lane],
                             "room_" + meeting.id.value() + "_",
                             mode_variables.rooms_by_lane.back());
        register_resource_occupancy(mode_variables.rooms_by_lane.back(), candidate.occupied_slots,
                                    room_occupancy);
      }
      meeting_variables.modes.push_back(std::move(mode_variables));
    }
    model.AddExactlyOne(mode_selection);
    variables.push_back(std::move(meeting_variables));
  }

  add_no_overlap(model, group_occupancy);
  add_no_overlap(model, teacher_occupancy);
  add_no_overlap(model, room_occupancy);

  sat::SatParameters parameters;
  parameters.set_max_time_in_seconds(
      std::chrono::duration<double>(request.parameters.time_limit).count());
  parameters.set_num_search_workers(request.parameters.worker_count);
  parameters.set_random_seed(request.parameters.random_seed);
  sat::Model solver;
  solver.Add(sat::NewSatParameters(parameters));
  const sat::CpSolverResponse response = sat::SolveCpModel(model.Build(), &solver);
  result.status = map_status(response.status(), request.cancellation.stop_requested());
  result.statistics.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - started);
  result.statistics.branches = response.num_branches();
  result.statistics.conflicts = response.num_conflicts();
  result.statistics.best_objective = response.objective_value();
  result.statistics.best_bound = response.best_objective_bound();

  if (result.status != SolveStatus::kOptimal && result.status != SolveStatus::kFeasible) {
    return result;
  }
  domain::Schedule schedule;
  schedule.meetings.reserve(variables.size());
  for (const MeetingVariables& meeting : variables) {
    for (std::size_t index = 0; index < meeting.modes.size(); ++index) {
      const ModeVariables& mode = meeting.modes[index];
      if (sat::SolutionBooleanValue(response, mode.selected)) {
        schedule.meetings.push_back(
            {.meeting = meeting.candidates->meeting,
             .start_slot = meeting.candidates->starts[index].start_slot,
             .teachers = selected_resources(mode.teachers_by_lane, response),
             .rooms = selected_resources(mode.rooms_by_lane, response)});
        break;
      }
    }
  }
  result.diagnostics = validation::ScheduleValidator{}.validate(request.project, schedule);
  if (!result.diagnostics.ok()) {
    result.status = SolveStatus::kSolverError;
    return result;
  }
  result.schedule = std::move(schedule);
  return result;
}

}  // namespace schedmesh::solver
