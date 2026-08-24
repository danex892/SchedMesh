#pragma once

#include <filesystem>
#include <string>

#include "schedmesh/domain/project.h"
#include "schedmesh/domain/schedule.h"

namespace schedmesh::exporting {

struct XlsxExportResult {
  bool success{};
  std::string error;
  std::size_t worksheet_count{};
  std::size_t class_count{};
};

[[nodiscard]] XlsxExportResult export_timetable_xlsx(const domain::Project& project,
                                                     const domain::Schedule& schedule,
                                                     const std::filesystem::path& output_path);

}  // namespace schedmesh::exporting
