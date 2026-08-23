#pragma once

#include <string>

#include "schedmesh/domain/schedule.h"

namespace schedmesh::io {

[[nodiscard]] std::string write_schedule_json(const domain::Schedule& schedule);

}  // namespace schedmesh::io
