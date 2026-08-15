#include <gtest/gtest.h>

#include <chrono>
#include <type_traits>
#include <unordered_set>

#include "schedmesh/domain/calendar.h"
#include "schedmesh/domain/entities.h"
#include "schedmesh/domain/ids.h"
#include "schedmesh/domain/meeting.h"
#include "schedmesh/domain/project.h"

namespace schedmesh::domain {
namespace {

static_assert(!std::is_convertible_v<std::string, TeacherId>);
static_assert(!std::is_same_v<TeacherId, RoomId>);

TEST(StrongIdTest, PreservesStableStringAndSupportsHashing) {
  const TeacherId teacher{"teacher-0042"};
  std::unordered_set<TeacherId> ids{teacher};

  EXPECT_EQ(teacher.value(), "teacher-0042");
  EXPECT_TRUE(ids.contains(TeacherId{"teacher-0042"}));
  EXPECT_FALSE(ids.contains(TeacherId{"teacher-0043"}));
}

TEST(CalendarTest, ExpandsDaysAndPeriodsIntoStableGlobalSlots) {
  using namespace std::chrono_literals;
  const Calendar calendar =
      make_calendar({{.id = "mon", .display_name = "Day 1", .ordinal = 0},
                     {.id = "tue", .display_name = "Day 2", .ordinal = 1}},
                    {{.id = "p1", .ordinal = 0, .start_time = 8h, .end_time = 8h + 40min},
                     {.id = "p2", .ordinal = 1, .start_time = 8h + 50min, .end_time = 9h + 30min}});

  ASSERT_EQ(calendar.slots.size(), 4U);
  EXPECT_EQ(calendar.slots[0],
            (Slot{.id = SlotId{"slot-mon-p1"}, .day_index = 0, .period_index = 0}));
  EXPECT_EQ(calendar.slots[3],
            (Slot{.id = SlotId{"slot-tue-p2"}, .day_index = 1, .period_index = 1}));
}

TEST(ResourceTest, RepresentsGymAsRoomFeature) {
  const Room gym{
      .id = RoomId{"room-gym-a"}, .display_name = "Gym A", .capacity = 60, .features = {"gym"}};

  EXPECT_TRUE(gym.features.contains("gym"));
}

TEST(MeetingTest, KeepsSimultaneousLanesInOneEvent) {
  const Meeting language_lesson{
      .id = MeetingId{"meeting-language-01"},
      .subject = SubjectId{"subject-language"},
      .groups = {StudentGroupId{"group-05-a"}},
      .teacher_requirements = {{.fixed_teacher = TeacherId{"teacher-a"}, .lane = 0},
                               {.fixed_teacher = TeacherId{"teacher-b"}, .lane = 1}},
      .room_requirements =
          {{.candidates = {RoomId{"room-101"}}, .required_features = {"language"}, .lane = 0},
           {.candidates = {RoomId{"room-102"}}, .required_features = {"language"}, .lane = 1}},
      .allowed_start_slots = {SlotId{"slot-mon-p1"}},
      .distribution_key = "language-05-a"};

  EXPECT_EQ(language_lesson.teacher_requirements.size(), 2U);
  EXPECT_EQ(language_lesson.room_requirements.size(), 2U);
  EXPECT_EQ(language_lesson.allowed_start_slots.size(), 1U);
}

TEST(ProjectTest, DefaultsToCurrentSchemaVersion) {
  const Project project{};

  EXPECT_EQ(project.schema_version, kCurrentSchemaVersion);
}

}  // namespace
}  // namespace schedmesh::domain
