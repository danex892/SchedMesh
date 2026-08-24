#include "schedmesh/import/xhstt.h"

#include <algorithm>
#include <map>
#include <pugixml.hpp>
#include <set>
#include <string>
#include <utility>

#include "schedmesh/domain/calendar.h"

namespace schedmesh::import {
namespace {

std::string attribute(pugi::xml_node node, const char* name) {
  return node.attribute(name).as_string();
}

std::string child_text(pugi::xml_node node, const char* name) {
  return node.child(name).text().as_string();
}

template <typename Id>
std::vector<Id> all_slot_ids(const domain::Project& project) {
  std::vector<Id> result;
  result.reserve(project.calendar.slots.size());
  for (const domain::Slot& slot : project.calendar.slots) {
    result.emplace_back(slot.id.value());
  }
  return result;
}

std::vector<domain::SlotId> allowed_starts(const domain::Project& project, int duration) {
  std::vector<domain::SlotId> result;
  for (const domain::Slot& slot : project.calendar.slots) {
    if (slot.period_index + static_cast<std::size_t>(duration) <= project.calendar.periods.size()) {
      result.push_back(slot.id);
    }
  }
  return result;
}

struct ResourceRule {
  std::string role;
  std::set<std::string, std::less<>> event_groups;
  std::set<std::string, std::less<>> events;
  std::vector<std::string> resources;
};

ResourceRule read_resource_rule(pugi::xml_node constraint) {
  ResourceRule rule{
      .role = child_text(constraint, "Role"), .event_groups = {}, .events = {}, .resources = {}};
  for (const pugi::xml_node group :
       constraint.child("AppliesTo").child("EventGroups").children("EventGroup")) {
    rule.event_groups.insert(attribute(group, "Reference"));
  }
  for (const pugi::xml_node event :
       constraint.child("AppliesTo").child("Events").children("Event")) {
    rule.events.insert(attribute(event, "Reference"));
  }
  return rule;
}

bool applies_to(const ResourceRule& rule, std::string_view event,
                const std::set<std::string, std::less<>>& event_groups) {
  return rule.events.contains(event) ||
         std::ranges::any_of(rule.event_groups,
                             [&](const auto& group) { return event_groups.contains(group); });
}

}  // namespace

XhsttImportResult import_xhstt(std::string_view contents) {
  XhsttImportResult result;
  pugi::xml_document document;
  const pugi::xml_parse_result parsed = document.load_buffer(contents.data(), contents.size());
  if (!parsed) {
    result.error = "Cannot parse XHSTT XML at offset " + std::to_string(parsed.offset) + ": " +
                   parsed.description();
    return result;
  }

  const pugi::xml_node archive = document.child("HighSchoolTimetableArchive");
  const pugi::xml_node instance = archive.child("Instances").child("Instance");
  if (!instance) {
    result.error = "XHSTT archive does not contain an instance.";
    return result;
  }

  domain::Project project;
  project.metadata = {.id = attribute(instance, "Id"),
                      .display_name = child_text(instance.child("MetaData"), "Name")};

  std::map<std::string, std::size_t, std::less<>> day_indices;
  int day_ordinal = 0;
  const pugi::xml_node time_groups = instance.child("Times").child("TimeGroups");
  for (const pugi::xml_node group : time_groups.children("Day")) {
    const std::string source_id = attribute(group, "Id");
    day_indices.emplace(source_id, project.calendar.days.size());
    project.calendar.days.push_back(
        {.id = source_id, .display_name = child_text(group, "Name"), .ordinal = day_ordinal++});
  }
  if (project.calendar.days.empty()) {
    result.error = "XHSTT instance does not define any days.";
    return result;
  }

  std::map<std::string, std::size_t, std::less<>> periods_per_day;
  std::size_t maximum_periods = 0;
  for (const pugi::xml_node time : instance.child("Times").children("Time")) {
    const std::string day = attribute(time.child("Day"), "Reference");
    maximum_periods = std::max(maximum_periods, ++periods_per_day[day]);
  }
  for (std::size_t index = 0; index < maximum_periods; ++index) {
    project.calendar.periods.push_back(
        {.id = "period-" + std::to_string(index + 1), .ordinal = static_cast<int>(index)});
  }
  project.calendar =
      domain::make_calendar(std::move(project.calendar.days), std::move(project.calendar.periods));

  std::map<std::string, domain::SlotId, std::less<>> time_slots;
  std::map<std::string, std::vector<domain::SlotId>, std::less<>> time_group_slots;
  periods_per_day.clear();
  for (const pugi::xml_node time : instance.child("Times").children("Time")) {
    const std::string day = attribute(time.child("Day"), "Reference");
    const auto day_index = day_indices.find(day);
    if (day_index == day_indices.end()) {
      result.error = "XHSTT time references an unknown day: " + day;
      return result;
    }
    const std::size_t period_index = periods_per_day[day]++;
    const auto slot = std::ranges::find_if(project.calendar.slots, [&](const domain::Slot& item) {
      return item.day_index == day_index->second && item.period_index == period_index;
    });
    time_slots.emplace(attribute(time, "Id"), slot->id);
    time_group_slots[day].push_back(slot->id);
    for (const pugi::xml_node group : time.child("TimeGroups").children("TimeGroup")) {
      time_group_slots[attribute(group, "Reference")].push_back(slot->id);
    }
  }

  std::map<std::string, domain::TeacherId, std::less<>> teachers;
  std::map<std::string, domain::StudentGroupId, std::less<>> groups;
  std::map<std::string, domain::RoomId, std::less<>> rooms;
  std::map<std::string, std::vector<std::string>, std::less<>> resources_by_type;
  std::map<std::string, std::vector<std::string>, std::less<>> resource_group_members;
  for (const pugi::xml_node resource : instance.child("Resources").children("Resource")) {
    const std::string source_id = attribute(resource, "Id");
    const std::string display_name = child_text(resource, "Name");
    const std::string type = attribute(resource.child("ResourceType"), "Reference");
    resources_by_type[type].push_back(source_id);
    if (type == "Teacher") {
      const domain::TeacherId id{"teacher-" + source_id};
      teachers.emplace(source_id, id);
      project.teachers.push_back({.id = id, .display_name = display_name});
    } else if (type == "Class" || type == "Student") {
      const domain::StudentGroupId id{"group-" + source_id};
      groups.emplace(source_id, id);
      project.student_groups.push_back({.id = id,
                                        .display_name = display_name,
                                        .allowed_slots = all_slot_ids<domain::SlotId>(project),
                                        .allow_repeated_subjects_per_day = true});
    } else if (type == "Room") {
      const domain::RoomId id{"room-" + source_id};
      rooms.emplace(source_id, id);
      project.rooms.push_back({.id = id, .display_name = display_name});
    }
    for (const pugi::xml_node group : resource.child("ResourceGroups").children("ResourceGroup")) {
      resource_group_members[attribute(group, "Reference")].push_back(source_id);
    }
  }

  std::vector<ResourceRule> assignment_rules;
  for (const pugi::xml_node constraint :
       instance.child("Constraints").children("AssignResourceConstraint")) {
    if (child_text(constraint, "Required") == "true") {
      assignment_rules.push_back(read_resource_rule(constraint));
    }
  }
  std::vector<ResourceRule> preference_rules;
  for (const pugi::xml_node constraint :
       instance.child("Constraints").children("PreferResourcesConstraint")) {
    if (child_text(constraint, "Required") != "true") {
      continue;
    }
    ResourceRule rule = read_resource_rule(constraint);
    for (const pugi::xml_node resource : constraint.child("Resources").children("Resource")) {
      rule.resources.push_back(attribute(resource, "Reference"));
    }
    for (const pugi::xml_node group :
         constraint.child("ResourceGroups").children("ResourceGroup")) {
      const std::string reference = attribute(group, "Reference");
      const auto members = resource_group_members.find(reference);
      if (members == resource_group_members.end()) {
        result.error =
            "XHSTT resource preference references an unknown resource group: " + reference;
        return result;
      }
      rule.resources.insert(rule.resources.end(), members->second.begin(), members->second.end());
    }
    preference_rules.push_back(std::move(rule));
  }

  std::map<std::string, std::size_t, std::less<>> teacher_indices;
  for (std::size_t index = 0; index < project.teachers.size(); ++index) {
    teacher_indices.emplace(project.teachers[index].id.value(), index);
  }
  std::map<std::string, std::size_t, std::less<>> group_indices;
  for (std::size_t index = 0; index < project.student_groups.size(); ++index) {
    group_indices.emplace(project.student_groups[index].id.value(), index);
  }
  std::map<std::string, std::size_t, std::less<>> room_indices;
  for (std::size_t index = 0; index < project.rooms.size(); ++index) {
    room_indices.emplace(project.rooms[index].id.value(), index);
  }

  for (const pugi::xml_node constraint :
       instance.child("Constraints").children("AvoidUnavailableTimesConstraint")) {
    if (child_text(constraint, "Required") != "true") {
      continue;
    }

    std::vector<domain::SlotId> unavailable_slots;
    const auto add_slot = [&](const domain::SlotId& slot) {
      if (std::ranges::find(unavailable_slots, slot) == unavailable_slots.end()) {
        unavailable_slots.push_back(slot);
      }
    };
    for (const pugi::xml_node time : constraint.child("Times").children("Time")) {
      const std::string reference = attribute(time, "Reference");
      const auto slot = time_slots.find(reference);
      if (slot == time_slots.end()) {
        result.error = "XHSTT availability constraint references an unknown time: " + reference;
        return result;
      }
      add_slot(slot->second);
    }
    for (const pugi::xml_node group : constraint.child("TimeGroups").children("TimeGroup")) {
      const std::string reference = attribute(group, "Reference");
      const auto slots = time_group_slots.find(reference);
      if (slots == time_group_slots.end()) {
        result.error =
            "XHSTT availability constraint references an unknown time group: " + reference;
        return result;
      }
      for (const domain::SlotId& slot : slots->second) {
        add_slot(slot);
      }
    }

    std::vector<std::string> resource_ids;
    for (const pugi::xml_node resource :
         constraint.child("AppliesTo").child("Resources").children("Resource")) {
      resource_ids.push_back(attribute(resource, "Reference"));
    }
    for (const pugi::xml_node group :
         constraint.child("AppliesTo").child("ResourceGroups").children("ResourceGroup")) {
      const std::string reference = attribute(group, "Reference");
      const auto members = resource_group_members.find(reference);
      if (members == resource_group_members.end()) {
        result.error =
            "XHSTT availability constraint references an unknown resource group: " + reference;
        return result;
      }
      resource_ids.insert(resource_ids.end(), members->second.begin(), members->second.end());
    }

    for (const std::string& source_id : resource_ids) {
      if (const auto teacher = teachers.find(source_id); teacher != teachers.end()) {
        domain::Teacher& item = project.teachers[teacher_indices.at(teacher->second.value())];
        for (const domain::SlotId& slot : unavailable_slots) {
          if (std::ranges::find(item.unavailable_slots, slot) == item.unavailable_slots.end()) {
            item.unavailable_slots.push_back(slot);
          }
        }
      } else if (const auto group = groups.find(source_id); group != groups.end()) {
        domain::StudentGroup& item =
            project.student_groups[group_indices.at(group->second.value())];
        std::erase_if(item.allowed_slots, [&](const domain::SlotId& slot) {
          return std::ranges::find(unavailable_slots, slot) != unavailable_slots.end();
        });
      } else if (const auto room = rooms.find(source_id); room != rooms.end()) {
        domain::Room& item = project.rooms[room_indices.at(room->second.value())];
        for (const domain::SlotId& slot : unavailable_slots) {
          if (std::ranges::find(item.unavailable_slots, slot) == item.unavailable_slots.end()) {
            item.unavailable_slots.push_back(slot);
          }
        }
      } else {
        result.error = "XHSTT availability constraint references an unknown resource: " + source_id;
        return result;
      }
    }
  }

  std::map<std::string, std::string, std::less<>> course_names;
  for (const pugi::xml_node course :
       instance.child("Events").child("EventGroups").children("Course")) {
    course_names.emplace(attribute(course, "Id"), child_text(course, "Name"));
  }
  std::map<std::string, domain::SubjectId, std::less<>> subjects;
  std::map<std::string, std::size_t, std::less<>> subject_indices;
  for (const auto& [course_id, course_name] : course_names) {
    const domain::SubjectId id{"subject-" + course_id};
    subjects.emplace(course_id, id);
    subject_indices.emplace(course_id, project.subjects.size());
    project.subjects.push_back(
        {.id = id, .display_name = course_name, .required_consecutive_periods = 0});
  }

  for (const pugi::xml_node constraint :
       instance.child("Constraints").children("SpreadEventsConstraint")) {
    if (child_text(constraint, "Required") != "true") {
      continue;
    }
    std::optional<int> maximum;
    for (const pugi::xml_node group : constraint.child("TimeGroups").children("TimeGroup")) {
      const int minimum = group.child("Minimum").text().as_int();
      const int group_maximum = group.child("Maximum").text().as_int();
      if (minimum != 0 || group_maximum <= 0 || (maximum && *maximum != group_maximum)) {
        result.error = "XHSTT spread constraint uses unsupported daily bounds.";
        return result;
      }
      maximum = group_maximum;
    }
    if (!maximum) {
      result.error = "XHSTT spread constraint does not define daily time-group bounds.";
      return result;
    }
    for (const pugi::xml_node group :
         constraint.child("AppliesTo").child("EventGroups").children("EventGroup")) {
      const std::string reference = attribute(group, "Reference");
      const auto subject = subject_indices.find(reference);
      if (subject == subject_indices.end()) {
        result.error = "XHSTT spread constraint references a non-course event group: " + reference;
        return result;
      }
      domain::Subject& item = project.subjects[subject->second];
      if (item.maximum_occurrences_per_day) {
        item.maximum_occurrences_per_day = std::min(*item.maximum_occurrences_per_day, *maximum);
      } else {
        item.maximum_occurrences_per_day = maximum;
      }
    }
  }

  std::set<std::string, std::less<>> linked_event_groups;
  for (const pugi::xml_node constraint :
       instance.child("Constraints").children("LinkEventsConstraint")) {
    if (child_text(constraint, "Required") != "true") {
      continue;
    }
    if (!constraint.child("AppliesTo").child("Events").child("Event").empty()) {
      result.error = "XHSTT link constraint applies directly to events instead of event groups.";
      return result;
    }
    for (const pugi::xml_node group :
         constraint.child("AppliesTo").child("EventGroups").children("EventGroup")) {
      linked_event_groups.insert(attribute(group, "Reference"));
    }
  }

  std::map<std::string, std::size_t, std::less<>> meeting_indices;
  std::map<std::string, std::vector<std::string>, std::less<>> meeting_teacher_roles;
  std::map<std::string, std::vector<std::string>, std::less<>> meeting_room_roles;
  for (const pugi::xml_node event : instance.child("Events").children("Event")) {
    const std::string event_id = attribute(event, "Id");
    const std::string course_id = attribute(event.child("Course"), "Reference");
    const auto subject = subjects.find(course_id);
    if (subject == subjects.end()) {
      result.error = "XHSTT event references an unknown course: " + course_id;
      return result;
    }
    const int duration = event.child("Duration").text().as_int();
    domain::Meeting meeting{.id = domain::MeetingId{"meeting-" + event_id},
                            .subject = subject->second,
                            .allowed_start_slots = allowed_starts(project, duration),
                            .duration_in_periods = duration,
                            .distribution_key = course_id};
    std::set<std::string, std::less<>> event_groups;
    for (const pugi::xml_node group : event.child("EventGroups").children("EventGroup")) {
      const std::string reference = attribute(group, "Reference");
      event_groups.insert(reference);
      if (linked_event_groups.contains(reference)) {
        meeting.simultaneity_keys.push_back(reference);
      }
    }
    std::vector<std::string> teacher_roles;
    std::vector<std::string> room_roles;
    const auto add_resource = [&](const std::string& source_id, const std::string& role) -> bool {
      if (const auto group = groups.find(source_id); group != groups.end()) {
        if (std::ranges::find(meeting.groups, group->second) == meeting.groups.end()) {
          meeting.groups.push_back(group->second);
        }
      } else if (const auto teacher = teachers.find(source_id); teacher != teachers.end()) {
        const bool already_assigned = std::ranges::any_of(
            meeting.teacher_requirements,
            [&](const auto& requirement) { return requirement.fixed_teacher == teacher->second; });
        if (!already_assigned) {
          const int lane = static_cast<int>(meeting.teacher_requirements.size());
          meeting.teacher_requirements.push_back({.fixed_teacher = teacher->second, .lane = lane});
          teacher_roles.push_back(role);
          domain::Teacher& item = project.teachers[teacher_indices.at(teacher->second.value())];
          item.maximum_weekly_load += duration;
          if (std::ranges::find(item.qualified_subjects, subject->second) ==
              item.qualified_subjects.end()) {
            item.qualified_subjects.push_back(subject->second);
          }
        }
      } else if (const auto room = rooms.find(source_id); room != rooms.end()) {
        const bool already_assigned = std::ranges::any_of(
            meeting.room_requirements,
            [&](const auto& requirement) { return requirement.fixed_room == room->second; });
        if (!already_assigned) {
          const int lane = static_cast<int>(meeting.room_requirements.size());
          meeting.room_requirements.push_back({.fixed_room = room->second, .lane = lane});
          room_roles.push_back(role);
        }
      } else {
        result.error = "XHSTT event references an unknown resource: " + source_id;
        return false;
      }
      return true;
    };
    for (const pugi::xml_node resource : event.child("Resources").children("Resource")) {
      const std::string source_id = attribute(resource, "Reference");
      const std::string role = child_text(resource, "Role");
      if (!source_id.empty()) {
        if (!add_resource(source_id, role)) {
          return result;
        }
        continue;
      }

      const std::string type = attribute(resource.child("ResourceType"), "Reference");
      const bool assignment_required =
          std::ranges::any_of(assignment_rules, [&](const ResourceRule& rule) {
            return rule.role == role && applies_to(rule, event_id, event_groups);
          });
      if (!assignment_required) {
        result.error =
            "XHSTT event has an unassigned resource role without a required AssignResource "
            "constraint: ";
        result.error.append(event_id).append("/").append(role);
        return result;
      }
      std::vector<std::string> candidates;
      for (const ResourceRule& rule : preference_rules) {
        if (rule.role == role && applies_to(rule, event_id, event_groups)) {
          candidates.insert(candidates.end(), rule.resources.begin(), rule.resources.end());
        }
      }
      if (candidates.empty()) {
        candidates = resources_by_type[type];
      }
      std::ranges::sort(candidates);
      const auto unique_end = std::ranges::unique(candidates).begin();
      candidates.erase(unique_end, candidates.end());

      if (type == "Teacher") {
        domain::TeacherRequirement requirement{
            .fixed_teacher = {},
            .candidates = {},
            .lane = static_cast<int>(meeting.teacher_requirements.size())};
        for (const std::string& candidate : candidates) {
          const auto teacher = teachers.find(candidate);
          if (teacher == teachers.end()) {
            result.error = "XHSTT teacher role has a non-teacher candidate: " + candidate;
            return result;
          }
          requirement.candidates.push_back(teacher->second);
          domain::Teacher& item = project.teachers[teacher_indices.at(teacher->second.value())];
          item.maximum_weekly_load += duration;
          if (std::ranges::find(item.qualified_subjects, subject->second) ==
              item.qualified_subjects.end()) {
            item.qualified_subjects.push_back(subject->second);
          }
        }
        if (requirement.candidates.empty()) {
          result.error = "XHSTT teacher role has no candidates: ";
          result.error.append(event_id).append("/").append(role);
          return result;
        }
        meeting.teacher_requirements.push_back(std::move(requirement));
        teacher_roles.push_back(role);
      } else if (type == "Room") {
        domain::RoomRequirement requirement{
            .fixed_room = {},
            .candidates = {},
            .required_features = {},
            .minimum_capacity = 0,
            .lane = static_cast<int>(meeting.room_requirements.size())};
        for (const std::string& candidate : candidates) {
          const auto room = rooms.find(candidate);
          if (room == rooms.end()) {
            result.error = "XHSTT room role has a non-room candidate: " + candidate;
            return result;
          }
          requirement.candidates.push_back(room->second);
        }
        if (requirement.candidates.empty()) {
          result.error = "XHSTT room role has no candidates: ";
          result.error.append(event_id).append("/").append(role);
          return result;
        }
        meeting.room_requirements.push_back(std::move(requirement));
        room_roles.push_back(role);
      } else {
        result.error = "XHSTT unassigned resource role uses an unsupported type: " + type;
        return result;
      }
    }
    for (const pugi::xml_node group : event.child("ResourceGroups").children("ResourceGroup")) {
      const std::string reference = attribute(group, "Reference");
      const auto members = resource_group_members.find(reference);
      if (members == resource_group_members.end()) {
        result.error = "XHSTT event references an unknown resource group: " + reference;
        return result;
      }
      for (const std::string& source_id : members->second) {
        if (!add_resource(source_id, "")) {
          return result;
        }
      }
    }
    meeting.resource_lanes_aligned =
        meeting.room_requirements.empty() ||
        meeting.teacher_requirements.size() == meeting.room_requirements.size();
    meeting_indices.emplace(event_id, project.meetings.size());
    meeting_teacher_roles.emplace(event_id, std::move(teacher_roles));
    meeting_room_roles.emplace(event_id, std::move(room_roles));
    project.meetings.push_back(std::move(meeting));
  }

  pugi::xml_node last_solution;
  for (const pugi::xml_node group : archive.child("SolutionGroups").children("SolutionGroup")) {
    const pugi::xml_node solution = group.child("Solution");
    if (!solution.empty() && attribute(solution, "Reference") == project.metadata.id) {
      last_solution = solution;
    }
  }
  if (!last_solution.empty()) {
    domain::Schedule schedule;
    schedule.meetings.reserve(project.meetings.size());
    for (const pugi::xml_node event : last_solution.child("Events").children("Event")) {
      const std::string source_event = attribute(event, "Reference");
      const std::string source_time = attribute(event.child("Time"), "Reference");
      const auto meeting_index = meeting_indices.find(source_event);
      const auto slot = time_slots.find(source_time);
      if (meeting_index == meeting_indices.end() || slot == time_slots.end()) {
        result.error = "XHSTT solution references an unknown event or time.";
        return result;
      }
      const domain::Meeting& meeting = project.meetings[meeting_index->second];
      domain::ScheduledMeeting assignment{.meeting = meeting.id, .start_slot = slot->second};
      std::map<std::string, std::vector<std::string>, std::less<>> assigned_resources;
      for (const pugi::xml_node resource : event.child("Resources").children("Resource")) {
        assigned_resources[child_text(resource, "Role")].push_back(
            attribute(resource, "Reference"));
      }
      std::map<std::string, std::size_t, std::less<>> assigned_resource_cursors;
      const auto assigned_source = [&](const std::string& role) -> std::optional<std::string> {
        const auto resources = assigned_resources.find(role);
        if (resources == assigned_resources.end()) {
          return std::nullopt;
        }
        std::size_t& cursor = assigned_resource_cursors[role];
        if (cursor >= resources->second.size()) {
          return std::nullopt;
        }
        return resources->second[cursor++];
      };
      const auto& teacher_roles = meeting_teacher_roles.at(source_event);
      for (std::size_t index = 0; index < meeting.teacher_requirements.size(); ++index) {
        const domain::TeacherRequirement& requirement = meeting.teacher_requirements[index];
        if (requirement.fixed_teacher) {
          assignment.teachers.push_back(*requirement.fixed_teacher);
          continue;
        }
        const std::optional<std::string> source = assigned_source(teacher_roles[index]);
        const auto teacher = source ? teachers.find(*source) : teachers.end();
        if (teacher == teachers.end() ||
            std::ranges::find(requirement.candidates, teacher->second) ==
                requirement.candidates.end()) {
          result.error = "XHSTT solution assigns an ineligible or missing teacher: " + source_event;
          return result;
        }
        assignment.teachers.push_back(teacher->second);
      }
      const auto& room_roles = meeting_room_roles.at(source_event);
      for (std::size_t index = 0; index < meeting.room_requirements.size(); ++index) {
        const domain::RoomRequirement& requirement = meeting.room_requirements[index];
        if (requirement.fixed_room) {
          assignment.rooms.push_back(*requirement.fixed_room);
          continue;
        }
        const std::optional<std::string> source = assigned_source(room_roles[index]);
        const auto room = source ? rooms.find(*source) : rooms.end();
        if (room == rooms.end() || std::ranges::find(requirement.candidates, room->second) ==
                                       requirement.candidates.end()) {
          result.error = "XHSTT solution assigns an ineligible or missing room: " + source_event;
          return result;
        }
        assignment.rooms.push_back(room->second);
      }
      schedule.meetings.push_back(std::move(assignment));
    }
    result.reference_schedule = std::move(schedule);
  } else {
    result.warnings.emplace_back("XHSTT archive does not contain a published solution.");
  }

  result.warnings.emplace_back(
      "Soft XHSTT constraints are not imported by the supported hard-constraint slice.");
  result.project = std::move(project);
  return result;
}

}  // namespace schedmesh::import
