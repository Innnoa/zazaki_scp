#include <gtest/gtest.h>
#include "progress_renderer.hpp"

TEST(ProgressRendererTest, TracksSingleFileProgress) {
  zscp::ProgressRenderer renderer;

  zssh::copy::TransferProgress progress;
  progress.file_path = "/tmp/test.txt";
  progress.bytes_transferred = 512;
  progress.total_bytes = 1024;
  progress.state = zssh::copy::TransferState::Transferring;

  renderer.on_progress(progress);

  ASSERT_EQ(renderer.entries().size(), 1);
  EXPECT_EQ(renderer.entries()[0].path, "/tmp/test.txt");
  EXPECT_EQ(renderer.entries()[0].bytes_transferred, 512);
  EXPECT_EQ(renderer.entries()[0].total_bytes, 1024);
  EXPECT_EQ(renderer.entries()[0].state, zssh::copy::TransferState::Transferring);
}

TEST(ProgressRendererTest, UpdatesExistingEntry) {
  zscp::ProgressRenderer renderer;

  zssh::copy::TransferProgress p1;
  p1.file_path = "/tmp/a.txt";
  p1.bytes_transferred = 100;
  p1.total_bytes = 500;
  p1.state = zssh::copy::TransferState::Transferring;
  renderer.on_progress(p1);

  zssh::copy::TransferProgress p2;
  p2.file_path = "/tmp/a.txt";
  p2.bytes_transferred = 300;
  p2.total_bytes = 500;
  p2.state = zssh::copy::TransferState::Transferring;
  renderer.on_progress(p2);

  ASSERT_EQ(renderer.entries().size(), 1);
  EXPECT_EQ(renderer.entries()[0].bytes_transferred, 300);
}

TEST(ProgressRendererTest, MarksFileCompleteCorrectly) {
  zscp::ProgressRenderer renderer;

  zssh::copy::TransferProgress progress;
  progress.file_path = "/tmp/a.txt";
  progress.state = zssh::copy::TransferState::Transferring;
  renderer.on_progress(progress);

  renderer.on_file_complete("/tmp/a.txt", true);

  EXPECT_EQ(renderer.entries()[0].state, zssh::copy::TransferState::Completed);
}

TEST(ProgressRendererTest, MarksFileFailedCorrectly) {
  zscp::ProgressRenderer renderer;

  zssh::copy::TransferProgress progress;
  progress.file_path = "/tmp/b.txt";
  progress.state = zssh::copy::TransferState::Transferring;
  renderer.on_progress(progress);

  renderer.on_file_complete("/tmp/b.txt", false);

  EXPECT_EQ(renderer.entries()[0].state, zssh::copy::TransferState::Failed);
}

TEST(ProgressRendererTest, MarksCompleteAfterSummary) {
  zscp::ProgressRenderer renderer;

  EXPECT_FALSE(renderer.is_complete());

  zssh::copy::TransferSummary summary;
  summary.total_files = 3;
  summary.succeeded = 3;
  summary.total_bytes = 4096;
  summary.elapsed_seconds = 1.5;
  renderer.on_transfer_complete(summary);

  EXPECT_TRUE(renderer.is_complete());
  EXPECT_EQ(renderer.summary().total_files, 3);
  EXPECT_EQ(renderer.summary().succeeded, 3);
  EXPECT_EQ(renderer.summary().total_bytes, 4096);
  EXPECT_DOUBLE_EQ(renderer.summary().elapsed_seconds, 1.5);
}
