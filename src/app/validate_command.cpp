#include "schedmesh/app/validate_command.h"

#include <fstream>
#include <iterator>
#include <ostream>
#include <string>

#include "schedmesh/io/project_json.h"

namespace schedmesh::app {
namespace {

const char* severity_name(validation::DiagnosticSeverity severity) {
  return severity == validation::DiagnosticSeverity::kError ? "error" : "warning";
}

void print_diagnostic(const validation::Diagnostic& diagnostic, std::ostream& errors) {
  errors << severity_name(diagnostic.severity) << ' ' << diagnostic.code << ' ' << diagnostic.path;
  if (!diagnostic.entity_id.empty()) {
    errors << " entity=" << diagnostic.entity_id;
  }
  errors << ": " << diagnostic.message;
  if (!diagnostic.suggested_action.empty()) {
    errors << " Action: " << diagnostic.suggested_action;
  }
  errors << '\n';
}

}  // namespace

int validate_project_file(std::string_view path, std::ostream& output, std::ostream& errors) {
  const std::string path_string{path};
  std::ifstream input(path_string, std::ios::binary);
  if (!input) {
    errors << "error io.open_failed /: Cannot open project file: " << path_string << '\n';
    return kExitUsageOrIoError;
  }

  const std::string contents{std::istreambuf_iterator<char>{input},
                             std::istreambuf_iterator<char>{}};
  const io::ProjectReadResult result = io::read_project_json(contents);
  for (const auto& diagnostic : result.validation.diagnostics) {
    print_diagnostic(diagnostic, errors);
  }
  if (!result.ok()) {
    return kExitValidationError;
  }

  output << "Valid project " << result.project->metadata.id << '\n';
  return kExitSuccess;
}

}  // namespace schedmesh::app
