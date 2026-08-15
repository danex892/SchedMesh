#pragma once

#include <compare>
#include <functional>
#include <string>
#include <utility>

namespace schedmesh::domain {

template <typename Tag>
class Id {
 public:
  Id() = default;
  explicit Id(std::string value) : value_(std::move(value)) {}

  [[nodiscard]] const std::string& value() const noexcept { return value_; }
  [[nodiscard]] bool empty() const noexcept { return value_.empty(); }

  auto operator<=>(const Id&) const = default;

 private:
  std::string value_;
};

using TeacherId = Id<struct TeacherTag>;
using StudentGroupId = Id<struct StudentGroupTag>;
using RoomId = Id<struct RoomTag>;
using SubjectId = Id<struct SubjectTag>;
using MeetingId = Id<struct MeetingTag>;
using SlotId = Id<struct SlotTag>;

}  // namespace schedmesh::domain

template <typename Tag>
struct std::hash<schedmesh::domain::Id<Tag>> {
  std::size_t operator()(const schedmesh::domain::Id<Tag>& id) const noexcept {
    return std::hash<std::string>{}(id.value());
  }
};
