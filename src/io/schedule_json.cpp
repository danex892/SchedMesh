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

}  // namespace schedmesh::io
