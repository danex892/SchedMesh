#include "schedmesh/solver/solve.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <ranges>
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

struct WeightedVariables {
  std::vector<sat::BoolVar> variables;
  std::vector<std::int64_t> coefficients;
};

std::string occupancy_key(std::string_view resource, const domain::SlotId& slot) {
  return std::string(resource) + "@" + slot.value();
}

std::string day_key(std::string_view resource, std::size_t day) {
  return std::string(resource) + "@" + std::to_string(day);
}

std::string subject_day_key(std::string_view group, std::size_t day, std::string_view subject) {
  return std::string(group) + "@" + std::to_string(day) + "@" + std::string(subject);
}

template <typename Entity, typename EntityId>
const Entity* find_entity(const std::vector<Entity>& entities, const EntityId& id) {
  const auto found =
      std::ranges::find_if(entities, [&](const Entity& entity) { return entity.id == id; });
  return found == entities.end() ? nullptr : &*found;
}

bool requires_room_feature(const domain::Meeting& meeting, std::string_view feature) {
  return std::ranges::any_of(meeting.room_requirements, [&](const auto& requirement) {
    return requirement.required_features.contains(std::string(feature));
  });
}

bool gym_grades_compatible(const domain::Project& project, const domain::Meeting& first,
                           const domain::Meeting& second) {
  return std::ranges::all_of(first.groups, [&](const domain::StudentGroupId& first_id) {
    const domain::StudentGroup* first_group = find_entity(project.student_groups, first_id);
    return std::ranges::all_of(second.groups, [&](const domain::StudentGroupId& second_id) {
      const domain::StudentGroup* second_group = find_entity(project.student_groups, second_id);
      return first_group == nullptr || second_group == nullptr || first_group->grade <= 0 ||
             second_group->grade <= 0 || std::abs(first_group->grade - second_group->grade) <= 1;
    });
  });
}

bool overlaps(const CandidateStart& first, const CandidateStart& second) {
  return std::ranges::any_of(first.occupied_slots, [&](const domain::SlotId& slot) {
    return std::ranges::find(second.occupied_slots, slot) != second.occupied_slots.end();
  });
}

