#include "schedmesh/import/legacy_text.h"

#include <gtest/gtest.h>

namespace schedmesh::import {
namespace {

TEST(LegacyCsvTest, ReadsQuotedCommasEscapedQuotesAndNewlines) {
  const CsvReadResult result = read_legacy_csv(
      "Teacher,Subject,Hours\r\n\"Family, Given\",\"A \"\"quoted\"\" subject\",2\r\n"
      "Teacher 2,\"line one\nline two\",3");

  ASSERT_TRUE(result.ok());
  ASSERT_EQ(result.table->size(), 3U);
  EXPECT_EQ((*result.table)[1][0], "Family, Given");
  EXPECT_EQ((*result.table)[1][1], "A \"quoted\" subject");
  EXPECT_EQ((*result.table)[2][1], "line one\nline two");
  EXPECT_EQ(result.report.source_records, 3U);
}

TEST(LegacyCsvTest, RejectsMalformedQuotedFieldsWithoutPartialTable) {
  const CsvReadResult result = read_legacy_csv("Teacher,Subject\n\"Teacher 1,Math\n");

  EXPECT_FALSE(result.ok());
  EXPECT_FALSE(result.table.has_value());
  ASSERT_EQ(result.report.diagnostics.size(), 1U);
  EXPECT_EQ(result.report.diagnostics.front().code, "legacy.csv.unclosed_quote");
}

TEST(LegacyConfigTest, ReadsTrimmedValuesAndIgnoresComments) {
  const LegacyConfigReadResult result = read_legacy_config(
      "# scheduling dimensions\n days = 6\nmaxlessons=7\nfile = data/input.csv\n");

  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.config->values.at("days"), "6");
  EXPECT_EQ(result.config->values.at("maxlessons"), "7");
  EXPECT_EQ(result.config->values.at("file"), "data/input.csv");
  EXPECT_EQ(result.report.source_records, 3U);
}

TEST(LegacyConfigTest, RejectsDuplicateAndMalformedEntries) {
  const LegacyConfigReadResult result =
      read_legacy_config("days = 5\ndays = 6\nthis is not an entry\n");

  EXPECT_FALSE(result.ok());
  EXPECT_FALSE(result.config.has_value());
  ASSERT_EQ(result.report.diagnostics.size(), 2U);
  EXPECT_EQ(result.report.diagnostics[0].code, "legacy.config.duplicate_key");
  EXPECT_EQ(result.report.diagnostics[1].code, "legacy.config.missing_separator");
}

}  // namespace
}  // namespace schedmesh::import
