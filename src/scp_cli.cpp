#include "scp_cli.hpp"

#include <cstdlib>
#include <string_view>

namespace zscp {

namespace {

std::uint64_t parse_rate_limit(const std::string& s) {
  if (s.empty()) return 0;
  std::uint64_t multiplier = 1;
  std::string num_str = s;
  char suffix = s.back();
  if (suffix == 'K' || suffix == 'k') {
    multiplier = 1024;
    num_str = s.substr(0, s.size() - 1);
  } else if (suffix == 'M' || suffix == 'm') {
    multiplier = 1024 * 1024;
    num_str = s.substr(0, s.size() - 1);
  } else if (suffix == 'G' || suffix == 'g') {
    multiplier = 1024 * 1024 * 1024;
    num_str = s.substr(0, s.size() - 1);
  }
  return static_cast<std::uint64_t>(std::stod(num_str) * multiplier);
}

bool is_remote_path(const std::string& s) {
  return s.find("://") != std::string::npos || s.find('@') != std::string::npos ||
         (s.find(':') != std::string::npos && s.find("://") == std::string::npos);
}

void parse_remote_path(const std::string& s, zssh::copy::CopyRequest& req) {
  std::string remaining = s;

  auto proto_pos = remaining.find("://");
  if (proto_pos != std::string::npos) {
    req.profile_name = remaining.substr(0, proto_pos);
    remaining = remaining.substr(proto_pos + 3);
  }

  auto at_pos = remaining.find('@');
  if (at_pos != std::string::npos) {
    remaining = remaining.substr(at_pos + 1);
  }
}

}  // namespace

ParsedArgs parse_args(int argc, const char* const* argv) {
  ParsedArgs result;

  std::vector<std::string> positional;

  for (int i = 1; i < argc; ++i) {
    std::string_view arg(argv[i]);

    if (arg == "--help") {
      result.show_help = true;
      return result;
    } else if (arg == "-r") {
      result.request.recursive = true;
    } else if (arg == "-p") {
      result.request.preserve_attrs = true;
    } else if (arg == "--resume") {
      result.request.resume = true;
    } else if (arg == "--no-progress") {
      result.no_progress = true;
    } else if (arg == "--json") {
      result.request.json_output = true;
    } else if (arg == "-j" && i + 1 < argc) {
      result.request.concurrency = static_cast<std::uint32_t>(std::stoul(argv[++i]));
    } else if (arg == "--limit" && i + 1 < argc) {
      result.request.rate_limit_bytes_per_sec = parse_rate_limit(argv[++i]);
    } else if (arg == "--include" && i + 1 < argc) {
      result.request.include_patterns.push_back(argv[++i]);
    } else if (arg == "--exclude" && i + 1 < argc) {
      result.request.exclude_patterns.push_back(argv[++i]);
    } else if (arg[0] != '-') {
      positional.push_back(std::string(arg));
    } else {
      result.error_message = std::string("unknown flag: ") + std::string(arg);
      return result;
    }
  }

  if (positional.size() < 2) {
    result.error_message = "source and destination required";
    return result;
  }

  std::string source = positional[0];
  std::string dest = positional[1];

  bool source_remote = is_remote_path(source);
  bool dest_remote = is_remote_path(dest);

  if (source_remote && dest_remote) {
    result.error_message = "both source and destination are remote; remote-to-remote not supported";
    return result;
  }

  if (dest_remote) {
    result.request.source_path = source;
    parse_remote_path(dest, result.request);
    auto proto_end = dest.find("://");
    auto colon_search_start = (proto_end != std::string::npos) ? proto_end + 3 : 0;
    auto colon_pos = dest.find(':', colon_search_start);
    result.request.destination_path = dest.substr(colon_pos + 1);
    result.request.direction = zssh::copy::CopyDirection::LocalToRemote;
  } else {
    parse_remote_path(source, result.request);
    auto proto_end = source.find("://");
    auto colon_search_start = (proto_end != std::string::npos) ? proto_end + 3 : 0;
    auto colon_pos = source.find(':', colon_search_start);
    result.request.source_path = source.substr(colon_pos + 1);
    result.request.destination_path = dest;
    result.request.direction = zssh::copy::CopyDirection::RemoteToLocal;
  }

  return result;
}

}  // namespace zscp
