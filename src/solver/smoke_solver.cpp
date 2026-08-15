#include "schedmesh/solver/smoke_solver.h"

#include "ortools/sat/cp_model.h"

namespace schedmesh::solver {

SmokeResult solve_smoke() {
  namespace sat = operations_research::sat;

  sat::CpModelBuilder model;
  const sat::IntVar slot = model.NewIntVar({0, 4}).WithName("slot");
  model.AddNotEqual(slot, 0);
  model.Minimize(slot);

  sat::Model solver;
  sat::SatParameters parameters;
  parameters.set_max_time_in_seconds(1.0);
  parameters.set_num_workers(1);
  solver.Add(sat::NewSatParameters(parameters));

  const sat::CpSolverResponse response = sat::SolveCpModel(model.Build(), &solver);
  const bool feasible = response.status() == sat::CpSolverStatus::OPTIMAL ||
                        response.status() == sat::CpSolverStatus::FEASIBLE;
  return {
      .feasible = feasible,
      .selected_slot = feasible ? static_cast<int>(sat::SolutionIntegerValue(response, slot)) : -1};
}

}  // namespace schedmesh::solver
