#include "schedmesh/solver/candidate_preprocessor.h"

#include <gtest/gtest.h>

#include "fixtures/tiny_project.h"
#include "schedmesh/solver/solve.h"

namespace schedmesh::solver {
namespace {

domain::Project two_period_project() {
  domain::Project project = test::make_tiny_project();
  project.calendar =
      domain::make_calendar({{.id = "mon", .display_name = "Day 1", .ordinal = 0}},
                            {{.id = "p1", .ordinal = 0}, {.id = "p2", .ordinal = 1}});
  const std::vector allowed{domain::SlotId{"slot-mon-p1"}, domain::SlotId{"slot-mon-p2"}};
  project.student_groups.front().allowed_slots = allowed;
  project.meetings.front().allowed_start_slots = allowed;
  return project;
}

TEST(CandidatePreprocessorTest, PreservesDeterministicAllowedStartOrder) {
  const domain::Project project = two_period_project();

  const CandidatePreprocessingResult result = CandidatePreprocessor{}.preprocess(project);

  ASSERT_TRUE(result.ok());
  ASSERT_EQ(result.meetings.size(), 1U);
  ASSERT_EQ(result.meetings.front().starts.size(), 2U);
  EXPECT_EQ(result.meetings.front().starts[0].start_slot, domain::SlotId{"slot-mon-p1"});
  EXPECT_EQ(result.meetings.front().starts[1].start_slot, domain::SlotId{"slot-mon-p2"});
}

TEST(CandidatePreprocessorTest, ExpandsDurationAndDropsSessionOverflow) {
  domain::Project project = two_period_project();
  project.subjects.front().required_consecutive_periods = 2;
  project.meetings.front().duration_in_periods = 2;

  const CandidatePreprocessingResult result = CandidatePreprocessor{}.preprocess(project);

  ASSERT_TRUE(result.ok());
  ASSERT_EQ(result.meetings.front().starts.size(), 1U);
  EXPECT_EQ(result.meetings.front().starts.front().occupied_slots,
            (std::vector{domain::SlotId{"slot-mon-p1"}, domain::SlotId{"slot-mon-p2"}}));
}

TEST(CandidatePreprocessorTest, DropsForbiddenDayBoundaryStarts) {
  domain::Project project = two_period_project();
  project.subjects.front().forbid_first_period = true;

  const CandidatePreprocessingResult result = CandidatePreprocessor{}.preprocess(project);

  ASSERT_TRUE(result.ok());
  ASSERT_EQ(result.meetings.front().starts.size(), 1U);
  EXPECT_EQ(result.meetings.front().starts.front().start_slot, domain::SlotId{"slot-mon-p2"});
}

TEST(CandidatePreprocessorTest, FiltersUnavailableCandidateResourcesPerStart) {
  domain::Project project = two_period_project();
  project.teachers.push_back({.id = domain::TeacherId{"teacher-002"},
                              .display_name = "Teacher 2",
                              .qualified_subjects = {domain::SubjectId{"subject-math"}},
                              .unavailable_slots = {domain::SlotId{"slot-mon-p1"}},
                              .maximum_weekly_load = project.teachers.front().maximum_weekly_load});
  auto& teacher_requirement = project.meetings.front().teacher_requirements.front();
  teacher_requirement.fixed_teacher.reset();
  teacher_requirement.candidates = {domain::TeacherId{"teacher-001"},
                                    domain::TeacherId{"teacher-002"}};

  const CandidatePreprocessingResult result = CandidatePreprocessor{}.preprocess(project);

  ASSERT_TRUE(result.ok());
  ASSERT_EQ(result.meetings.front().starts.size(), 2U);
  EXPECT_EQ(result.meetings.front().starts[0].eligible_teachers_by_lane.front(),
            std::vector{domain::TeacherId{"teacher-001"}});
  EXPECT_EQ(result.meetings.front().starts[1].eligible_teachers_by_lane.front(),
            (std::vector{domain::TeacherId{"teacher-001"}, domain::TeacherId{"teacher-002"}}));
}

TEST(CandidatePreprocessorTest, RejectsInvalidProjectBeforeModelConstruction) {
  domain::Project project = test::make_tiny_project();
  project.meetings.front().allowed_start_slots.clear();

  const CandidatePreprocessingResult result = CandidatePreprocessor{}.preprocess(project);

  EXPECT_FALSE(result.ok());
  EXPECT_TRUE(result.meetings.empty());
}

TEST(SolveApiTest, DefaultsToDeterministicSingleWorkerExecution) {
  const SolveParameters parameters;

  EXPECT_EQ(parameters.worker_count, 1);
  EXPECT_EQ(parameters.random_seed, 1);
  EXPECT_GT(parameters.time_limit, std::chrono::milliseconds::zero());
}

}  // namespace
}  // namespace schedmesh::solver
