#pragma once

#include <iosfwd>
#include <string_view>

namespace schedmesh::app {

inline constexpr int kExitSuccess = 0;
inline constexpr int kExitValidationError = 1;
inline constexpr int kExitUsageOrIoError = 2;

int validate_project_file(std::string_view path, std::ostream& output, std::ostream& errors);

}  // namespace schedmesh::app
