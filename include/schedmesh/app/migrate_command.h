#pragma once

#include <iosfwd>
#include <string_view>

namespace schedmesh::app {

int migrate_legacy_project(std::string_view settings_path, std::string_view output_path,
                           std::ostream& output, std::ostream& errors);

}  // namespace schedmesh::app
