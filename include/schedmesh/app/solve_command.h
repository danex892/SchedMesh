#pragma once

#include <iosfwd>
#include <string_view>

#include "schedmesh/solver/solve.h"

namespace schedmesh::app {

int solve_project_file(std::string_view project_path, std::string_view schedule_path,
                       const solver::SolveParameters& parameters, std::ostream& output,
                       std::ostream& errors);

}  // namespace schedmesh::app
