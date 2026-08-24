#include "schedmesh/import/xhstt.h"

#include <gtest/gtest.h>

#include <string>
#include <string_view>

#include "schedmesh/validation/project_validator.h"
#include "schedmesh/validation/schedule_validator.h"

namespace schedmesh::import {
namespace {

TEST(XhsttImportTest, ImportsHardAvailabilityAndPublishedSolution) {
  constexpr std::string_view archive = R"xml(
<HighSchoolTimetableArchive>
  <Instances>
    <Instance Id="synthetic-availability">
      <MetaData><Name>Synthetic availability</Name></MetaData>
      <Times>
        <TimeGroups><Day Id="monday"><Name>Monday</Name></Day></TimeGroups>
        <Time Id="monday-1"><Day Reference="monday"/></Time>
        <Time Id="monday-2"><Day Reference="monday"/></Time>
      </Times>
      <Resources>
        <Resource Id="class"><Name>Class</Name><ResourceType Reference="Class"/></Resource>
        <Resource Id="teacher"><Name>Teacher</Name><ResourceType Reference="Teacher"/></Resource>
        <Resource Id="room"><Name>Room</Name><ResourceType Reference="Room"/></Resource>
      </Resources>
      <Events>
        <EventGroups><Course Id="course"><Name>Course</Name></Course></EventGroups>
        <Event Id="lesson">
          <Name>Lesson</Name><Duration>1</Duration><Course Reference="course"/>
          <Resources>
            <Resource Reference="class"><Role>Class</Role></Resource>
            <Resource Reference="teacher"><Role>Teacher</Role></Resource>
            <Resource Reference="room"><Role>Room</Role></Resource>
          </Resources>
        </Event>
      </Events>
      <Constraints>
        <AvoidUnavailableTimesConstraint Id="unavailable">
          <Name>Unavailable</Name><Required>true</Required><Weight>1</Weight>
          <CostFunction>Linear</CostFunction>
          <AppliesTo><Resources>
            <Resource Reference="class"/><Resource Reference="teacher"/>
            <Resource Reference="room"/>
          </Resources></AppliesTo>
          <Times><Time Reference="monday-1"/></Times>
        </AvoidUnavailableTimesConstraint>
      </Constraints>
    </Instance>
  </Instances>
  <SolutionGroups>
    <SolutionGroup Id="published">
      <Solution Reference="synthetic-availability">
        <Events><Event Reference="lesson"><Time Reference="monday-2"/></Event></Events>
      </Solution>
    </SolutionGroup>
  </SolutionGroups>
</HighSchoolTimetableArchive>)xml";

  const XhsttImportResult result = import_xhstt(archive);

