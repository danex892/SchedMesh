#include "schedmesh/export/xlsx_exporter.h"

#include <xlsxwriter.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <map>
#include <optional>
#include <ranges>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "schedmesh/validation/schedule_validator.h"

namespace schedmesh::exporting {
namespace {

constexpr std::string_view kProfileSeparator = " / profile ";
constexpr double kClassColumnWidth = 25.0;
constexpr double kMinimumLessonHeight = 54.0;
constexpr double kBodyFontSize = 9.0;
constexpr double kSummaryLabelWidth = 28.0;
constexpr double kSummaryValueWidth = 22.0;
constexpr double kSummaryTitleFontSize = 18.0;
constexpr double kSummaryTitleHeight = 32.0;
constexpr double kSheetTitleFontSize = 16.0;
constexpr double kHeaderFontSize = 10.0;
constexpr double kDayColumnWidth = 14.0;
constexpr double kPeriodColumnWidth = 7.0;
constexpr double kSheetTitleHeight = 28.0;
constexpr double kHorizontalMargin = 0.25;
constexpr double kVerticalMargin = 0.4;
constexpr double kTextLineHeight = 13.0;
constexpr double kTextPadding = 8.0;
constexpr std::uint32_t kWhite = 0xFFFFFF;
constexpr std::uint32_t kBorderColor = 0xB7C9D6;
constexpr std::uint32_t kSummaryColor = 0x1F4E78;
constexpr std::uint32_t kSummaryLabelColor = 0xD9EAF7;
constexpr std::uint32_t kFirstShiftColor = 0x4472C4;
constexpr std::uint32_t kSecondShiftColor = 0x70AD47;
constexpr std::uint32_t kSubtitleColor = 0x44546A;
constexpr std::uint32_t kHeaderColor = 0x5B9BD5;
constexpr std::uint32_t kPeriodColor = 0xE2F0D9;
constexpr std::uint32_t kMixedSubjectColor = 0xE7E6E6;
constexpr lxw_row_t kLegendRow = 9;
constexpr lxw_row_t kSourceRow = 11;
// libxlsxwriter uses Excel's numeric paper-size codes; 9 denotes A4.
constexpr std::uint8_t kA4Paper = 9;
constexpr std::array<std::uint32_t, 12> kSubjectColors = {0xDDEBF7, 0xE2F0D9, 0xFFF2CC, 0xFCE4D6,
                                                          0xE4DFEC, 0xDDEBF7, 0xF4CCCC, 0xD9EAD3,
                                                          0xCFE2F3, 0xFCE5CD, 0xD9D2E9, 0xEAD1DC};

struct GroupView {
  std::string class_name;
  std::optional<int> profile;
  std::size_t shift{};
  std::size_t period_offset{};
};

struct LessonBlock {
  std::string subject_id;
  std::string subject;
  std::optional<int> profile;
  std::vector<std::string> resources;
  int part{1};
  int duration{1};
};

using CellKey = std::tuple<std::size_t, std::size_t, std::size_t, std::string>;

template <typename Entity, typename Id>
const Entity* find_entity(const std::vector<Entity>& entities, const Id& id) {
  const auto found =
      std::ranges::find_if(entities, [&](const Entity& entity) { return entity.id == id; });
  return found == entities.end() ? nullptr : &*found;
}

std::pair<std::string, std::optional<int>> split_profile_name(std::string_view display_name) {
  const std::size_t separator = display_name.rfind(kProfileSeparator);
  if (separator == std::string_view::npos) {
    return {std::string(display_name), std::nullopt};
  }
  int profile{};
  const std::string_view suffix = display_name.substr(separator + kProfileSeparator.size());
  const auto [end, error] = std::from_chars(suffix.data(), suffix.data() + suffix.size(), profile);
  if (error != std::errc{} || end != suffix.data() + suffix.size()) {
    return {std::string(display_name), std::nullopt};
  }
  return {std::string(display_name.substr(0, separator)), profile};
}

std::size_t first_period(const domain::Project& project, const domain::StudentGroup& group) {
  std::size_t first = project.calendar.periods.size();
  for (const domain::SlotId& slot_id : group.allowed_slots) {
    const domain::Slot* slot = find_entity(project.calendar.slots, slot_id);
    if (slot != nullptr) {
      first = std::min(first, slot->period_index);
    }
  }
  return first;
}

std::tuple<int, std::string> natural_class_key(std::string_view name) {
  int grade{};
  const auto [end, error] = std::from_chars(name.data(), name.data() + name.size(), grade);
  if (error != std::errc{} || end == name.data()) {
    return {0, std::string(name)};
  }
  return {grade, std::string(end, name.data() + name.size())};
}

lxw_format* body_format(lxw_workbook* workbook, std::uint32_t color) {
  lxw_format* format = workbook_add_format(workbook);
  format_set_font_name(format, "Aptos");
  format_set_font_size(format, kBodyFontSize);
  format_set_text_wrap(format);
  format_set_align(format, LXW_ALIGN_CENTER);
  format_set_align(format, LXW_ALIGN_VERTICAL_CENTER);
  format_set_border(format, LXW_BORDER_THIN);
  format_set_border_color(format, kBorderColor);
  format_set_bg_color(format, color);
  return format;
}

std::string lesson_text(const std::vector<LessonBlock>& blocks) {
  std::string result;
  for (std::size_t block_index = 0; block_index < blocks.size(); ++block_index) {
    const LessonBlock& block = blocks[block_index];
    if (block_index > 0) {
      result.append("\n────────\n");
    }
    if (blocks.size() > 1) {
      result.append(std::to_string(block_index + 1)).append(") ");
    }
    if (block.profile) {
      result.append("Profile ").append(std::to_string(*block.profile)).append(" · ");
    }
    result.append(block.subject);
    if (block.duration > 1) {
      result.append(" (")
          .append(std::to_string(block.part))
          .append("/")
          .append(std::to_string(block.duration))
          .append(")");
    }
    for (std::size_t resource = 0; resource < block.resources.size(); ++resource) {
      result.push_back('\n');
      if (block.resources.size() > 1) {
        result.append(std::to_string(resource + 1)).append(") ");
      }
      result.append(block.resources[resource]);
    }
  }
  return result;
}

std::size_t line_count(std::string_view value) {
  return 1 + static_cast<std::size_t>(std::ranges::count(value, '\n'));
}

void configure_title_format(lxw_format* format, std::uint32_t background, double size) {
  format_set_font_name(format, "Aptos Display");
  format_set_font_size(format, size);
  format_set_bold(format);
  format_set_font_color(format, kWhite);
  format_set_bg_color(format, background);
  format_set_align(format, LXW_ALIGN_CENTER);
  format_set_align(format, LXW_ALIGN_VERTICAL_CENTER);
}

void write_summary(lxw_workbook* workbook, const domain::Project& project, std::size_t class_count,
                   std::size_t shift_count) {
  lxw_worksheet* sheet = workbook_add_worksheet(workbook, "Summary");
  worksheet_gridlines(sheet, LXW_HIDE_ALL_GRIDLINES);
  worksheet_set_tab_color(sheet, kSummaryColor);
  worksheet_set_column(sheet, 0, 0, kSummaryLabelWidth, nullptr);
  worksheet_set_column(sheet, 1, 3, kSummaryValueWidth, nullptr);

  lxw_format* title = workbook_add_format(workbook);
  configure_title_format(title, kSummaryColor, kSummaryTitleFontSize);
  worksheet_merge_range(sheet, 0, 0, 0, 3, "SchedMesh · Generated Timetable", title);
  worksheet_set_row(sheet, 0, kSummaryTitleHeight, nullptr);

  lxw_format* label = workbook_add_format(workbook);
  format_set_font_name(label, "Aptos");
  format_set_bold(label);
  format_set_bg_color(label, kSummaryLabelColor);
  format_set_border(label, LXW_BORDER_THIN);
  lxw_format* value = workbook_add_format(workbook);
  format_set_font_name(value, "Aptos");
  format_set_border(value, LXW_BORDER_THIN);
  format_set_text_wrap(value);

  const std::array<std::pair<const char*, std::size_t>, 6> statistics = {
      std::pair{"Classes", class_count},
      std::pair{"Shifts", shift_count},
      std::pair{"Teachers", project.teachers.size()},
      std::pair{"Subjects", project.subjects.size()},
      std::pair{"Rooms", project.rooms.size()},
      std::pair{"Meetings", project.meetings.size()}};
  for (std::size_t index = 0; index < statistics.size(); ++index) {
    worksheet_write_string(sheet, static_cast<lxw_row_t>(index + 2), 0, statistics[index].first,
                           label);
    worksheet_write_number(sheet, static_cast<lxw_row_t>(index + 2), 1,
                           static_cast<double>(statistics[index].second), value);
  }
  worksheet_write_string(sheet, kLegendRow, 0, "Legend", label);
  worksheet_merge_range(sheet, kLegendRow, 1, kLegendRow, 3,
                        "Each cell lists a subject, teacher, and room. Numbered entries represent "
                        "parallel subgroups.",
                        value);
  worksheet_write_string(sheet, kSourceRow, 0, "Project", label);
  const std::string project_description = project.metadata.display_name + " · " +
                                          project.metadata.id +
                                          " · generated and validated by SchedMesh";
  worksheet_merge_range(sheet, kSourceRow, 1, kSourceRow, 3, project_description.c_str(), value);
}

}  // namespace

XlsxExportResult export_timetable_xlsx(const domain::Project& project,
                                       const domain::Schedule& schedule,
                                       const std::filesystem::path& output_path) {
  const validation::ValidationResult validation =
      validation::ScheduleValidator{}.validate(project, schedule);
  if (!validation.ok()) {
    return {.error = "Schedule must pass independent validation before XLSX export."};
  }

  std::set<std::size_t> offsets;
  for (const domain::StudentGroup& group : project.student_groups) {
    offsets.insert(first_period(project, group));
  }
  std::vector<std::size_t> shift_offsets(offsets.begin(), offsets.end());
  std::map<std::string, GroupView, std::less<>> group_views;
  std::vector<std::set<std::string, std::less<>>> classes_by_shift(shift_offsets.size());
  std::vector<std::size_t> periods_by_shift(shift_offsets.size(), 0);
  for (const domain::StudentGroup& group : project.student_groups) {
    const std::size_t offset = first_period(project, group);
    const std::size_t shift =
        static_cast<std::size_t>(std::ranges::find(shift_offsets, offset) - shift_offsets.begin());
    auto [class_name, profile] = split_profile_name(group.display_name);
    group_views.emplace(
        group.id.value(),
        GroupView{
            .class_name = class_name, .profile = profile, .shift = shift, .period_offset = offset});
    classes_by_shift[shift].insert(class_name);
    for (const domain::SlotId& slot_id : group.allowed_slots) {
      const domain::Slot* slot = find_entity(project.calendar.slots, slot_id);
      if (slot != nullptr) {
        periods_by_shift[shift] =
            std::max(periods_by_shift[shift], slot->period_index - offset + 1);
      }
    }
  }

  std::map<CellKey, std::vector<LessonBlock>> cells;
  for (const domain::ScheduledMeeting& assignment : schedule.meetings) {
    const domain::Meeting* meeting = find_entity(project.meetings, assignment.meeting);
    const domain::Slot* start = find_entity(project.calendar.slots, assignment.start_slot);
    if (meeting == nullptr || start == nullptr) {
      return {.error = "Schedule references an unknown meeting or slot."};
    }
    const domain::Subject* subject = find_entity(project.subjects, meeting->subject);
    if (subject == nullptr) {
      return {.error = "Meeting references an unknown subject."};
    }
    std::vector<std::string> resources;
    const std::size_t lanes = std::max(assignment.teachers.size(), assignment.rooms.size());
    resources.reserve(lanes);
    for (std::size_t lane = 0; lane < lanes; ++lane) {
      const domain::Teacher* teacher =
          lane < assignment.teachers.size()
              ? find_entity(project.teachers, assignment.teachers[lane])
              : nullptr;
      const domain::Room* room = lane < assignment.rooms.size()
                                     ? find_entity(project.rooms, assignment.rooms[lane])
                                     : nullptr;
      std::string line = teacher == nullptr ? "No teacher" : teacher->display_name;
      line.append(" · ").append(room == nullptr ? "no room" : "room " + room->display_name);
      resources.push_back(std::move(line));
    }

    std::map<std::string, std::vector<const GroupView*>, std::less<>> meeting_classes;
    for (const domain::StudentGroupId& group_id : meeting->groups) {
      const auto group = group_views.find(group_id.value());
      if (group != group_views.end()) {
        meeting_classes[group->second.class_name].push_back(&group->second);
      }
    }
    for (const auto& [class_name, groups] : meeting_classes) {
      const GroupView& group = *groups.front();
      const std::optional<int> profile = groups.size() == 1 ? group.profile : std::nullopt;
      for (int part = 0; part < meeting->duration_in_periods; ++part) {
        cells[{group.shift, start->day_index,
               start->period_index - group.period_offset + static_cast<std::size_t>(part),
               class_name}]
            .push_back({.subject_id = subject->id.value(),
                        .subject = subject->display_name,
                        .profile = profile,
                        .resources = resources,
                        .part = part + 1,
                        .duration = meeting->duration_in_periods});
      }
    }
  }

  const std::string output = output_path.string();
  lxw_workbook* workbook = workbook_new(output.c_str());
  if (workbook == nullptr) {
    return {.error = "Cannot create XLSX workbook."};
  }
  std::set<std::string, std::less<>> all_classes;
  for (const auto& classes : classes_by_shift) {
    all_classes.insert(classes.begin(), classes.end());
  }
  write_summary(workbook, project, all_classes.size(), shift_offsets.size());

  std::map<std::string, lxw_format*, std::less<>> subject_formats;
  for (std::size_t index = 0; index < project.subjects.size(); ++index) {
    subject_formats.emplace(project.subjects[index].id.value(),
                            body_format(workbook, kSubjectColors[index % kSubjectColors.size()]));
  }
  lxw_format* mixed_format = body_format(workbook, kMixedSubjectColor);
  lxw_format* empty_format = body_format(workbook, kWhite);
  lxw_format* title = workbook_add_format(workbook);
  configure_title_format(title, kFirstShiftColor, kSheetTitleFontSize);
  lxw_format* subtitle = workbook_add_format(workbook);
  format_set_font_name(subtitle, "Aptos");
  format_set_italic(subtitle);
  format_set_font_color(subtitle, kSubtitleColor);
  format_set_align(subtitle, LXW_ALIGN_CENTER);
  lxw_format* header = workbook_add_format(workbook);
  configure_title_format(header, kHeaderColor, kHeaderFontSize);
  format_set_border(header, LXW_BORDER_THIN);
  lxw_format* day_format = workbook_add_format(workbook);
  configure_title_format(day_format, kSecondShiftColor, kHeaderFontSize);
  format_set_border(day_format, LXW_BORDER_THIN);
  format_set_text_wrap(day_format);
  lxw_format* period_format = body_format(workbook, kPeriodColor);
  format_set_bold(period_format);

  for (std::size_t shift = 0; shift < shift_offsets.size(); ++shift) {
    std::vector<std::string> classes(classes_by_shift[shift].begin(),
                                     classes_by_shift[shift].end());
    std::ranges::sort(classes, [](const std::string& left, const std::string& right) {
      return natural_class_key(left) < natural_class_key(right);
    });
    const std::string sheet_name = "Shift " + std::to_string(shift + 1);
    lxw_worksheet* sheet = workbook_add_worksheet(workbook, sheet_name.c_str());
    if (shift == 0) {
      worksheet_activate(sheet);
    }
    worksheet_gridlines(sheet, LXW_HIDE_ALL_GRIDLINES);
    worksheet_set_tab_color(sheet, shift == 0 ? kFirstShiftColor : kSecondShiftColor);
    worksheet_set_landscape(sheet);
    worksheet_fit_to_pages(sheet, 1, 0);
    worksheet_set_paper(sheet, kA4Paper);
    worksheet_set_margins(sheet, kHorizontalMargin, kHorizontalMargin, kVerticalMargin,
                          kVerticalMargin);
    worksheet_freeze_panes(sheet, 3, 2);
    worksheet_repeat_rows(sheet, 0, 2);
    worksheet_set_column(sheet, 0, 0, kDayColumnWidth, nullptr);
    worksheet_set_column(sheet, 1, 1, kPeriodColumnWidth, nullptr);
    if (!classes.empty()) {
      worksheet_set_column(sheet, 2, static_cast<lxw_col_t>(classes.size() + 1), kClassColumnWidth,
                           nullptr);
    }
    const auto last_column = static_cast<lxw_col_t>(classes.size() + 1);
    worksheet_merge_range(sheet, 0, 0, 0, last_column, ("Timetable · " + sheet_name).c_str(),
                          title);
    worksheet_set_row(sheet, 0, kSheetTitleHeight, nullptr);
    worksheet_merge_range(sheet, 1, 0, 1, last_column, "Subject · teacher · room", subtitle);
    worksheet_write_string(sheet, 2, 0, "Day", header);
    worksheet_write_string(sheet, 2, 1, "Period", header);
    for (std::size_t column = 0; column < classes.size(); ++column) {
      worksheet_write_string(sheet, 2, static_cast<lxw_col_t>(column + 2), classes[column].c_str(),
                             header);
    }

    lxw_row_t row = 3;
    for (std::size_t day = 0; day < project.calendar.days.size(); ++day) {
      const lxw_row_t first_row = row;
      const auto last_row = static_cast<lxw_row_t>(row + periods_by_shift[shift] - 1);
      worksheet_merge_range(sheet, first_row, 0, last_row, 0,
                            project.calendar.days[day].display_name.c_str(), day_format);
      for (std::size_t period = 0; period < periods_by_shift[shift]; ++period, ++row) {
        worksheet_write_number(sheet, row, 1, static_cast<double>(period + 1), period_format);
        std::size_t maximum_lines = 1;
        for (std::size_t column = 0; column < classes.size(); ++column) {
          auto found = cells.find({shift, day, period, classes[column]});
          if (found == cells.end()) {
            worksheet_write_blank(sheet, row, static_cast<lxw_col_t>(column + 2), empty_format);
            continue;
          }
          auto& blocks = found->second;
          std::ranges::sort(blocks, [](const LessonBlock& left, const LessonBlock& right) {
            return std::tuple{left.profile.value_or(0), left.subject, left.resources} <
                   std::tuple{right.profile.value_or(0), right.subject, right.resources};
          });
          const std::string text = lesson_text(blocks);
          maximum_lines = std::max(maximum_lines, line_count(text));
          lxw_format* format = mixed_format;
          const bool one_subject = std::ranges::all_of(blocks, [&](const LessonBlock& block) {
            return block.subject_id == blocks.front().subject_id;
          });
          if (one_subject) {
            const auto subject_format = subject_formats.find(blocks.front().subject_id);
            if (subject_format != subject_formats.end()) {
              format = subject_format->second;
            }
          }
          worksheet_write_string(sheet, row, static_cast<lxw_col_t>(column + 2), text.c_str(),
                                 format);
        }
        const double content_height =
            (kTextLineHeight * static_cast<double>(maximum_lines)) + kTextPadding;
        worksheet_set_row(sheet, row, std::max(kMinimumLessonHeight, content_height), nullptr);
      }
    }
  }

  const lxw_error close_error = workbook_close(workbook);
  if (close_error != LXW_NO_ERROR) {
    return {.error = std::string{"Cannot write XLSX workbook: "} + lxw_strerror(close_error)};
  }
  return {.success = true,
          .error = {},
          .worksheet_count = shift_offsets.size() + 1,
          .class_count = all_classes.size()};
}

}  // namespace schedmesh::exporting
