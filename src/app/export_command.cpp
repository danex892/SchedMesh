#include "schedmesh/app/export_command.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <ostream>
#include <string>

#include "schedmesh/app/validate_command.h"
#include "schedmesh/export/xlsx_exporter.h"
#include "schedmesh/io/project_json.h"
#include "schedmesh/io/schedule_json.h"

namespace schedmesh::app {
namespace {

std::optional<std::string> read_file(const std::filesystem::path& path, std::ostream& errors) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    errors << "error io.open_failed " << path.generic_string() << ": Cannot open input file.\n";
    return std::nullopt;
  }
  return std::string{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

}  // namespace

int export_schedule_xlsx(std::string_view project_path, std::string_view schedule_path,
                         std::string_view output_path, std::ostream& output, std::ostream& errors) {
  const auto project_contents = read_file(std::filesystem::path{project_path}, errors);
  const auto schedule_contents = read_file(std::filesystem::path{schedule_path}, errors);
  if (!project_contents || !schedule_contents) {
    return kExitUsageOrIoError;
  }
  const io::ProjectReadResult project = io::read_project_json(*project_contents);
  if (!project.ok()) {
    errors << "error export.invalid_project /: Canonical project is invalid.\n";
    return kExitValidationError;
  }
  const io::ScheduleReadResult schedule = io::read_schedule_json(*schedule_contents);
  if (!schedule.ok()) {
    errors << "error export.invalid_schedule /: " << schedule.error << '\n';
    return kExitValidationError;
  }
  const exporting::XlsxExportResult result = exporting::export_timetable_xlsx(
      *project.project, *schedule.schedule, std::filesystem::path{output_path});
  if (!result.success) {
    errors << "error export.xlsx_failed /: " << result.error << '\n';
    return kExitValidationError;
  }
  output << "Exported " << result.class_count << " classes to " << output_path
         << " (worksheets=" << result.worksheet_count << ")\n";
  return kExitSuccess;
}

}  // namespace schedmesh::app
