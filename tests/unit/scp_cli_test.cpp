#include <gtest/gtest.h>
#include "scp_cli.hpp"

TEST(ScpCliTest, ParsesSimpleLocalToRemote) {
  const char* argv[] = {"zazaki_scp", "./local.txt", "prod://:/tmp/remote.txt"};

  auto parsed = zscp::parse_args(3, argv);

  EXPECT_TRUE(parsed.error_message.empty());
  EXPECT_EQ(parsed.request.source_path, "./local.txt");
  EXPECT_EQ(parsed.request.destination_path, "/tmp/remote.txt");
  EXPECT_EQ(parsed.request.profile_name, "prod");
  EXPECT_EQ(parsed.request.direction, zssh::copy::CopyDirection::LocalToRemote);
}

TEST(ScpCliTest, ParsesRemoteToLocal) {
  const char* argv[] = {"zazaki_scp", "prod://:/var/log/app.log", "./logs/"};

  auto parsed = zscp::parse_args(3, argv);

  EXPECT_TRUE(parsed.error_message.empty());
  EXPECT_EQ(parsed.request.source_path, "/var/log/app.log");
  EXPECT_EQ(parsed.request.destination_path, "./logs/");
  EXPECT_EQ(parsed.request.profile_name, "prod");
  EXPECT_EQ(parsed.request.direction, zssh::copy::CopyDirection::RemoteToLocal);
}

TEST(ScpCliTest, ParsesFlags) {
  const char* argv[] = {"zazaki_scp", "-r", "-p", "--resume", "-j", "4",
                        "--limit", "10M", "./dir", "prod://:/backup/"};

  auto parsed = zscp::parse_args(10, argv);

  EXPECT_TRUE(parsed.request.recursive);
  EXPECT_TRUE(parsed.request.preserve_attrs);
  EXPECT_TRUE(parsed.request.resume);
  EXPECT_EQ(parsed.request.concurrency, 4);
  EXPECT_EQ(parsed.request.rate_limit_bytes_per_sec, 10 * 1024 * 1024);
}

TEST(ScpCliTest, ParsesIncludeExclude) {
  const char* argv[] = {"zazaki_scp", "--include", "*.txt", "--exclude", "*.tmp",
                        "./src", "prod://:/app/"};

  auto parsed = zscp::parse_args(8, argv);

  ASSERT_EQ(parsed.request.include_patterns.size(), 1);
  EXPECT_EQ(parsed.request.include_patterns[0], "*.txt");
  ASSERT_EQ(parsed.request.exclude_patterns.size(), 1);
  EXPECT_EQ(parsed.request.exclude_patterns[0], "*.tmp");
}

TEST(ScpCliTest, ShowHelp) {
  const char* argv[] = {"zazaki_scp", "--help"};
  auto parsed = zscp::parse_args(2, argv);
  EXPECT_TRUE(parsed.show_help);
}

TEST(ScpCliTest, MissingArgsReturnsError) {
  const char* argv[] = {"zazaki_scp", "./only_source"};
  auto parsed = zscp::parse_args(2, argv);
  EXPECT_FALSE(parsed.error_message.empty());
}