  ASSERT_TRUE(result.ok()) << result.error;
  ASSERT_TRUE(result.reference_schedule.has_value());
  ASSERT_EQ(result.project->teachers.size(), 1U);
  ASSERT_EQ(result.project->rooms.size(), 1U);
  ASSERT_EQ(result.project->student_groups.size(), 1U);
  EXPECT_EQ(result.project->teachers.front().unavailable_slots,
            (std::vector<domain::SlotId>{domain::SlotId{"slot-monday-period-1"}}));
  EXPECT_EQ(result.project->rooms.front().unavailable_slots,
            (std::vector<domain::SlotId>{domain::SlotId{"slot-monday-period-1"}}));
  EXPECT_EQ(result.project->student_groups.front().allowed_slots,
            (std::vector<domain::SlotId>{domain::SlotId{"slot-monday-period-2"}}));
  EXPECT_EQ(result.reference_schedule->meetings.front().start_slot,
            domain::SlotId{"slot-monday-period-2"});
  EXPECT_TRUE(validation::ProjectValidator{}.validate(*result.project).ok());
  EXPECT_TRUE(
      validation::ScheduleValidator{}.validate(*result.project, *result.reference_schedule).ok());
}

TEST(XhsttImportTest, ExpandsStudentGroupsAndAllowsResourceOnlyEventsWithoutRooms) {
  constexpr std::string_view archive = R"xml(
<HighSchoolTimetableArchive>
  <Instances>
    <Instance Id="synthetic-students">
      <MetaData><Name>Synthetic students</Name></MetaData>
      <Times>
        <TimeGroups><Day Id="monday"><Name>Monday</Name></Day></TimeGroups>
        <Time Id="monday-1"><Day Reference="monday"/></Time>
      </Times>
      <Resources>
        <Resource Id="student-a">
          <Name>Student A</Name><ResourceType Reference="Student"/>
          <ResourceGroups><ResourceGroup Reference="cohort"/></ResourceGroups>
        </Resource>
        <Resource Id="student-b">
          <Name>Student B</Name><ResourceType Reference="Student"/>
          <ResourceGroups><ResourceGroup Reference="cohort"/></ResourceGroups>
        </Resource>
        <Resource Id="teacher">
          <Name>Teacher</Name><ResourceType Reference="Teacher"/>
        </Resource>
        <Resource Id="unused-room">
          <Name>Unused room</Name><ResourceType Reference="Room"/>
        </Resource>
      </Resources>
      <Events>
        <EventGroups>
          <Course Id="course"><Name>Course</Name></Course>
          <EventGroup Id="linked"><Name>Linked events</Name></EventGroup>
        </EventGroups>
        <Event Id="lesson">
          <Name>Lesson</Name><Duration>1</Duration><Course Reference="course"/>
          <Resources><Resource Reference="teacher"><Role>Teacher</Role></Resource></Resources>
          <ResourceGroups><ResourceGroup Reference="cohort"/></ResourceGroups>
          <EventGroups><EventGroup Reference="linked"/></EventGroups>
        </Event>
        <Event Id="teacher-only">
          <Name>Teacher-only event</Name><Duration>1</Duration><Course Reference="course"/>
          <Resources><Resource Reference="teacher"><Role>Teacher</Role></Resource></Resources>
          <EventGroups><EventGroup Reference="linked"/></EventGroups>
        </Event>
      </Events>
      <Constraints>
        <LinkEventsConstraint Id="link">
          <Name>Link</Name><Required>true</Required><Weight>1</Weight>
          <CostFunction>Linear</CostFunction>
          <AppliesTo><EventGroups><EventGroup Reference="linked"/></EventGroups></AppliesTo>
        </LinkEventsConstraint>
      </Constraints>
    </Instance>
  </Instances>
</HighSchoolTimetableArchive>)xml";

  const XhsttImportResult result = import_xhstt(archive);

  ASSERT_TRUE(result.ok()) << result.error;
  ASSERT_EQ(result.project->meetings.size(), 2U);
  EXPECT_EQ(result.project->student_groups.size(), 2U);
  EXPECT_EQ(result.project->rooms.size(), 1U);
  EXPECT_EQ(result.project->meetings.front().groups.size(), 2U);
  EXPECT_EQ(result.project->meetings.front().teacher_requirements.size(), 1U);
  EXPECT_TRUE(result.project->meetings.front().room_requirements.empty());
  EXPECT_EQ(result.project->meetings.front().simultaneity_keys,
            (std::vector<std::string>{"linked"}));
  EXPECT_TRUE(result.project->meetings.back().groups.empty());
  EXPECT_TRUE(result.project->meetings.back().room_requirements.empty());
  EXPECT_EQ(result.project->meetings.back().simultaneity_keys,
            (std::vector<std::string>{"linked"}));
  EXPECT_TRUE(validation::ProjectValidator{}.validate(*result.project).ok());
}

