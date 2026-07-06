#pragma once

#include "zssh/copy/CopyRequest.hpp"

#include <string>
#include <vector>

namespace zscp {

struct ParsedArgs {
  zssh::copy::CopyRequest request;
  bool show_help{false};
  bool no_progress{false};
  std::string error_message;
};

ParsedArgs parse_args(int argc, const char* const* argv);

}  // namespace zscp