std::optional<int> daily_occurrence_limit(const domain::Subject& subject,
                                          const domain::StudentGroup& group) {
  if (subject.maximum_occurrences_per_day) {
    return subject.maximum_occurrences_per_day;
  }
  if (!group.allow_repeated_subjects_per_day) {
    return 1;
  }
  return std::nullopt;
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

void add_weighted_limit(sat::CpModelBuilder& model, const WeightedVariables& terms,
                        std::int64_t limit) {
  if (!terms.variables.empty()) {
    model.AddLessOrEqual(sat::LinearExpr::WeightedSum(terms.variables, terms.coefficients), limit);
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

validation::ValidationResult validate_parameters(const SolveParameters& parameters) {
  validation::ValidationResult result;
  if (parameters.time_limit <= std::chrono::milliseconds::zero()) {
    result.diagnostics.push_back(
        {.code = "solver.invalid_time_limit",
         .severity = validation::DiagnosticSeverity::kError,
         .path = "/parameters/time_limit",
         .message = "Solver time limit must be positive.",
         .suggested_action = "Use a deadline of at least one millisecond."});
  }
  if (parameters.worker_count <= 0) {
    result.diagnostics.push_back({.code = "solver.invalid_worker_count",
                                  .severity = validation::DiagnosticSeverity::kError,
                                  .path = "/parameters/worker_count",
                                  .message = "Solver worker count must be positive.",
                                  .suggested_action = "Use at least one worker."});
  }
  return result;
}

}  // namespace

SolveResult solve(const SolveRequest& request) {
  const auto started = std::chrono::steady_clock::now();
  SolveResult result;
  result.diagnostics = validate_parameters(request.parameters);
  if (!result.diagnostics.ok()) {
    result.status = SolveStatus::kInvalidParameters;
    return result;
  }
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
  std::unordered_map<std::string, WeightedVariables> weekly_teacher_load;
  std::unordered_map<std::string, WeightedVariables> daily_teacher_load;
  std::unordered_map<std::string, std::vector<sat::BoolVar>> group_day_subjects;
  std::unordered_map<std::string, std::vector<std::size_t>> simultaneous_meetings;

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
      const domain::Slot* start = find_entity(request.project.calendar.slots, candidate.start_slot);
      const std::size_t day = start->day_index;
      for (const domain::StudentGroupId& group : meeting.groups) {
        for (const domain::SlotId& slot : candidate.occupied_slots) {
          group_occupancy[occupancy_key(group.value(), slot)].push_back(mode);
        }
        group_day_subjects[subject_day_key(group.value(), day, meeting.subject.value())].push_back(
            mode);
      }
      for (std::size_t lane = 0; lane < candidate.eligible_teachers_by_lane.size(); ++lane) {
        mode_variables.teachers_by_lane.emplace_back();
        add_resource_choices(model, mode, candidate.eligible_teachers_by_lane[lane],
                             "teacher_" + meeting.id.value() + "_",
                             mode_variables.teachers_by_lane.back());
        register_resource_occupancy(mode_variables.teachers_by_lane.back(),
                                    candidate.occupied_slots, teacher_occupancy);
        for (const auto& choice : mode_variables.teachers_by_lane.back()) {
          auto& weekly = weekly_teacher_load[choice.resource.value()];
          weekly.variables.push_back(choice.selected);
          weekly.coefficients.push_back(meeting.duration_in_periods);
          auto& daily = daily_teacher_load[day_key(choice.resource.value(), day)];
          daily.variables.push_back(choice.selected);
          daily.coefficients.push_back(meeting.duration_in_periods);
        }
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
    for (const std::string& key : meeting.simultaneity_keys) {
      simultaneous_meetings[key].push_back(meeting_index);
    }
    variables.push_back(std::move(meeting_variables));
  }

  for (const auto& [key, meeting_indices] : simultaneous_meetings) {
    static_cast<void>(key);
    if (meeting_indices.size() < 2) {
      continue;
    }
    const std::size_t first_index = meeting_indices.front();
    for (const std::size_t linked_index : meeting_indices | std::views::drop(1)) {
      for (std::size_t first_mode = 0; first_mode < variables[first_index].modes.size();
           ++first_mode) {
        const domain::SlotId& start =
            variables[first_index].candidates->starts[first_mode].start_slot;
        const auto linked = std::ranges::find_if(
            variables[linked_index].candidates->starts,
            [&](const CandidateStart& candidate) { return candidate.start_slot == start; });
        if (linked == variables[linked_index].candidates->starts.end()) {
          model.AddEquality(variables[first_index].modes[first_mode].selected, 0);
        } else {
          const std::size_t linked_mode =
              static_cast<std::size_t>(linked - variables[linked_index].candidates->starts.begin());
          model.AddEquality(variables[first_index].modes[first_mode].selected,
                            variables[linked_index].modes[linked_mode].selected);
        }
      }
      for (std::size_t linked_mode = 0; linked_mode < variables[linked_index].modes.size();
           ++linked_mode) {
        const domain::SlotId& start =
            variables[linked_index].candidates->starts[linked_mode].start_slot;
        const bool present_in_first = std::ranges::any_of(
            variables[first_index].candidates->starts,
            [&](const CandidateStart& candidate) { return candidate.start_slot == start; });
        if (!present_in_first) {
          model.AddEquality(variables[linked_index].modes[linked_mode].selected, 0);
        }
      }
    }
  }

  add_no_overlap(model, group_occupancy);
  add_no_overlap(model, teacher_occupancy);
  add_no_overlap(model, room_occupancy);

  for (std::size_t first_index = 0; first_index < variables.size(); ++first_index) {
    const domain::Meeting& first = request.project.meetings[first_index];
    if (!requires_room_feature(first, "gym")) {
      continue;
    }
    for (std::size_t second_index = first_index + 1; second_index < variables.size();
         ++second_index) {
      const domain::Meeting& second = request.project.meetings[second_index];
      if (!requires_room_feature(second, "gym") ||
          gym_grades_compatible(request.project, first, second)) {
        continue;
      }
      for (std::size_t first_mode = 0; first_mode < variables[first_index].modes.size();
           ++first_mode) {
        for (std::size_t second_mode = 0; second_mode < variables[second_index].modes.size();
             ++second_mode) {
          if (overlaps(variables[first_index].candidates->starts[first_mode],
                       variables[second_index].candidates->starts[second_mode])) {
            model.AddLessOrEqual(
                sat::LinearExpr::Sum({variables[first_index].modes[first_mode].selected,
                                      variables[second_index].modes[second_mode].selected}),
                1);
          }
        }
      }
    }
  }

  for (const domain::Teacher& teacher : request.project.teachers) {
    add_weighted_limit(model, weekly_teacher_load[teacher.id.value()], teacher.maximum_weekly_load);
    if (teacher.maximum_daily_load) {
      for (std::size_t day = 0; day < request.project.calendar.days.size(); ++day) {
        add_weighted_limit(model, daily_teacher_load[day_key(teacher.id.value(), day)],
                           *teacher.maximum_daily_load);
      }
    }
  }
  for (const domain::StudentGroup& group : request.project.student_groups) {
    for (std::size_t day = 0; day < request.project.calendar.days.size(); ++day) {
      for (const domain::Subject& subject : request.project.subjects) {
        const auto& occurrences =
            group_day_subjects[subject_day_key(group.id.value(), day, subject.id.value())];
        const std::optional<int> occurrence_limit = daily_occurrence_limit(subject, group);
        if (occurrence_limit && occurrences.size() > static_cast<std::size_t>(*occurrence_limit)) {
          model.AddLessOrEqual(sat::LinearExpr::Sum(occurrences), *occurrence_limit);
        }
        for (const domain::SubjectId& conflict_id : subject.conflicting_subjects) {
          if (subject.id.value() >= conflict_id.value()) {
            continue;
          }
          const auto& conflicts =
              group_day_subjects[subject_day_key(group.id.value(), day, conflict_id.value())];
          for (const sat::BoolVar& occurrence : occurrences) {
            for (const sat::BoolVar& conflict : conflicts) {
              model.AddLessOrEqual(sat::LinearExpr::Sum({occurrence, conflict}), 1);
            }
          }
        }
      }
    }
  }

  sat::SatParameters parameters;
  parameters.set_max_time_in_seconds(
      std::chrono::duration<double>(request.parameters.time_limit).count());
  parameters.set_num_search_workers(request.parameters.worker_count);
  parameters.set_random_seed(request.parameters.random_seed);
  sat::Model solver;
  solver.Add(sat::NewSatParameters(parameters));
  std::stop_callback cancellation_callback(request.cancellation,
                                           [&solver] { sat::StopSearch(&solver); });
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
