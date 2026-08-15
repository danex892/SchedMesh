#include "schedmesh/validation/project_validator.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <string_view>

#include "fixtures/tiny_project.h"

namespace schedmesh::validation {
namespace {

bool has_code(const ValidationResult& result, std::string_view code) {
  return std::ranges::any_of(
      result.diagnostics, [code](const Diagnostic& diagnostic) { return diagnostic.code == code; });
}

TEST(ProjectValidatorTest, AcceptsTinyProject) {
  const auto project = test::make_tiny_project();

  const ValidationResult result = ProjectValidator{}.validate(project);

  EXPECT_TRUE(result.ok());
  EXPECT_TRUE(result.diagnostics.empty());
}

TEST(ProjectValidatorTest, RejectsDuplicateStableIds) {
  auto project = test::make_tiny_project();
  project.rooms.push_back(project.rooms.front());

  const ValidationResult result = ProjectValidator{}.validate(project);

  EXPECT_FALSE(result.ok());
  EXPECT_TRUE(has_code(result, "project.duplicate_id"));
}

TEST(ProjectValidatorTest, RejectsUnknownReferencesWithJsonPath) {
  auto project = test::make_tiny_project();
  project.meetings.front().groups = {domain::StudentGroupId{"group-missing"}};

  const ValidationResult result = ProjectValidator{}.validate(project);

  ASSERT_FALSE(result.ok());
  const auto diagnostic = std::ranges::find_if(result.diagnostics, [](const Diagnostic& item) {
    return item.code == "project.unknown_reference";
  });
  ASSERT_NE(diagnostic, result.diagnostics.end());
  EXPECT_EQ(diagnostic->path, "/meetings/0/groups/0");
  EXPECT_EQ(diagnostic->entity_id, "meeting-001");
}

TEST(ProjectValidatorTest, RejectsInvalidMeetingShape) {
  auto project = test::make_tiny_project();
  auto& meeting = project.meetings.front();
  meeting.duration_in_periods = 0;
  meeting.teacher_requirements.push_back(
      {.fixed_teacher = domain::TeacherId{"teacher-001"}, .lane = 0});

  const ValidationResult result = ProjectValidator{}.validate(project);

  EXPECT_FALSE(result.ok());
  EXPECT_TRUE(has_code(result, "meeting.invalid_duration"));
  EXPECT_TRUE(has_code(result, "meeting.duplicate_lane"));
}

TEST(ProjectValidatorTest, RejectsAmbiguousResourceRequirement) {
  auto project = test::make_tiny_project();
  project.meetings.front().room_requirements.front().candidates = {domain::RoomId{"room-001"}};

  const ValidationResult result = ProjectValidator{}.validate(project);

  EXPECT_FALSE(result.ok());
  EXPECT_TRUE(has_code(result, "meeting.ambiguous_room_requirement"));
}

}  // namespace
}  // namespace schedmesh::validation
