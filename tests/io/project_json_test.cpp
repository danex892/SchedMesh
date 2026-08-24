#include "schedmesh/io/project_json.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "fixtures/tiny_project.h"

namespace schedmesh::io {
namespace {

TEST(ProjectJsonTest, RoundTripsTinyProjectByteStably) {
  const std::string first = write_project_json(test::make_tiny_project());

  const ProjectReadResult parsed = read_project_json(first);

  ASSERT_TRUE(parsed.ok()) << (parsed.validation.diagnostics.empty()
                                   ? "no diagnostic"
                                   : parsed.validation.diagnostics.front().message);
  ASSERT_TRUE(parsed.project.has_value());
  EXPECT_EQ(write_project_json(*parsed.project), first);
  EXPECT_EQ(first.back(), '\n');
}

TEST(ProjectJsonTest, RoundTripsOptionalSubjectDailyOccurrenceLimit) {
  domain::Project project = test::make_tiny_project();
  project.subjects.front().maximum_occurrences_per_day = 2;
  project.meetings.front().simultaneity_keys = {"linked-lessons"};
  project.meetings.front().resource_lanes_aligned = false;

  const ProjectReadResult parsed = read_project_json(write_project_json(project));

  ASSERT_TRUE(parsed.ok());
  ASSERT_TRUE(parsed.project.has_value());
  EXPECT_EQ(parsed.project->subjects.front().maximum_occurrences_per_day, 2);
  EXPECT_EQ(parsed.project->meetings.front().simultaneity_keys,
            (std::vector<std::string>{"linked-lessons"}));
  EXPECT_FALSE(parsed.project->meetings.front().resource_lanes_aligned);
}

TEST(ProjectJsonTest, PublicFixtureMatchesCanonicalWriterByteForByte) {
  const auto fixture = std::filesystem::path{"tests"} / "fixtures" / "tiny_project.json";
  std::ifstream input(fixture, std::ios::binary);
  ASSERT_TRUE(input);
  const std::string fixture_contents{std::istreambuf_iterator<char>{input},
                                     std::istreambuf_iterator<char>{}};

  const ProjectReadResult parsed = read_project_json(fixture_contents);

  ASSERT_TRUE(parsed.ok()) << (parsed.validation.diagnostics.empty()
                                   ? "no diagnostic"
                                   : parsed.validation.diagnostics.front().message);
  ASSERT_TRUE(parsed.project.has_value());
  EXPECT_EQ(write_project_json(*parsed.project), fixture_contents);
  EXPECT_EQ(write_project_json(test::make_tiny_project()), fixture_contents);
}

TEST(ProjectJsonTest, ReportsMalformedJsonWithoutPartialProject) {
  const ProjectReadResult parsed = read_project_json(R"({"schema_version": 1,})");

  EXPECT_FALSE(parsed.ok());
  EXPECT_FALSE(parsed.project.has_value());
  ASSERT_EQ(parsed.validation.diagnostics.size(), 1U);
  EXPECT_EQ(parsed.validation.diagnostics.front().code, "json.invalid_document");
}

TEST(ProjectJsonTest, RejectsUnknownTopLevelField) {
  std::string json = write_project_json(test::make_tiny_project());
  const std::size_t insertion = json.find('{') + 1;
  json.insert(insertion, R"("future_field": true,)");

  const ProjectReadResult parsed = read_project_json(json);

  EXPECT_FALSE(parsed.ok());
  EXPECT_TRUE(parsed.project.has_value())
      << (parsed.validation.diagnostics.empty() ? "no diagnostic"
                                                : parsed.validation.diagnostics.front().message);
  ASSERT_FALSE(parsed.validation.diagnostics.empty());
  EXPECT_EQ(parsed.validation.diagnostics.front().code, "json.unknown_field");
  EXPECT_EQ(parsed.validation.diagnostics.front().path, "/future_field");
}

TEST(ProjectJsonTest, ReportsStructurallyInvalidReferencesAfterParsing) {
  auto project = test::make_tiny_project();
  project.meetings.front().subject = domain::SubjectId{"subject-missing"};

  const ProjectReadResult parsed = read_project_json(write_project_json(project));

  EXPECT_FALSE(parsed.ok());
  EXPECT_TRUE(parsed.project.has_value())
      << (parsed.validation.diagnostics.empty() ? "no diagnostic"
                                                : parsed.validation.diagnostics.front().message);
  ASSERT_FALSE(parsed.validation.diagnostics.empty());
  EXPECT_EQ(parsed.validation.diagnostics.front().code, "project.unknown_reference");
  EXPECT_EQ(parsed.validation.diagnostics.front().path, "/meetings/0/subject");
}

}  // namespace
}  // namespace schedmesh::io
