#include "schedmesh/io/schedule_json.h"

#include <nlohmann/json.hpp>
#include <utility>

namespace schedmesh::io {
namespace {

using Json = nlohmann::json;

template <typename Id>
Json ids_json(const std::vector<Id>& ids) {
  Json output = Json::array();
  for (const Id& id : ids) {
    output.push_back(id.value());
  }
  return output;
}

}  // namespace

std::string write_schedule_json(const domain::Schedule& schedule) {
  Json meetings = Json::array();
  for (const domain::ScheduledMeeting& meeting : schedule.meetings) {
    meetings.push_back(Json{{"meeting", meeting.meeting.value()},
                            {"rooms", ids_json(meeting.rooms)},
                            {"start_slot", meeting.start_slot.value()},
                            {"teachers", ids_json(meeting.teachers)}});
  }
  return Json{{"meetings", std::move(meetings)}}.dump(2) + '\n';
}

ScheduleReadResult read_schedule_json(std::string_view input) {
  ScheduleReadResult result;
  try {
    const Json root = Json::parse(input);
    domain::Schedule schedule;
    for (const Json& item : root.at("meetings")) {
      domain::ScheduledMeeting meeting{
          .meeting = domain::MeetingId{item.at("meeting").get<std::string>()},
          .start_slot = domain::SlotId{item.at("start_slot").get<std::string>()},
          .teachers = {},
          .rooms = {}};
      for (const Json& teacher : item.at("teachers")) {
        meeting.teachers.emplace_back(teacher.get<std::string>());
      }
      for (const Json& room : item.at("rooms")) {
        meeting.rooms.emplace_back(room.get<std::string>());
      }
      schedule.meetings.push_back(std::move(meeting));
    }
    result.schedule = std::move(schedule);
  } catch (const Json::exception& error) {
    result.error = error.what();
  }
  return result;
}

}  // namespace schedmesh::io
