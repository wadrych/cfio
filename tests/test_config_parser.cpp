/// @file test_config_parser.cpp
/// @brief Unit tests for JSON/CSV parsers, ParserFactory, and round-trip.

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "config/config_validator.h"
#include "config/csv_parser.h"
#include "config/json_parser.h"
#include "config/parser_factory.h"

namespace cfio {

class ConfigParserTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
    temp_dir_ = std::filesystem::path(::testing::TempDir()) / ("cfio_" + std::string(info->name()));
    std::filesystem::create_directories(temp_dir_);
  }

  void TearDown() override { std::filesystem::remove_all(temp_dir_); }

  std::filesystem::path WriteFile(const std::string& name, const std::string& content) {
    auto path = temp_dir_ / name;
    std::ofstream out(path);
    out << content;
    return path;
  }

  std::filesystem::path temp_dir_;
};

namespace {

// JsonParser tests

TEST_F(ConfigParserTest, JsonValidMultiJob) {
  auto path = WriteFile("multi.json", R"({
    "jobs": [
      {"name":"j1","engine":"io_uring","rw":"randread","bs":"4k","size":"1g",
       "iodepth":32,"direct":true,"rwmixread":100,"filename":"/tmp/f1.dat",
       "align":"4k"},
      {"name":"j2","engine":"psync","rw":"write","bs":"128k","size":"2g",
       "iodepth":1,"direct":false,"rwmixread":50,"filename":"/tmp/f2.dat",
       "align":"4k"}
    ]
  })");

  JsonParser parser;
  auto configs = parser.Parse(path);
  ASSERT_EQ(configs.size(), 2u);

  EXPECT_EQ(configs[0].name, "j1");
  EXPECT_EQ(configs[0].engine, "io_uring");
  EXPECT_EQ(configs[0].rw_mode, RWMode::kRandRead);
  EXPECT_EQ(configs[0].access_pattern, AccessPattern::kRandom);
  EXPECT_EQ(configs[0].block_size, 4096u);
  EXPECT_EQ(configs[0].file_size, 1073741824u);
  EXPECT_EQ(configs[0].iodepth, 32);
  EXPECT_TRUE(configs[0].direct);
  EXPECT_EQ(configs[0].rwmixread, 100);
  EXPECT_EQ(configs[0].filename.string(), "/tmp/f1.dat");
  EXPECT_EQ(configs[0].alignment, 4096u);

  EXPECT_EQ(configs[1].name, "j2");
  EXPECT_EQ(configs[1].engine, "psync");
  EXPECT_EQ(configs[1].rw_mode, RWMode::kWrite);
  EXPECT_EQ(configs[1].access_pattern, AccessPattern::kSequential);
  EXPECT_EQ(configs[1].block_size, 131072u);
  EXPECT_EQ(configs[1].file_size, 2147483648u);
  EXPECT_EQ(configs[1].iodepth, 1);
  EXPECT_FALSE(configs[1].direct);
  EXPECT_EQ(configs[1].rwmixread, 50);
  EXPECT_EQ(configs[1].filename.string(), "/tmp/f2.dat");
  EXPECT_EQ(configs[1].alignment, 4096u);
}

TEST_F(ConfigParserTest, JsonDefaultsApplied) {
  auto path = WriteFile("defaults.json", R"({
    "jobs":[{"name":"def","engine":"psync","rw":"read","bs":"4k","size":"1g"}]
  })");

  JsonParser parser;
  auto configs = parser.Parse(path);
  ASSERT_EQ(configs.size(), 1u);

  EXPECT_EQ(configs[0].rw_mode, RWMode::kRead);
  EXPECT_EQ(configs[0].access_pattern, AccessPattern::kSequential);
  EXPECT_EQ(configs[0].iodepth, 1);
  EXPECT_TRUE(configs[0].direct);
  EXPECT_EQ(configs[0].rwmixread, 50);
  EXPECT_EQ(configs[0].filename.string(), "./cfio-def.dat");
  EXPECT_EQ(configs[0].alignment, 4096u);
}

