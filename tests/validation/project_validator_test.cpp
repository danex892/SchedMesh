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

TEST(ProjectValidatorTest, RejectsTeacherWithoutSubjectQualification) {
  auto project = test::make_tiny_project();
  project.teachers.front().qualified_subjects.clear();

  const ValidationResult result = ProjectValidator{}.validate(project);

  EXPECT_FALSE(result.ok());
  EXPECT_TRUE(has_code(result, "meeting.teacher_not_qualified"));
  EXPECT_TRUE(has_code(result, "meeting.no_feasible_start_slot"));
}

TEST(ProjectValidatorTest, RejectsMeetingWithoutCommonAvailableSlot) {
  auto project = test::make_tiny_project();
  const domain::SlotId only_slot = project.calendar.slots.front().id;
  project.teachers.front().unavailable_slots = {only_slot};

  const ValidationResult result = ProjectValidator{}.validate(project);

  EXPECT_FALSE(result.ok());
  EXPECT_TRUE(has_code(result, "meeting.no_feasible_start_slot"));
}

TEST(ProjectValidatorTest, AcceptsOneRemainingCandidateRoom) {
  auto project = test::make_tiny_project();
  const domain::SlotId only_slot = project.calendar.slots.front().id;
  project.rooms.push_back({.id = domain::RoomId{"room-002"},
                           .display_name = "Room 2",
                           .features = {"laboratory"},
                           .unavailable_slots = {only_slot}});
  auto& requirement = project.meetings.front().room_requirements.front();
  requirement.fixed_room.reset();
  requirement.candidates = {domain::RoomId{"room-001"}, domain::RoomId{"room-002"}};

  const ValidationResult result = ProjectValidator{}.validate(project);

  EXPECT_TRUE(result.ok());
}

TEST(ProjectValidatorTest, RejectsMissingSubgroupRoomLane) {
  auto project = test::make_tiny_project();
  project.teachers.push_back(project.teachers.front());
  project.teachers.back().id = domain::TeacherId{"teacher-002"};
  project.meetings.front().teacher_requirements.push_back(
      {.fixed_teacher = domain::TeacherId{"teacher-002"}, .lane = 1});

  const ValidationResult result = ProjectValidator{}.validate(project);

  EXPECT_FALSE(result.ok());
  EXPECT_TRUE(has_code(result, "meeting.room_lane_mismatch"));
}

TEST(ProjectValidatorTest, RejectsRoomsWithoutRequiredFeatures) {
  auto project = test::make_tiny_project();
  auto& requirement = project.meetings.front().room_requirements.front();
  requirement.required_features = {"laboratory"};

  const ValidationResult result = ProjectValidator{}.validate(project);

  EXPECT_FALSE(result.ok());
  EXPECT_TRUE(has_code(result, "meeting.no_feasible_start_slot"));
}

TEST(ProjectValidatorTest, RejectsNegativeRoomCapacityRequirement) {
  auto project = test::make_tiny_project();
  project.meetings.front().room_requirements.front().minimum_capacity = -1;

  const ValidationResult result = ProjectValidator{}.validate(project);

  EXPECT_FALSE(result.ok());
  EXPECT_TRUE(has_code(result, "meeting.negative_room_capacity"));
}

TEST(ProjectValidatorTest, RejectsMeetingWithoutLargeEnoughRoom) {
  constexpr int kRequiredCapacity = 31;
  auto project = test::make_tiny_project();
  project.meetings.front().room_requirements.front().minimum_capacity = kRequiredCapacity;

  const ValidationResult result = ProjectValidator{}.validate(project);

  EXPECT_FALSE(result.ok());
  EXPECT_TRUE(has_code(result, "meeting.no_feasible_start_slot"));
}

TEST(ProjectValidatorTest, AppliesSubjectBoundaryRestrictionsToMeetingDomain) {
  auto project = test::make_tiny_project();
  project.subjects.front().forbid_first_period = true;

  const ValidationResult result = ProjectValidator{}.validate(project);

  EXPECT_FALSE(result.ok());
  EXPECT_TRUE(has_code(result, "meeting.no_feasible_start_slot"));
}

TEST(ProjectValidatorTest, RejectsConsecutiveMeetingWithoutEnoughAvailablePeriods) {
  auto project = test::make_tiny_project();
  project.subjects.front().required_consecutive_periods = 2;
  project.meetings.front().duration_in_periods = 2;

  const ValidationResult result = ProjectValidator{}.validate(project);

  EXPECT_FALSE(result.ok());
  EXPECT_TRUE(has_code(result, "meeting.no_feasible_start_slot"));
}

TEST(ProjectValidatorTest, RejectsMeetingDurationDifferentFromSubjectPolicy) {
  auto project = test::make_tiny_project();
  project.subjects.front().required_consecutive_periods = 2;

  const ValidationResult result = ProjectValidator{}.validate(project);

  EXPECT_FALSE(result.ok());
  EXPECT_TRUE(has_code(result, "meeting.subject_duration_mismatch"));
}

TEST(ProjectValidatorTest, RejectsAsymmetricSubjectConflict) {
  auto project = test::make_tiny_project();
  project.subjects.push_back(
      {.id = domain::SubjectId{"subject-science"}, .display_name = "Subject 2"});
  project.subjects.front().conflicting_subjects = {domain::SubjectId{"subject-science"}};

  const ValidationResult result = ProjectValidator{}.validate(project);

  EXPECT_FALSE(result.ok());
  EXPECT_TRUE(has_code(result, "subject.asymmetric_conflict"));
}

}  // namespace
}  // namespace schedmesh::validation
