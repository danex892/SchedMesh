#include "schedmesh/validation/room_audit.h"

#include <gtest/gtest.h>

#include "fixtures/tiny_project.h"

namespace schedmesh::validation {
namespace {

TEST(RoomAuditTest, SummarizesRoomRequirements) {
  constexpr int kRequiredCapacity = 20;
  domain::Project project = test::make_tiny_project();
  auto& requirement = project.meetings.front().room_requirements.front();
  requirement.required_features = {"laboratory"};
  requirement.minimum_capacity = kRequiredCapacity;
  project.rooms.front().features = {"laboratory"};

  const RoomAuditReport report = audit_rooms(project);

  EXPECT_EQ(report.statistics.rooms, 1U);
  EXPECT_EQ(report.statistics.meetings, 1U);
  EXPECT_EQ(report.statistics.room_lanes, 1U);
  EXPECT_EQ(report.statistics.fixed_room_lanes, 1U);
  EXPECT_EQ(report.statistics.feature_room_lanes, 1U);
  EXPECT_EQ(report.statistics.capacity_room_lanes, 1U);
  EXPECT_FALSE(report.has_proven_capacity_overload());
}

TEST(RoomAuditTest, ProvesAnAggregateRoomPoolOverload) {
  domain::Project project = test::make_tiny_project();
  constexpr int kMeetingCount = 2;
  project.meetings.front().duration_in_periods = kMeetingCount;

  const RoomAuditReport report = audit_rooms(project);

  ASSERT_EQ(report.pool_bounds.size(), 1U);
  EXPECT_EQ(report.pool_bounds.front().required_periods, kMeetingCount);
  EXPECT_EQ(report.pool_bounds.front().available_room_periods, 1);
  EXPECT_TRUE(report.has_proven_capacity_overload());
}

}  // namespace
}  // namespace schedmesh::validation
