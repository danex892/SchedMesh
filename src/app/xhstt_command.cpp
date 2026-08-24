#include "schedmesh/app/xhstt_command.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <ostream>
#include <string>

#include "schedmesh/app/validate_command.h"
#include "schedmesh/import/xhstt.h"
#include "schedmesh/io/project_json.h"
#include "schedmesh/io/schedule_json.h"
#include "schedmesh/validation/project_validator.h"
#include "schedmesh/validation/schedule_validator.h"

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

bool write_file(const std::filesystem::path& path, std::string_view contents,
                std::ostream& errors) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output << contents;
  if (!output) {
    errors << "error io.write_failed " << path.generic_string() << ": Cannot write output file.\n";
    return false;
  }
  return true;
}

void print_diagnostics(const validation::ValidationResult& validation, std::ostream& errors) {
  for (const validation::Diagnostic& diagnostic : validation.diagnostics) {
    errors << "error " << diagnostic.code << ' ' << diagnostic.path;
    if (!diagnostic.entity_id.empty()) {
      errors << " entity=" << diagnostic.entity_id;
    }
    errors << ": " << diagnostic.message << '\n';
  }
}

}  // namespace

int import_xhstt_file(std::string_view input_path, std::string_view project_output_path,
                      std::string_view schedule_output_path, std::ostream& output,
                      std::ostream& errors) {
  const std::optional<std::string> contents = read_file(std::filesystem::path{input_path}, errors);
  if (!contents) {
    return kExitUsageOrIoError;
  }
  import::XhsttImportResult imported = import::import_xhstt(*contents);
  if (!imported.ok()) {
    errors << "error xhstt.import_failed /: " << imported.error << '\n';
    return kExitValidationError;
  }
  for (const std::string& warning : imported.warnings) {
    errors << "warning xhstt.partial_support /: " << warning << '\n';
  }

  const validation::ValidationResult project_validation =
      validation::ProjectValidator{}.validate(*imported.project);
  if (!project_validation.ok()) {
    print_diagnostics(project_validation, errors);
    return kExitValidationError;
  }
  if (!write_file(std::filesystem::path{project_output_path},
                  io::write_project_json(*imported.project), errors)) {
    return kExitUsageOrIoError;
  }

  if (!imported.reference_schedule) {
    output << "Imported XHSTT project without a reference schedule to " << project_output_path
           << '\n';
    return kExitSuccess;
  }
  if (!write_file(std::filesystem::path{schedule_output_path},
                  io::write_schedule_json(*imported.reference_schedule), errors)) {
    return kExitUsageOrIoError;
  }
  const validation::ValidationResult schedule_validation =
      validation::ScheduleValidator{}.validate(*imported.project, *imported.reference_schedule);
  if (!schedule_validation.ok()) {
    print_diagnostics(schedule_validation, errors);
    return kExitValidationError;
  }
  output << "Imported valid XHSTT project " << imported.project->metadata.id << " with "
         << imported.project->meetings.size() << " meetings and a valid reference schedule\n";
  return kExitSuccess;
}

}  // namespace schedmesh::app