TEST_F(ConfigParserTest, JsonEmptyJobsArray) {
  auto path = WriteFile("empty.json", R"({"jobs":[]})");

  JsonParser parser;
  std::vector<JobConfig> result;
  EXPECT_NO_THROW(result = parser.Parse(path));
  EXPECT_TRUE(result.empty());
}

TEST_F(ConfigParserTest, JsonMissingRequiredName) {
  auto path = WriteFile("t.json", R"({
    "jobs":[{"engine":"psync","rw":"read","bs":"4k","size":"1g"}]
  })");
  JsonParser parser;
  EXPECT_THROW(parser.Parse(path), std::runtime_error);
}

TEST_F(ConfigParserTest, JsonMissingRequiredEngine) {
  auto path = WriteFile("t.json", R"({
    "jobs":[{"name":"x","rw":"read","bs":"4k","size":"1g"}]
  })");
  JsonParser parser;
  EXPECT_THROW(parser.Parse(path), std::runtime_error);
}

TEST_F(ConfigParserTest, JsonMissingRequiredRw) {
  auto path = WriteFile("t.json", R"({
    "jobs":[{"name":"x","engine":"psync","bs":"4k","size":"1g"}]
  })");
  JsonParser parser;
  EXPECT_THROW(parser.Parse(path), std::runtime_error);
}

TEST_F(ConfigParserTest, JsonMissingRequiredBs) {
  auto path = WriteFile("t.json", R"({
    "jobs":[{"name":"x","engine":"psync","rw":"read","size":"1g"}]
  })");
  JsonParser parser;
  EXPECT_THROW(parser.Parse(path), std::runtime_error);
}

TEST_F(ConfigParserTest, JsonMissingRequiredSize) {
  auto path = WriteFile("t.json", R"({
    "jobs":[{"name":"x","engine":"psync","rw":"read","bs":"4k"}]
  })");
  JsonParser parser;
  EXPECT_THROW(parser.Parse(path), std::runtime_error);
}

TEST_F(ConfigParserTest, JsonBadRwValue) {
  auto path = WriteFile("t.json", R"({
    "jobs":[{"name":"x","engine":"psync","rw":"invalid","bs":"4k","size":"1g"}]
  })");
  JsonParser parser;
  EXPECT_THROW(parser.Parse(path), std::invalid_argument);
}

TEST_F(ConfigParserTest, JsonBadBsSuffix) {
  // 'g' suffix rejected by kKM restriction on block size.
  auto path = WriteFile("t.json", R"({
    "jobs":[{"name":"x","engine":"psync","rw":"read","bs":"4g","size":"1g"}]
  })");
  JsonParser parser;
  EXPECT_THROW(parser.Parse(path), std::invalid_argument);
}

TEST_F(ConfigParserTest, JsonWrongFieldType) {
  // Integer where string expected — json::type_error → runtime_error.
  auto path = WriteFile("t.json", R"({
    "jobs":[{"name":"x","engine":"psync","rw":42,"bs":"4k","size":"1g"}]
  })");
  JsonParser parser;
  EXPECT_THROW(parser.Parse(path), std::runtime_error);
}

TEST_F(ConfigParserTest, JsonMalformedJson) {
  auto path = WriteFile("t.json", "not { valid json");
  JsonParser parser;
  EXPECT_THROW(parser.Parse(path), std::runtime_error);
}

TEST_F(ConfigParserTest, JsonRootNotObject) {
  auto path = WriteFile("t.json", "[1,2,3]");
  JsonParser parser;
  EXPECT_THROW(parser.Parse(path), std::runtime_error);
}

TEST_F(ConfigParserTest, JsonMissingJobsKey) {
  auto path = WriteFile("t.json", "{}");
  JsonParser parser;
  EXPECT_THROW(parser.Parse(path), std::runtime_error);
}

TEST_F(ConfigParserTest, JsonJobsNotArray) {
  auto path = WriteFile("t.json", R"({"jobs":"hello"})");
  JsonParser parser;
  EXPECT_THROW(parser.Parse(path), std::runtime_error);
}

