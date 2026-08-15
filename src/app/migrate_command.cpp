#include "schedmesh/app/migrate_command.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>

#include "schedmesh/app/validate_command.h"
#include "schedmesh/import/legacy_project.h"
#include "schedmesh/io/project_json.h"

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

std::filesystem::path resolve_input_path(const std::filesystem::path& settings_path,
                                         std::string_view configured_path) {
  std::filesystem::path path{configured_path};
  if (path.is_absolute() || std::filesystem::exists(path)) {
    return path;
  }
  return settings_path.parent_path() / path;
}

void print_report(const import::MigrationReport& report, std::ostream& errors) {
  for (const import::MigrationDiagnostic& diagnostic : report.diagnostics) {
    errors << (diagnostic.severity == import::MigrationSeverity::kError ? "error " : "warning ")
           << diagnostic.code << ' ' << diagnostic.path << ": " << diagnostic.message;
    if (!diagnostic.suggested_action.empty()) {
      errors << " Action: " << diagnostic.suggested_action;
    }
    errors << '\n';
  }
}

std::optional<import::CsvTable> read_csv_file(const std::filesystem::path& path,
                                              std::ostream& errors) {
  const std::optional<std::string> contents = read_file(path, errors);
  if (!contents) {
    return std::nullopt;
  }
  import::CsvReadResult result = import::read_legacy_csv(*contents);
  print_report(result.report, errors);
  return result.ok() ? std::move(result.table) : std::nullopt;
}

}  // namespace

int migrate_legacy_project(std::string_view settings_path_value, std::string_view output_path_value,
                           std::ostream& output, std::ostream& errors) {
  const std::filesystem::path settings_path{settings_path_value};
  const std::optional<std::string> config_contents = read_file(settings_path, errors);
  if (!config_contents) {
    return kExitUsageOrIoError;
  }
  import::LegacyConfigReadResult config = import::read_legacy_config(*config_contents);
  print_report(config.report, errors);
  if (!config.ok()) {
    return kExitValidationError;
  }
  import::LegacySettingsReadResult settings = import::decode_legacy_settings(*config.config);
  print_report(settings.report, errors);
  if (!settings.ok()) {
    return kExitValidationError;
  }

  const auto timetable_path = resolve_input_path(settings_path, settings.settings->input_file);
  const auto classrooms_path =
      resolve_input_path(settings_path, settings.settings->classrooms_file);
  std::optional<import::CsvTable> timetable = read_csv_file(timetable_path, errors);
  std::optional<import::CsvTable> classrooms = read_csv_file(classrooms_path, errors);
  if (!timetable || !classrooms) {
    return kExitUsageOrIoError;
  }
  std::optional<import::CsvTable> methodical_days;
  if (settings.settings->methodical_days_file) {
    methodical_days = read_csv_file(
        resolve_input_path(settings_path, *settings.settings->methodical_days_file), errors);
    if (!methodical_days) {
      return kExitUsageOrIoError;
    }
  }

  import::LegacyProjectImportResult project =
      import::import_legacy_timetable(*settings.settings, *timetable);
  print_report(project.report, errors);
  if (!project.ok()) {
    return kExitValidationError;
  }
  project =
      import::import_legacy_resources(std::move(*project.project), *classrooms, methodical_days);
  print_report(project.report, errors);
  if (!project.ok()) {
    return kExitValidationError;
  }

  const std::filesystem::path output_path{output_path_value};
  std::ofstream destination(output_path, std::ios::binary | std::ios::trunc);
  if (!destination) {
    errors << "error io.open_failed " << output_path.generic_string()
           << ": Cannot open output file.\n";
    return kExitUsageOrIoError;
  }
  destination << io::write_project_json(*project.project);
  if (!destination) {
    errors << "error io.write_failed " << output_path.generic_string()
           << ": Cannot write migrated project.\n";
    return kExitUsageOrIoError;
  }
  output << "Migrated legacy project to " << output_path.generic_string() << '\n';
  return kExitSuccess;
}

}  // namespace schedmesh::app
