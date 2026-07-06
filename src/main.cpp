#include "scp_cli.hpp"
#include "progress_renderer.hpp"

#include "zssh/config/ConfigLoader.hpp"
#include "zssh/copy/CopyService.hpp"
#include "zssh/protocol/LibsshAdapter.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <unordered_map>

namespace {

std::string resolve_config_path() {
  if (const char* env = std::getenv("ZSSH_CONFIG_PATH"); env && *env) {
    return env;
  }
  if (std::filesystem::exists("zssh.yaml")) {
    return "zssh.yaml";
  }
  if (const char* home = std::getenv("HOME")) {
    auto path = std::filesystem::path(home) / ".config" / "zssh" / "zssh.yaml";
    if (std::filesystem::exists(path)) return path.string();
  }
  return "zssh.yaml";
}

zssh::protocol::SshConnectionConfig make_connection(
    const std::string& profile_name,
    const zssh::config::ProfileConfig& profile) {
  return {
    .profile_name = profile_name,
    .host = profile.host,
    .port = profile.port,
    .auth = {
      .method = profile.auth.method,
      .username = profile.auth.username,
      .password_command = profile.auth.password_command,
      .key_path = profile.auth.key_path,
      .use_agent = profile.auth.use_agent,
    },
  };
}

zssh::copy::FileInfo local_stat_file(const std::string& path) {
  std::error_code ec;
  auto size = std::filesystem::file_size(path, ec);
  bool is_dir = std::filesystem::is_directory(path, ec);
  return {path, ec ? std::uint64_t{0} : size, !ec && is_dir};
}

}  // namespace

int run_main(int argc, const char** argv) {
  auto parsed = zscp::parse_args(argc, argv);

  if (parsed.show_help) {
    std::cout << "zazaki_scp [options] source... destination\n"
              << "  profile://[user@]host:path\n"
              << "  -r -p --resume -j <n> --limit <rate>\n"
              << "  --include <pat> --exclude <pat>\n"
              << "  --json --no-progress --help\n";
    return 0;
  }

  if (!parsed.error_message.empty()) {
    std::cerr << "error: " << parsed.error_message << "\n";
    return 2;
  }

  try {
    auto config = zssh::config::load_config(resolve_config_path());
    auto profile_it = config.profiles.find(parsed.request.profile_name);
    if (profile_it == config.profiles.end()) {
      std::cerr << "error: unknown profile: " << parsed.request.profile_name << "\n";
      return 2;
    }

    auto connection = make_connection(parsed.request.profile_name, profile_it->second);
    zssh::protocol::LibsshAdapter ssh;
    ssh.connect(connection);

    zssh::copy::CopyService service(ssh);
    zscp::ProgressRenderer renderer;

    auto local_read = [&](const std::string& path, char* buf, std::uint64_t size) -> std::uint64_t {
      static std::unordered_map<std::string, std::ifstream> open_files;
      auto& fs = open_files[path];
      if (!fs.is_open()) {
        fs.open(path, std::ios::binary);
      }
      if (!fs) return 0;
      fs.read(buf, size);
      return fs.gcount();
    };

    auto local_write = [&](const std::string& path, const char* buf, std::uint64_t size) -> std::uint64_t {
      static std::unordered_map<std::string, std::ofstream> open_files;
      auto& fs = open_files[path];
      if (!fs.is_open()) {
        auto parent = std::filesystem::path(path).parent_path();
        if (!parent.empty()) std::filesystem::create_directories(parent);
        fs.open(path, std::ios::binary | std::ios::app);
      }
      if (!fs) return 0;
      fs.write(buf, size);
      return size;
    };

    bool ok = service.run(parsed.request, renderer, local_stat_file, local_read, local_write);

    if (!parsed.no_progress) {
      renderer.render_frame();
    }

    if (parsed.request.json_output) {
      auto& s = renderer.summary();
      std::cout << "{\"status\":\"" << (ok ? "ok" : "failed") << "\""
                << ",\"files\":" << s.succeeded
                << ",\"bytes\":" << s.total_bytes
                << ",\"elapsed\":" << s.elapsed_seconds << "}\n";
    }

    return ok ? 0 : 1;
  } catch (const std::exception& ex) {
    std::cerr << "error: " << ex.what() << "\n";
    return 1;
  }
}

#ifndef ZSCP_TEST_BUILD
int main(int argc, const char** argv) {
  return run_main(argc, argv);
}
#endif
