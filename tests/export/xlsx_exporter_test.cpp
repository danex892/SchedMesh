#include "schedmesh/export/xlsx_exporter.h"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>

#include "fixtures/tiny_project.h"

namespace schedmesh::exporting {
namespace {

constexpr std::streamoff kMinimumWorkbookSize = 1'000;

domain::Schedule tiny_schedule() {
  return {.meetings = {{.meeting = domain::MeetingId{"meeting-001"},
                        .start_slot = domain::SlotId{"slot-mon-p1"},
                        .teachers = {domain::TeacherId{"teacher-001"}},
                        .rooms = {domain::RoomId{"room-001"}}}}};
}

TEST(XlsxExporterTest, WritesValidatedWorkbook) {
  const std::filesystem::path output_path =
      std::filesystem::temp_directory_path() / "schedmesh-tiny-timetable.xlsx";
  std::filesystem::remove(output_path);

  const XlsxExportResult result =
      export_timetable_xlsx(test::make_tiny_project(), tiny_schedule(), output_path);

  ASSERT_TRUE(result.success) << result.error;
  EXPECT_EQ(result.worksheet_count, 2U);
  EXPECT_EQ(result.class_count, 1U);
  ASSERT_TRUE(std::filesystem::exists(output_path));
  std::ifstream input(output_path, std::ios::binary | std::ios::ate);
  EXPECT_GT(input.tellg(), kMinimumWorkbookSize);
  input.seekg(0);
  std::array<unsigned char, 4> signature{};
  input.read(reinterpret_cast<char*>(signature.data()),
             static_cast<std::streamsize>(signature.size()));
  EXPECT_EQ(signature, (std::array<unsigned char, 4>{0x50, 0x4B, 0x03, 0x04}));
  input.close();
  std::filesystem::remove(output_path);
}

TEST(XlsxExporterTest, RejectsInvalidScheduleBeforeWriting) {
  const std::filesystem::path output_path =
      std::filesystem::temp_directory_path() / "schedmesh-invalid-timetable.xlsx";
  std::filesystem::remove(output_path);
  domain::Schedule schedule = tiny_schedule();
  schedule.meetings.front().meeting = domain::MeetingId{"missing-meeting"};

  const XlsxExportResult result =
      export_timetable_xlsx(test::make_tiny_project(), schedule, output_path);

  EXPECT_FALSE(result.success);
  EXPECT_FALSE(result.error.empty());
  EXPECT_FALSE(std::filesystem::exists(output_path));
}

}  // namespace
}  // namespace schedmesh::exporting
