#include "schedmesh/import/legacy_text.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <utility>

namespace schedmesh::import {
namespace {

std::string trim(std::string_view value) {
  const auto is_space = [](unsigned char character) { return std::isspace(character) != 0; };
  const auto begin = std::ranges::find_if_not(value, is_space);
  const auto end = std::ranges::find_if_not(value.rbegin(), value.rend(), is_space).base();
  return begin < end ? std::string(begin, end) : std::string{};
}

void add_error(MigrationReport& report, std::string code, std::string path, std::string message,
               std::string action) {
  report.diagnostics.push_back({.severity = MigrationSeverity::kError,
                                .code = std::move(code),
                                .path = std::move(path),
                                .message = std::move(message),
                                .suggested_action = std::move(action)});
}

}  // namespace

bool MigrationReport::ok() const noexcept {
  return std::ranges::none_of(diagnostics, [](const MigrationDiagnostic& diagnostic) {
    return diagnostic.severity == MigrationSeverity::kError;
  });
}

bool CsvReadResult::ok() const noexcept { return table.has_value() && report.ok(); }

bool LegacyConfigReadResult::ok() const noexcept { return config.has_value() && report.ok(); }

CsvReadResult read_legacy_csv(std::string_view contents) {
  CsvReadResult result;
  CsvTable table;
  CsvRow row;
  std::string field;
  bool quoted = false;
  bool quote_closed = false;
  std::size_t line = 1;

  const auto finish_field = [&] {
    row.push_back(std::move(field));
    field.clear();
    quote_closed = false;
  };
  const auto finish_row = [&] {
    finish_field();
    table.push_back(std::move(row));
    row.clear();
    ++result.report.source_records;
  };

  for (std::size_t index = 0; index < contents.size(); ++index) {
    const char character = contents[index];
    if (quoted) {
      if (character == '"') {
        if (index + 1 < contents.size() && contents[index + 1] == '"') {
          field.push_back('"');
          ++index;
        } else {
          quoted = false;
          quote_closed = true;
        }
      } else {
        field.push_back(character);
        if (character == '\n') {
          ++line;
        }
      }
      continue;
    }

    if (character == '"' && field.empty() && !quote_closed) {
      quoted = true;
    } else if (character == ',') {
      finish_field();
    } else if (character == '\n') {
      finish_row();
      ++line;
    } else if (character == '\r') {
      if (index + 1 >= contents.size() || contents[index + 1] != '\n') {
        finish_row();
        ++line;
      }
    } else if (quote_closed) {
      add_error(result.report, "legacy.csv.characters_after_quote", "/lines/" + std::to_string(line),
                "Unexpected characters after a closing quote.",
                "Place the closing quote immediately before a comma or line ending.");
      break;
    } else if (character == '"') {
      add_error(result.report, "legacy.csv.unexpected_quote", "/lines/" + std::to_string(line),
                "A quote appeared inside an unquoted field.",
                "Quote the entire field and escape embedded quotes by doubling them.");
      break;
    } else {
      field.push_back(character);
    }
  }

  if (quoted) {
    add_error(result.report, "legacy.csv.unclosed_quote", "/lines/" + std::to_string(line),
              "The CSV input ended inside a quoted field.", "Add the missing closing quote.");
  }
  if (result.report.ok() && (!contents.empty() && contents.back() != '\n' && contents.back() != '\r')) {
    finish_row();
  }
  if (result.report.ok()) {
    result.table = std::move(table);
  }
  return result;
}

LegacyConfigReadResult read_legacy_config(std::string_view contents) {
  LegacyConfigReadResult result;
  LegacyConfig config;
  std::istringstream input{std::string(contents)};
  std::string line;
  std::size_t line_number = 0;

  while (std::getline(input, line)) {
    ++line_number;
    const std::string normalized = trim(line);
    if (normalized.empty() || normalized.starts_with('#')) {
      continue;
    }
    ++result.report.source_records;
    const std::size_t separator = normalized.find('=');
    const std::string path = "/lines/" + std::to_string(line_number);
    if (separator == std::string::npos) {
      add_error(result.report, "legacy.config.missing_separator", path,
                "Configuration entry does not contain '='.", "Use the form 'key = value'.");
      continue;
    }
    const std::string key = trim(std::string_view(normalized).substr(0, separator));
    const std::string value = trim(std::string_view(normalized).substr(separator + 1));
    if (key.empty()) {
      add_error(result.report, "legacy.config.empty_key", path,
                "Configuration entry has an empty key.", "Provide a key before '='.");
      continue;
    }
    if (!config.values.emplace(key, value).second) {
      add_error(result.report, "legacy.config.duplicate_key", path,
                "Configuration key '" + key + "' is defined more than once.",
                "Keep exactly one value for each configuration key.");
    }
  }

  if (result.report.ok()) {
    result.config = std::move(config);
  }
  return result;
}

}  // namespace schedmesh::import