TEST_F(ConfigParserTest, JsonNonexistentFile) {
  JsonParser parser;
  EXPECT_THROW(parser.Parse("/nonexistent/path.json"), std::runtime_error);
}

TEST_F(ConfigParserTest, JsonEmptyFile) {
  auto path = WriteFile("t.json", "");
  JsonParser parser;
  EXPECT_THROW(parser.Parse(path), std::runtime_error);
}

// CsvParser tests

TEST_F(ConfigParserTest, CsvValidMultiJob) {
  auto path = WriteFile("multi.csv",
                        "name,engine,rw,bs,size,iodepth,direct,rwmixread,filename,align\n"
                        "j1,io_uring,randread,4k,1g,32,true,100,/tmp/f1.dat,4k\n"
                        "j2,psync,write,128k,2g,1,false,50,/tmp/f2.dat,4k\n");

  CsvParser parser;
  auto configs = parser.Parse(path);
  ASSERT_EQ(configs.size(), 2u);

  EXPECT_EQ(configs[0].name, "j1");
  EXPECT_EQ(configs[0].engine, "io_uring");
  EXPECT_EQ(configs[0].rw_mode, RWMode::kRandRead);
  EXPECT_EQ(configs[0].access_pattern, AccessPattern::kRandom);
  EXPECT_EQ(configs[0].block_size, 4096u);
  EXPECT_EQ(configs[0].file_size, 1073741824u);
  EXPECT_EQ(configs[0].iodepth, 32);
  EXPECT_TRUE(configs[0].direct);
  EXPECT_EQ(configs[0].rwmixread, 100);
  EXPECT_EQ(configs[0].filename.string(), "/tmp/f1.dat");
  EXPECT_EQ(configs[0].alignment, 4096u);

  EXPECT_EQ(configs[1].name, "j2");
  EXPECT_EQ(configs[1].engine, "psync");
  EXPECT_EQ(configs[1].rw_mode, RWMode::kWrite);
  EXPECT_EQ(configs[1].access_pattern, AccessPattern::kSequential);
  EXPECT_EQ(configs[1].block_size, 131072u);
  EXPECT_EQ(configs[1].file_size, 2147483648u);
  EXPECT_EQ(configs[1].iodepth, 1);
  EXPECT_FALSE(configs[1].direct);
  EXPECT_EQ(configs[1].rwmixread, 50);
  EXPECT_EQ(configs[1].filename.string(), "/tmp/f2.dat");
  EXPECT_EQ(configs[1].alignment, 4096u);
}

TEST_F(ConfigParserTest, CsvDefaultsApplied) {
  auto path = WriteFile("defaults.csv",
                        "name,engine,rw,bs,size\n"
                        "def,psync,read,4k,1g\n");

  CsvParser parser;
  auto configs = parser.Parse(path);
  ASSERT_EQ(configs.size(), 1u);

  EXPECT_EQ(configs[0].rw_mode, RWMode::kRead);
  EXPECT_EQ(configs[0].access_pattern, AccessPattern::kSequential);
  EXPECT_EQ(configs[0].iodepth, 1);
  EXPECT_TRUE(configs[0].direct);
  EXPECT_EQ(configs[0].rwmixread, 50);
  EXPECT_EQ(configs[0].filename.string(), "./cfio-def.dat");
  EXPECT_EQ(configs[0].alignment, 4096u);
}

TEST_F(ConfigParserTest, CsvEmptyOptionalCells) {
  auto path = WriteFile("empty_opt.csv",
                        "name,engine,rw,bs,size,iodepth,direct,rwmixread,filename,align\n"
                        "e,psync,read,4k,1g,,,,,\n");

  CsvParser parser;
  auto configs = parser.Parse(path);
  ASSERT_EQ(configs.size(), 1u);

  EXPECT_EQ(configs[0].iodepth, 1);
  EXPECT_TRUE(configs[0].direct);
  EXPECT_EQ(configs[0].rwmixread, 50);
  EXPECT_EQ(configs[0].filename.string(), "./cfio-e.dat");
  EXPECT_EQ(configs[0].alignment, 4096u);
}

