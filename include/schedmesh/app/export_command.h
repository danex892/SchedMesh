#pragma once

#include <iosfwd>
#include <string_view>

namespace schedmesh::app {

int export_schedule_xlsx(std::string_view project_path, std::string_view schedule_path,
                         std::string_view output_path, std::ostream& output, std::ostream& errors);

}  // namespace schedmesh::app