TEST(XhsttImportTest, ImportsRequiredRoomCandidatesAndPublishedAssignment) {
  constexpr std::string_view archive = R"xml(
<HighSchoolTimetableArchive>
  <Instances>
    <Instance Id="synthetic-room-choice">
      <MetaData><Name>Synthetic room choice</Name></MetaData>
      <Times>
        <TimeGroups><Day Id="monday"><Name>Monday</Name></Day></TimeGroups>
        <Time Id="monday-1"><Day Reference="monday"/></Time>
      </Times>
      <Resources>
        <Resource Id="class"><Name>Class</Name><ResourceType Reference="Class"/></Resource>
        <Resource Id="teacher"><Name>Teacher</Name><ResourceType Reference="Teacher"/></Resource>
        <Resource Id="room-a">
          <Name>Room A</Name><ResourceType Reference="Room"/>
          <ResourceGroups><ResourceGroup Reference="preferred-rooms"/></ResourceGroups>
        </Resource>
        <Resource Id="room-b">
          <Name>Room B</Name><ResourceType Reference="Room"/>
          <ResourceGroups><ResourceGroup Reference="preferred-rooms"/></ResourceGroups>
        </Resource>
        <Resource Id="room-c"><Name>Room C</Name><ResourceType Reference="Room"/></Resource>
      </Resources>
      <Events>
        <EventGroups>
          <Course Id="course"><Name>Course</Name></Course>
          <EventGroup Id="room-events"><Name>Room events</Name></EventGroup>
        </EventGroups>
        <Event Id="lesson">
          <Name>Lesson</Name><Duration>1</Duration><Course Reference="course"/>
          <Resources>
            <Resource Reference="class"><Role>Class</Role><ResourceType Reference="Class"/></Resource>
            <Resource Reference="teacher"><Role>Teacher</Role><ResourceType Reference="Teacher"/></Resource>
            <Resource><Role>Room</Role><ResourceType Reference="Room"/></Resource>
          </Resources>
          <EventGroups><EventGroup Reference="room-events"/></EventGroups>
        </Event>
      </Events>
      <Constraints>
        <AssignResourceConstraint Id="assign-room">
          <Name>Assign room</Name><Required>true</Required><Weight>1</Weight>
          <CostFunction>Linear</CostFunction>
          <AppliesTo><EventGroups><EventGroup Reference="room-events"/></EventGroups></AppliesTo>
          <Role>Room</Role>
        </AssignResourceConstraint>
        <PreferResourcesConstraint Id="prefer-room">
          <Name>Prefer rooms</Name><Required>true</Required><Weight>1</Weight>
          <CostFunction>Linear</CostFunction>
          <AppliesTo><EventGroups><EventGroup Reference="room-events"/></EventGroups></AppliesTo>
          <ResourceGroups><ResourceGroup Reference="preferred-rooms"/></ResourceGroups>
          <Role>Room</Role>
        </PreferResourcesConstraint>
      </Constraints>
    </Instance>
  </Instances>
  <SolutionGroups>
    <SolutionGroup Id="published">
      <Solution Reference="synthetic-room-choice">
        <Events>
          <Event Reference="lesson">
            <Time Reference="monday-1"/>
            <Resources><Resource Reference="room-b"><Role>Room</Role></Resource></Resources>
          </Event>
        </Events>
      </Solution>
    </SolutionGroup>
  </SolutionGroups>
</HighSchoolTimetableArchive>)xml";

  const XhsttImportResult result = import_xhstt(archive);

  ASSERT_TRUE(result.ok()) << result.error;
  ASSERT_TRUE(result.reference_schedule.has_value());
  ASSERT_EQ(result.project->meetings.size(), 1U);
  const domain::Meeting& meeting = result.project->meetings.front();
  ASSERT_EQ(meeting.room_requirements.size(), 1U);
  EXPECT_FALSE(meeting.room_requirements.front().fixed_room.has_value());
  EXPECT_EQ(
      meeting.room_requirements.front().candidates,
      (std::vector<domain::RoomId>{domain::RoomId{"room-room-a"}, domain::RoomId{"room-room-b"}}));
  EXPECT_EQ(result.reference_schedule->meetings.front().rooms,
            (std::vector<domain::RoomId>{domain::RoomId{"room-room-b"}}));
  EXPECT_TRUE(
      validation::ScheduleValidator{}.validate(*result.project, *result.reference_schedule).ok());
}

TEST(XhsttImportTest, RejectsMalformedXmlWithoutPartialOutput) {
  const XhsttImportResult result = import_xhstt("<HighSchoolTimetableArchive>");

  EXPECT_FALSE(result.ok());
  EXPECT_FALSE(result.project.has_value());
  EXPECT_FALSE(result.error.empty());
}

}  // namespace
}  // namespace schedmesh::import
