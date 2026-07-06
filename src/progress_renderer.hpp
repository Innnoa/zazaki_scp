#pragma once

#include "zssh/copy/IProgressSink.hpp"

#include <string>
#include <vector>

namespace zscp {

struct FileProgressEntry {
  std::string path;
  std::uint64_t bytes_transferred{0};
  std::uint64_t total_bytes{0};
  zssh::copy::TransferState state{zssh::copy::TransferState::Queued};
};

class ProgressRenderer : public zssh::copy::IProgressSink {
 public:
  ProgressRenderer() = default;

  void on_progress(const zssh::copy::TransferProgress& progress) override;
  void on_file_complete(const std::string& path, bool success) override;
  void on_transfer_complete(const zssh::copy::TransferSummary& summary) override;

  void render_frame();

  bool is_complete() const { return complete_; }
  const std::vector<FileProgressEntry>& entries() const { return entries_; }
  const zssh::copy::TransferSummary& summary() const { return summary_; }

 private:
  std::vector<FileProgressEntry> entries_;
  zssh::copy::TransferSummary summary_;
  bool complete_{false};
};

}  // namespace zscp
