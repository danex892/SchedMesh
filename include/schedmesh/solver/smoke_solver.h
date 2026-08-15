#pragma once

namespace schedmesh::solver {

struct SmokeResult {
  bool feasible;
  int selected_slot;
};

[[nodiscard]] SmokeResult solve_smoke();

}  // namespace schedmesh::solver
