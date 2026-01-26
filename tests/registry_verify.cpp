#include "registry.h"
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <iostream>

namespace fs = std::filesystem;

class RegistryVerifyTest : public ::testing::Test {
protected:
  void SetUp() override {
    testDir = fs::current_path() / "test_verify_prefix";
    fs::create_directories(testDir);
  }

  void TearDown() override { fs::remove_all(testDir); }

  fs::path testDir;
};

TEST_F(RegistryVerifyTest, RoundTripNoChanges) {
  std::string content = "WINE REGISTRY Version 2\n"
                        ";; All keys relative to REGISTRY\\\\Machine\n\n"
                        "#arch=win64\n\n"
                        "[Software\\\\Test] 1700000000\n"
                        "\"StringValue\"=\"Hello\"\n"
                        "\"DwordValue\"=dword:00000001\n"
                        "\"HexValue\"=hex:01,02,03\n\n"
                        "[Software\\\\Empty] 1700000000\n\n"
                        "[Software\\\\Sub\\\\Key] 1700000000\n"
                        "\"Val\"=\"1\"\n";

  fs::path regPath = testDir / "system.reg";
  std::ofstream os(regPath);
  os << content;
  os.close();

  rsjfw::Registry reg(testDir.string());

  reg.add("HKLM\\Software\\New", "Val", "2");

  ASSERT_TRUE(reg.commit());

  std::ifstream is(regPath);
  std::string newContent((std::istreambuf_iterator<char>(is)),
                         std::istreambuf_iterator<char>());

  EXPECT_NE(newContent.find("[Software\\\\Test]"), std::string::npos);
  EXPECT_NE(newContent.find("[Software\\\\Empty]"), std::string::npos);
  EXPECT_NE(newContent.find("[Software\\\\Sub\\\\Key]"), std::string::npos);
  EXPECT_NE(newContent.find("\"StringValue\"=\"Hello\""), std::string::npos);
  EXPECT_NE(newContent.find("\"DwordValue\"=dword:00000001"),
            std::string::npos);
  EXPECT_NE(newContent.find("\"HexValue\"=hex:01,02,03"), std::string::npos);
  EXPECT_NE(newContent.find("[Software\\\\New]"), std::string::npos);
}

TEST_F(RegistryVerifyTest, CaseInsensitivity) {
  std::string content = "WINE REGISTRY Version 2\n"
                        ";; All keys relative to REGISTRY\\\\Machine\n\n"
                        "[SOFTWARE\\\\CaseTest] 1700000000\n"
                        "\"Value\"=\"1\"\n";

  fs::path regPath = testDir / "system.reg";
  std::ofstream os(regPath);
  os << content;
  os.close();

  rsjfw::Registry reg(testDir.string());

  auto val = reg.query("HKLM\\software\\casetest", "value");
  ASSERT_TRUE(val.has_value());
  EXPECT_EQ(val.value(), "1");
}
