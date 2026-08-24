#include "schedmesh/app/export_command.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "schedmesh/app/validate_command.h"
#include "schedmesh/io/schedule_json.h"

namespace schedmesh::app {
namespace {

TEST(ExportCommandTest, ExportsCanonicalProjectAndSchedule) {
  const std::filesystem::path temporary = std::filesystem::temp_directory_path();
  const std::filesystem::path schedule_path = temporary / "schedmesh-export-schedule.json";
  const std::filesystem::path workbook_path = temporary / "schedmesh-export-timetable.xlsx";
  std::filesystem::remove(schedule_path);
  std::filesystem::remove(workbook_path);
  const domain::Schedule schedule = {.meetings = {{.meeting = domain::MeetingId{"meeting-001"},
                                                   .start_slot = domain::SlotId{"slot-mon-p1"},
                                                   .teachers = {domain::TeacherId{"teacher-001"}},
                                                   .rooms = {domain::RoomId{"room-001"}}}}};
  std::ofstream schedule_file(schedule_path, std::ios::binary);
  schedule_file << io::write_schedule_json(schedule);
  schedule_file.close();
  std::ostringstream output;
  std::ostringstream errors;

  const int exit_code =
      export_schedule_xlsx("tests/fixtures/tiny_project.json", schedule_path.string(),
                           workbook_path.string(), output, errors);

  EXPECT_EQ(exit_code, kExitSuccess);
  EXPECT_TRUE(errors.str().empty());
  EXPECT_NE(output.str().find("Exported 1 classes"), std::string::npos);
  EXPECT_TRUE(std::filesystem::exists(workbook_path));
  std::filesystem::remove(schedule_path);
  std::filesystem::remove(workbook_path);
}

}  // namespace
}  // namespace schedmesh::app