TEST_F(ConfigParserTest, CsvHeaderOnly) {
  auto path = WriteFile("header.csv", "name,engine,rw,bs,size\n");

  CsvParser parser;
  std::vector<JobConfig> result;
  EXPECT_NO_THROW(result = parser.Parse(path));
  EXPECT_TRUE(result.empty());
}

TEST_F(ConfigParserTest, CsvMissingRequiredColumn) {
  auto path = WriteFile("t.csv",
                        "name,rw,bs,size\n"
                        "x,read,4k,1g\n");
  CsvParser parser;
  EXPECT_THROW(parser.Parse(path), std::runtime_error);
}

TEST_F(ConfigParserTest, CsvEmptyRequiredName) {
  auto path = WriteFile("t.csv",
                        "name,engine,rw,bs,size\n"
                        ",psync,read,4k,1g\n");
  CsvParser parser;
  EXPECT_THROW(parser.Parse(path), std::runtime_error);
}

TEST_F(ConfigParserTest, CsvEmptyRequiredEngine) {
  auto path = WriteFile("t.csv",
                        "name,engine,rw,bs,size\n"
                        "x,,read,4k,1g\n");
  CsvParser parser;
  EXPECT_THROW(parser.Parse(path), std::runtime_error);
}

TEST_F(ConfigParserTest, CsvBadRwValue) {
  auto path = WriteFile("t.csv",
                        "name,engine,rw,bs,size\n"
                        "x,psync,invalid,4k,1g\n");
  CsvParser parser;
  EXPECT_THROW(parser.Parse(path), std::invalid_argument);
}

TEST_F(ConfigParserTest, CsvBadDirectValue) {
  auto path = WriteFile("t.csv",
                        "name,engine,rw,bs,size,direct\n"
                        "x,psync,read,4k,1g,yes\n");
  CsvParser parser;
  EXPECT_THROW(parser.Parse(path), std::invalid_argument);
}

TEST_F(ConfigParserTest, CsvBadIodepthNotInt) {
  auto path = WriteFile("t.csv",
                        "name,engine,rw,bs,size,iodepth\n"
                        "x,psync,read,4k,1g,abc\n");
  CsvParser parser;
  EXPECT_THROW(parser.Parse(path), std::invalid_argument);
}

TEST_F(ConfigParserTest, CsvIodepthTrailingJunk) {
  auto path = WriteFile("t.csv",
                        "name,engine,rw,bs,size,iodepth\n"
                        "x,psync,read,4k,1g,32x\n");
  CsvParser parser;
  EXPECT_THROW(parser.Parse(path), std::invalid_argument);
}

TEST_F(ConfigParserTest, CsvBadRwmixread) {
  auto path = WriteFile("t.csv",
                        "name,engine,rw,bs,size,rwmixread\n"
                        "x,psync,read,4k,1g,abc\n");
  CsvParser parser;
  EXPECT_THROW(parser.Parse(path), std::invalid_argument);
}

TEST_F(ConfigParserTest, CsvBadBsSuffix) {
  auto path = WriteFile("t.csv",
                        "name,engine,rw,bs,size\n"
                        "x,psync,read,4g,1g\n");
  CsvParser parser;
  EXPECT_THROW(parser.Parse(path), std::invalid_argument);
}

TEST_F(ConfigParserTest, CsvNonexistentFile) {
  CsvParser parser;
  EXPECT_THROW(parser.Parse("/nonexistent/path.csv"), std::runtime_error);
}

TEST_F(ConfigParserTest, CsvColumnOrderIndependence) {
  // Shuffled column order — csv-parser uses column names, not positions.
  auto path = WriteFile("shuffled.csv",
                        "size,name,bs,engine,rw\n"
                        "1g,x,4k,psync,read\n");

  CsvParser parser;
  auto configs = parser.Parse(path);
  ASSERT_EQ(configs.size(), 1u);

  EXPECT_EQ(configs[0].name, "x");
  EXPECT_EQ(configs[0].engine, "psync");
  EXPECT_EQ(configs[0].rw_mode, RWMode::kRead);
  EXPECT_EQ(configs[0].block_size, 4096u);
  EXPECT_EQ(configs[0].file_size, 1073741824u);
}

