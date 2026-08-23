#include "schedmesh/app/solve_command.h"

#include <fstream>
#include <iterator>
#include <ostream>
#include <string>
#include <utility>

#include "schedmesh/app/validate_command.h"
#include "schedmesh/io/project_json.h"
#include "schedmesh/io/schedule_json.h"

namespace schedmesh::app {
namespace {

std::string_view status_name(solver::SolveStatus status) {
  switch (status) {
    case solver::SolveStatus::kInvalidParameters:
      return "invalid_parameters";
    case solver::SolveStatus::kInvalidProject:
      return "invalid_project";
    case solver::SolveStatus::kFeasible:
      return "feasible";
    case solver::SolveStatus::kOptimal:
      return "optimal";
    case solver::SolveStatus::kInfeasible:
      return "infeasible";
    case solver::SolveStatus::kTimeLimit:
      return "time_limit";
    case solver::SolveStatus::kCancelled:
      return "cancelled";
    case solver::SolveStatus::kSolverError:
      return "solver_error";
  }
  return "solver_error";
}

void print_diagnostics(const validation::ValidationResult& validation, std::ostream& errors) {
  for (const validation::Diagnostic& diagnostic : validation.diagnostics) {
    errors << (diagnostic.severity == validation::DiagnosticSeverity::kError ? "error" : "warning")
           << ' ' << diagnostic.code << ' ' << diagnostic.path << ": " << diagnostic.message
           << '\n';
  }
}

}  // namespace

int solve_project_file(std::string_view project_path, std::string_view schedule_path,
                       const solver::SolveParameters& parameters, std::ostream& output,
                       std::ostream& errors) {
  std::ifstream input(std::string{project_path}, std::ios::binary);
  if (!input) {
    errors << "error io.open_failed /: Cannot open project file: " << project_path << '\n';
    return kExitUsageOrIoError;
  }
  const std::string contents{std::istreambuf_iterator<char>{input},
                             std::istreambuf_iterator<char>{}};
  io::ProjectReadResult project = io::read_project_json(contents);
  print_diagnostics(project.validation, errors);
  if (!project.ok()) {
    return kExitValidationError;
  }

  solver::SolveResult result =
      solver::solve({.project = *project.project, .parameters = parameters});
  print_diagnostics(result.diagnostics, errors);
  output << "status=" << status_name(result.status)
         << " elapsed_ms=" << result.statistics.elapsed.count()
         << " branches=" << result.statistics.branches
         << " conflicts=" << result.statistics.conflicts << '\n';
  if (!result.schedule) {
    return result.status == solver::SolveStatus::kInfeasible ? kExitValidationError
                                                             : kExitUsageOrIoError;
  }

  std::ofstream schedule_output(std::string{schedule_path}, std::ios::binary | std::ios::trunc);
  if (!schedule_output) {
    errors << "error io.write_failed /: Cannot create schedule file: " << schedule_path << '\n';
    return kExitUsageOrIoError;
  }
  schedule_output << io::write_schedule_json(*result.schedule);
  if (!schedule_output) {
    errors << "error io.write_failed /: Cannot write schedule file: " << schedule_path << '\n';
    return kExitUsageOrIoError;
  }
  return kExitSuccess;
}

}  // namespace schedmesh::app
