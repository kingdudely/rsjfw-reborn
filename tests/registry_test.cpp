#include <gtest/gtest.h>
#include "registry.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

class RegistryTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDir = fs::current_path() / "test_registry";
        fs::create_directories(testDir);
    }

    void TearDown() override {
        fs::remove_all(testDir);
    }

    fs::path testDir;
};

TEST_F(RegistryTest, ParseBasic) {
    std::string content = 
        "WINE REGISTRY Version 2\n"
        ";; All keys relative to REGISTRY\\\\Machine\n\n"
        "[Software\\\\Test] 1000000000\n"
        "\"StringValue\"=\"Hello\"\n"
        "\"DwordValue\"=dword:00000001\n";
    
    std::ofstream os(testDir / "system.reg");
    os << content;
    os.close();

    rsjfw::Registry reg(testDir.string());
    auto val = reg.query("HKLM\\Software\\Test", "StringValue");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), "Hello");

    auto dword = reg.query("HKLM\\Software\\Test", "DwordValue");
    // query returns string, but we can verify it exists
    ASSERT_TRUE(dword.has_value());
}

TEST_F(RegistryTest, AddAndCommit) {
    rsjfw::Registry reg(testDir.string());
    reg.add("HKCU\\Software\\NewKey", "NewValue", "NewData");
    ASSERT_TRUE(reg.commit());

    std::ifstream is(testDir / "user.reg");
    std::string line;
    bool found = false;
    while (std::getline(is, line)) {
        if (line.find("\"NewValue\"=\"NewData\"") != std::string::npos) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(RegistryTest, HexTypes) {
    std::string content = 
        "WINE REGISTRY Version 2\n"
        ";; All keys relative to REGISTRY\\\\Machine\n\n"
        "[Software\\\\Test] 1000000000\n"
        "\"BinaryValue\"=hex:01,02,03\n";
    
    std::ofstream os(testDir / "system.reg");
    os << content;
    os.close();

    rsjfw::Registry reg(testDir.string());
    auto val = reg.query("HKLM\\Software\\Test", "BinaryValue");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), "010203");
}
