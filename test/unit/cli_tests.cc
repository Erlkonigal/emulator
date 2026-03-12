#include "framework/test_framework.h"

#include <cstring>
#include <string>
#include <vector>

#include "emulator/runtime_config.h"

using emulator::RuntimeConfig;

namespace {

std::vector<char *> MakeArgv(const std::vector<std::string> &args,
                             std::vector<std::string> &storage) {
  storage = args;
  std::vector<char *> argv;
  argv.reserve(storage.size());
  for (auto &s : storage) {
    argv.push_back(s.data());
  }
  return argv;
}

}

TEST(cli_parse_no_flags) {
  std::vector<std::string> storage;
  std::vector<std::string> args = {"emulator", "--rom", "test.bin"};
  auto argv = MakeArgv(args, storage);

  auto &config = RuntimeConfig::getInstance();
  std::string error;
  bool ok = config.parseArgs(static_cast<int>(argv.size()), argv.data(), &error);
  ASSERT_TRUE(ok);
  EXPECT_TRUE(!config.debug);
}

TEST(cli_parse_debug_flag) {
  std::vector<std::string> storage;
  std::vector<std::string> args = {"emulator", "--rom", "test.bin", "--debug"};
  auto argv = MakeArgv(args, storage);

  auto &config = RuntimeConfig::getInstance();
  std::string error;
  bool ok = config.parseArgs(static_cast<int>(argv.size()), argv.data(), &error);
  ASSERT_TRUE(ok);
  EXPECT_TRUE(config.debug);
}

TEST(cli_parse_default_port_values) {
  std::vector<std::string> storage;
  std::vector<std::string> args = {"emulator", "--rom", "test.bin"};
  auto argv = MakeArgv(args, storage);

  auto &config = RuntimeConfig::getInstance();
  std::string error;
  bool ok = config.parseArgs(static_cast<int>(argv.size()), argv.data(), &error);
  ASSERT_TRUE(ok);
  EXPECT_EQ(config.debugPort, kDefaultDebugPort);
}

TEST(cli_parse_help_flag) {
  std::vector<std::string> storage;
  std::vector<std::string> args = {"emulator", "--help"};
  auto argv = MakeArgv(args, storage);

  auto &config = RuntimeConfig::getInstance();
  std::string error;
  bool ok = config.parseArgs(static_cast<int>(argv.size()), argv.data(), &error);
  ASSERT_TRUE(ok);
  EXPECT_TRUE(config.showHelp);
}

TEST(cli_parse_help_short_flag) {
  std::vector<std::string> storage;
  std::vector<std::string> args = {"emulator", "-h"};
  auto argv = MakeArgv(args, storage);

  auto &config = RuntimeConfig::getInstance();
  std::string error;
  bool ok = config.parseArgs(static_cast<int>(argv.size()), argv.data(), &error);
  ASSERT_TRUE(ok);
  EXPECT_TRUE(config.showHelp);
}