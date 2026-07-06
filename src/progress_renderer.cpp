#include "progress_renderer.hpp"

#include <iomanip>
#include <iostream>
#include <sstream>

namespace zscp {

void ProgressRenderer::on_progress(const zssh::copy::TransferProgress& progress) {
  for (auto& entry : entries_) {
    if (entry.path == progress.file_path) {
      entry.bytes_transferred = progress.bytes_transferred;
      entry.total_bytes = progress.total_bytes;
      entry.state = progress.state;
      return;
    }
  }
  entries_.push_back(FileProgressEntry{
      progress.file_path, progress.bytes_transferred, progress.total_bytes, progress.state});
}

void ProgressRenderer::on_file_complete(const std::string& path, bool success) {
  for (auto& entry : entries_) {
    if (entry.path == path) {
      entry.state = success ? zssh::copy::TransferState::Completed
                            : zssh::copy::TransferState::Failed;
      return;
    }
  }
}

void ProgressRenderer::on_transfer_complete(const zssh::copy::TransferSummary& summary) {
  summary_ = summary;
  complete_ = true;
}

void ProgressRenderer::render_frame() {
  for (const auto& entry : entries_) {
    std::string state_prefix;
    switch (entry.state) {
      case zssh::copy::TransferState::Queued: state_prefix = "[ ]"; break;
      case zssh::copy::TransferState::Transferring: state_prefix = "[>]"; break;
      case zssh::copy::TransferState::Completed: state_prefix = "[OK]"; break;
      case zssh::copy::TransferState::Failed: state_prefix = "[FAIL]"; break;
    }

    std::ostringstream line;
    line << state_prefix << " " << entry.path
         << " (" << entry.bytes_transferred << "/" << entry.total_bytes << ")";
    std::cout << line.str() << "\n";
  }

  if (complete_) {
    std::ostringstream oss;
    oss << "Complete: " << summary_.succeeded << "/" << summary_.total_files
        << " files, " << summary_.total_bytes << " bytes, "
        << std::fixed << std::setprecision(1) << summary_.elapsed_seconds << "s";
    std::cout << oss.str() << "\n";
  }
}

}  // namespace zscp
