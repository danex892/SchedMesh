#include "schedmesh/solver/smoke_solver.h"

#include <gtest/gtest.h>

TEST(SmokeSolverTest, FindsTheMinimumAllowedSlot) {
    const auto result = schedmesh::solver::solve_smoke();

    EXPECT_TRUE(result.feasible);
    EXPECT_EQ(result.selected_slot, 1);
}