// ParserFactory tests

TEST(ParserFactoryTest, JsonExtension) {
  auto parser = ParserFactory::Create("test.json");
  EXPECT_NE(parser, nullptr);
}

TEST(ParserFactoryTest, CsvExtension) {
  auto parser = ParserFactory::Create("test.csv");
  EXPECT_NE(parser, nullptr);
}

TEST(ParserFactoryTest, JsonUpperCase) {
  auto parser = ParserFactory::Create("test.JSON");
  EXPECT_NE(parser, nullptr);
}

TEST(ParserFactoryTest, CsvMixedCase) {
  auto parser = ParserFactory::Create("test.Csv");
  EXPECT_NE(parser, nullptr);
}

TEST(ParserFactoryTest, UnknownExtension) {
  EXPECT_THROW(ParserFactory::Create("test.txt"), std::invalid_argument);
}

TEST(ParserFactoryTest, NoExtension) {
  EXPECT_THROW(ParserFactory::Create("config"), std::invalid_argument);
}

// Integration tests

TEST_F(ConfigParserTest, JsonParseValidate) {
  auto path = WriteFile("rt.json", R"({
    "jobs": [
      {"name":"rt1","engine":"io_uring","rw":"randread","bs":"4k","size":"1m",
       "iodepth":32,"direct":true,"rwmixread":70,
       "filename":"/tmp/rt1.dat","align":"4k"},
      {"name":"rt2","engine":"psync","rw":"write","bs":"8k","size":"1m",
       "iodepth":1,"direct":false,"rwmixread":50,
       "filename":"/tmp/rt2.dat","align":"4k"}
    ]
  })");

  JsonParser parser;
  auto configs = parser.Parse(path);
  EXPECT_NO_THROW(ConfigValidator::ValidateAll(configs));
  ASSERT_EQ(configs.size(), 2u);

  EXPECT_EQ(configs[0].name, "rt1");
  EXPECT_EQ(configs[0].engine, "io_uring");
  EXPECT_EQ(configs[0].rw_mode, RWMode::kRandRead);
  EXPECT_EQ(configs[0].block_size, 4096u);
  EXPECT_EQ(configs[0].file_size, 1048576u);

  EXPECT_EQ(configs[1].name, "rt2");
  EXPECT_EQ(configs[1].engine, "psync");
  EXPECT_EQ(configs[1].rw_mode, RWMode::kWrite);
  EXPECT_EQ(configs[1].block_size, 8192u);
  EXPECT_EQ(configs[1].file_size, 1048576u);
}

TEST_F(ConfigParserTest, CsvParseValidate) {
  auto path = WriteFile("rt.csv",
                        "name,engine,rw,bs,size,iodepth,direct,rwmixread,filename,align\n"
                        "rt1,io_uring,randread,4k,1m,32,true,70,/tmp/rt1.dat,4k\n"
                        "rt2,psync,write,8k,1m,1,false,50,/tmp/rt2.dat,4k\n");

  CsvParser parser;
  auto configs = parser.Parse(path);
  EXPECT_NO_THROW(ConfigValidator::ValidateAll(configs));
  ASSERT_EQ(configs.size(), 2u);

  EXPECT_EQ(configs[0].name, "rt1");
  EXPECT_EQ(configs[0].engine, "io_uring");
  EXPECT_EQ(configs[0].rw_mode, RWMode::kRandRead);
  EXPECT_EQ(configs[0].block_size, 4096u);
  EXPECT_EQ(configs[0].file_size, 1048576u);

  EXPECT_EQ(configs[1].name, "rt2");
  EXPECT_EQ(configs[1].engine, "psync");
  EXPECT_EQ(configs[1].rw_mode, RWMode::kWrite);
  EXPECT_EQ(configs[1].block_size, 8192u);
  EXPECT_EQ(configs[1].file_size, 1048576u);
}

}  // namespace
}  // namespace cfio
