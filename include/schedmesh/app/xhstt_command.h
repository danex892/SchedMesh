#pragma once

#include <iosfwd>
#include <string_view>

namespace schedmesh::app {

int import_xhstt_file(std::string_view input_path, std::string_view project_output_path,
                      std::string_view schedule_output_path, std::ostream& output,
                      std::ostream& errors);

}  // namespace schedmesh::app
