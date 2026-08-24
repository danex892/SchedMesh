#include "schedmesh/io/schedule_json.h"

#include <gtest/gtest.h>

#include <string>

namespace schedmesh::io {
namespace {

domain::Schedule tiny_schedule() {
  return {.meetings = {{.meeting = domain::MeetingId{"meeting-001"},
                        .start_slot = domain::SlotId{"slot-mon-p1"},
                        .teachers = {domain::TeacherId{"teacher-001"}},
                        .rooms = {domain::RoomId{"room-001"}}}}};
}

TEST(ScheduleJsonTest, RoundTripsCanonicalSchedule) {
  const std::string serialized = write_schedule_json(tiny_schedule());

  const ScheduleReadResult parsed = read_schedule_json(serialized);

  ASSERT_TRUE(parsed.ok()) << parsed.error;
  ASSERT_TRUE(parsed.schedule.has_value());
  ASSERT_EQ(parsed.schedule->meetings.size(), 1U);
  const domain::ScheduledMeeting& meeting = parsed.schedule->meetings.front();
  EXPECT_EQ(meeting.meeting, domain::MeetingId{"meeting-001"});
  EXPECT_EQ(meeting.start_slot, domain::SlotId{"slot-mon-p1"});
  EXPECT_EQ(meeting.teachers, (std::vector{domain::TeacherId{"teacher-001"}}));
  EXPECT_EQ(meeting.rooms, (std::vector{domain::RoomId{"room-001"}}));
  EXPECT_EQ(write_schedule_json(*parsed.schedule), serialized);
}

TEST(ScheduleJsonTest, RejectsMalformedSchedule) {
  const ScheduleReadResult parsed = read_schedule_json(R"({"meetings": [})");

  EXPECT_FALSE(parsed.ok());
  EXPECT_FALSE(parsed.schedule.has_value());
  EXPECT_FALSE(parsed.error.empty());
}

}  // namespace
}  // namespace schedmesh::io
