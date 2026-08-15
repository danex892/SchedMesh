#include "schedmesh/import/legacy_settings.h"

#include <gtest/gtest.h>

namespace schedmesh::import {
namespace {

LegacyConfig complete_config() {
  return {.values = {
              {"days", "6"},
              {"maxlessons", "7"},
              {"sessions", "2"},
              {"last_day_short", "1"},
              {"file", "data/input.csv"},
              {"classrooms_file", "data/rooms.csv"},
              {"physical_culture_name", "Sports"},
              {"days_of_the_week", "Monday / Tuesday / Wednesday / Thursday / Friday / Saturday"},
              {"double_lessons", "Technology / Art"},
              {"conflicts", "Sports / Safety, Chemistry / Biology"},
              {"steps", "100"}}};
}

TEST(LegacySettingsTest, DecodesSchedulingValuesAndAccountsForRuntimeKeys) {
  const LegacySettingsReadResult result = decode_legacy_settings(complete_config());

  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.settings->days, 6);
  EXPECT_EQ(result.settings->maximum_lessons_per_session, 7);
  EXPECT_EQ(result.settings->sessions, 2);
  EXPECT_TRUE(result.settings->last_day_short);
  ASSERT_EQ(result.settings->conflicts.size(), 2U);
  EXPECT_EQ(result.settings->conflicts[0].first, "Sports");
  EXPECT_EQ(result.report.consumed_fields, 10U);
  EXPECT_EQ(result.report.ignored_fields, 1U);
}

TEST(LegacySettingsTest, RejectsInvalidDimensionsAndMissingRequiredValues) {
  LegacyConfig config = complete_config();
  config.values["days"] = "six";
  config.values.erase("file");

  const LegacySettingsReadResult result = decode_legacy_settings(config);

  EXPECT_FALSE(result.ok());
  EXPECT_FALSE(result.settings.has_value());
  ASSERT_GE(result.report.diagnostics.size(), 2U);
  EXPECT_EQ(result.report.diagnostics[0].code, "legacy.config.invalid_integer");
  EXPECT_EQ(result.report.diagnostics[1].code, "legacy.config.required_key_missing");
}

TEST(LegacySettingsTest, ReportsUnknownKeysWithoutRejectingMigration) {
  LegacyConfig config = complete_config();
  config.values["custom_option"] = "enabled";

  const LegacySettingsReadResult result = decode_legacy_settings(config);

  ASSERT_TRUE(result.ok());
  ASSERT_EQ(result.report.diagnostics.size(), 1U);
  EXPECT_EQ(result.report.diagnostics[0].severity, MigrationSeverity::kWarning);
  EXPECT_EQ(result.report.diagnostics[0].code, "legacy.config.unknown_key");
  EXPECT_EQ(result.report.ignored_fields, 2U);
}

}  // namespace
}  // namespace schedmesh::import
