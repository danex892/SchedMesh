#include <iostream>
#include <string_view>

#include "ortools/base/version.h"
#include "schedmesh/app/migrate_command.h"
#include "schedmesh/app/validate_command.h"
#include "schedmesh/solver/smoke_solver.h"

namespace {

void print_version() {
  std::cout << "schedmesh-next " << SCHEDMESH_VERSION << '\n'
            << "OR-Tools " << operations_research::OrToolsVersionString() << '\n';
}

void print_usage() {
  std::cout << "Usage: schedmesh-next [--version|solve-smoke|validate <project.json>|"
               "migrate-legacy <settings.conf> <project.json>]\n";
}

}  // namespace

int main(int argc, char* argv[]) {
  if (argc == 2 && std::string_view{argv[1]} == "--version") {
    print_version();
    return 0;
  }

  if (argc == 2 && std::string_view{argv[1]} == "solve-smoke") {
    const auto result = schedmesh::solver::solve_smoke();
    if (!result.feasible) {
      std::cerr << "CP-SAT smoke model did not produce a feasible solution.\n";
      return 1;
    }
    std::cout << "CP-SAT smoke model solved; selected slot: " << result.selected_slot << '\n';
    return 0;
  }

  if (argc == 3 && std::string_view{argv[1]} == "validate") {
    return schedmesh::app::validate_project_file(argv[2], std::cout, std::cerr);
  }

  if (argc == 4 && std::string_view{argv[1]} == "migrate-legacy") {
    return schedmesh::app::migrate_legacy_project(argv[2], argv[3], std::cout, std::cerr);
  }

  print_usage();
  return argc == 1 ? 0 : 2;
}
