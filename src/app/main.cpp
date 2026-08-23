#include <charconv>
#include <chrono>
#include <iostream>
#include <string>
#include <string_view>

#include "ortools/base/version.h"
#include "schedmesh/app/migrate_command.h"
#include "schedmesh/app/solve_command.h"
#include "schedmesh/app/validate_command.h"
#include "schedmesh/solver/smoke_solver.h"

namespace {

void print_version() {
  std::cout << "schedmesh-next " << SCHEDMESH_VERSION << '\n'
            << "OR-Tools " << operations_research::OrToolsVersionString() << '\n';
}

void print_usage() {
  std::cout << "Usage: schedmesh-next [--version|solve-smoke|validate <project.json>|"
               "migrate-legacy <settings.conf> <project.json>|solve <project.json> <schedule.json> "
               "[--time-limit-ms N] [--workers N] [--seed N]]\n";
}

bool parse_int(std::string_view input, int& value) {
  const auto [end, error] = std::from_chars(input.data(), input.data() + input.size(), value);
  return error == std::errc{} && end == input.data() + input.size();
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

  if (argc >= 4 && std::string_view{argv[1]} == "solve") {
    schedmesh::solver::SolveParameters parameters;
    for (int index = 4; index < argc; index += 2) {
      if (index + 1 >= argc) {
        print_usage();
        return 2;
      }
      int value{};
      if (!parse_int(argv[index + 1], value)) {
        print_usage();
        return 2;
      }
      const std::string_view option{argv[index]};
      if (option == "--time-limit-ms") {
        parameters.time_limit = std::chrono::milliseconds{value};
      } else if (option == "--workers") {
        parameters.worker_count = value;
      } else if (option == "--seed") {
        parameters.random_seed = value;
      } else {
        print_usage();
        return 2;
      }
    }
    return schedmesh::app::solve_project_file(argv[2], argv[3], parameters, std::cout, std::cerr);
  }

  print_usage();
  return argc == 1 ? 0 : 2;
}
