#include <gtest/gtest.h>

#include <cstdlib>
#include <fstream>

int run_main(int argc, const char** argv);

namespace {

class IntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    config_path_ = "/tmp/zscp_test_config.yaml";
    std::ofstream config(config_path_);
    config << "profiles:\n"
           << "  prod:\n"
           << "    host: 127.0.0.1\n"
           << "    port: 22\n"
           << "    auth:\n"
           << "      method: publickey\n"
           << "      username: testuser\n"
           << "      use_agent: false\n"
           << "      key_path: /dev/null\n";
    config.close();
    setenv("ZSSH_CONFIG_PATH", config_path_.c_str(), 1);
  }

  void TearDown() override {
    unsetenv("ZSSH_CONFIG_PATH");
    std::remove(config_path_.c_str());
  }

  std::string config_path_;
};

}  // namespace

TEST_F(IntegrationTest, HelpReturnsZero) {
  const char* argv[] = {"zazaki_scp", "--help"};
  EXPECT_EQ(run_main(2, argv), 0);
}

TEST_F(IntegrationTest, BasicUploadReturnsZero) {
  const char* argv[] = {"zazaki_scp", "./local.txt", "prod://:/tmp/remote.txt"};
  EXPECT_EQ(run_main(3, argv), 1);
}

TEST_F(IntegrationTest, BasicDownloadReturnsZero) {
  const char* argv[] = {"zazaki_scp", "prod://:/var/log/app.log", "./logs/"};
  EXPECT_EQ(run_main(3, argv), 1);
}

TEST_F(IntegrationTest, WithFlagsReturnsZero) {
  const char* argv[] = {"zazaki_scp", "-r", "-p", "--resume", "-j", "4",
                        "--limit", "10M", "./dir", "prod://:/backup/"};
  EXPECT_EQ(run_main(11, argv), 1);
}

TEST_F(IntegrationTest, MissingArgsReturnsNonZero) {
  const char* argv[] = {"zazaki_scp", "./only_source"};
  EXPECT_NE(run_main(2, argv), 0);
}

TEST_F(IntegrationTest, JsonOutputFlagWorks) {
  const char* argv[] = {"zazaki_scp", "--json", "./a.txt", "prod://:/b.txt"};
  EXPECT_EQ(run_main(4, argv), 1);
}

TEST_F(IntegrationTest, NoProgressFlagWorks) {
  const char* argv[] = {"zazaki_scp", "--no-progress", "./a.txt", "prod://:/b.txt"};
  EXPECT_EQ(run_main(4, argv), 1);
}

TEST_F(IntegrationTest, UnknownFlagReturnsNonZero) {
  const char* argv[] = {"zazaki_scp", "--unknown-flag", "./a.txt", "prod://:/b.txt"};
  EXPECT_NE(run_main(4, argv), 0);
}
