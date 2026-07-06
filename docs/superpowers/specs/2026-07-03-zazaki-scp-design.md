# zazaki_scp Design

Date: 2026-07-03
Status: accepted design baseline

## 1. Overview

`zazaki_scp` is a thin CLI facade for file transfer. It provides `scp`-style argument conventions and an FTXUI-based transfer progress UI, while delegating all SSH transport, authentication, session management, and copy logic to `zazaki_ssh`'s reusable copy runtime (`libzssh_copy`).

`zazaki_scp` is NOT a second implementation of SSH file transfer. It is a second frontend.

## 2. Product Boundary

```
zazaki_ssh (comprehensive SSH tool)
├── zssh CLI           → connect/exec/config/keygen/proxy/copy
├── libzssh_copy       → reusable copy runtime/service
│   ├── SftpTransfer   (SFTP upload/download engine)
│   ├── TransferPlanner (resume, concurrency, chunk scheduling)
│   ├── RateLimiter     (token bucket)
│   └── ProgressEmitter (callback-based progress interface)
└── zssh_core          → config, auth, session, protocol, platform

zazaki_scp (thin copy-only facade)
├── scp_cli            → parse scp-style argv → CopyRequest
├── ProgressRenderer   → FTXUI-based transfer panel
└── calls libzssh_copy → zero custom SSH/transfer logic
```

### What zazaki_scp owns:
- scp-style argument parsing (user@host:path syntax)
- FTXUI progress rendering (progress bars, transfer rate, ETA, file queue)
- scp-style defaults (recursive `-r`, preserve `-p`, etc.) mapped to `CopyRequest` fields

### What zazaki_scp does NOT own:
- SSH connection establishment or teardown
- Authentication (password, key, agent)
- SFTP protocol operations
- Configuration loading (uses zazaki_ssh's YAML config)
- Session lifecycle or backpressure
- Platform abstraction (epoll/IOCP)
- Error classification or JSON output schema

## 3. Goals

- Provide a familiar `scp`-style CLI with enhanced features (progress, resume, concurrency, rate limit, file filter)
- Reuse `zazaki_ssh`'s copy runtime, config, auth, and session layers without duplication
- Deliver a clean FTXUI-based transfer progress UI
- Support Linux, macOS, and Windows via zazaki_ssh's platform abstraction

## 4. Non-goals

- Do not implement any SSH protocol logic
- Do not re-implement configuration loading or authentication
- Do not duplicate SFTP operations or transfer orchestration
- Do not provide interactive shell, remote exec, proxying, or key generation

## 5. Enhanced Features (vs standard scp)

| Feature | Implementation |
|---------|---------------|
| Progress bar + transfer rate | FTXUI real-time rendering, driven by `ProgressEmitter` callbacks from `libzssh_copy` |
| Resume (断点续传) | `TransferPlanner` in `libzssh_copy` compares local/remote file sizes via SFTP stat, resumes from offset |
| Concurrent multi-file transfer | `TransferPlanner` manages multiple SFTP channels with thread pool scheduling |
| Rate limiting | `RateLimiter` (token bucket) in `libzssh_copy`, configured via CLI flags or YAML |
| File filter rules | Glob include/exclude patterns parsed by `scp_cli`, passed as filter to `libzssh_copy` |

## 6. CLI Design

### 6.1 Command Syntax

```
zazaki_scp [options] source... destination
```

### 6.2 Remote Path Syntax

```
[profile://][user@]host:path
```

- `profile://` — optional, selects a named profile from zazaki_ssh's YAML config
- `user@` — optional, overrides profile username
- `host` — hostname or IP (uses profile if `profile://` is set, otherwise bare host)
- `:path` — remote filesystem path

Examples:
```
zazaki_scp ./local.txt prod://:/tmp/remote.txt
zazaki_scp prod://:/var/log/*.log ./logs/
zazaki_scp -r dev://user@10.0.0.5:/app ./backup/
```

### 6.3 Options

| Flag | Description | Maps to |
|------|-------------|---------|
| `-r` | Recursive directory copy | `CopyRequest::recursive` |
| `-p` | Preserve modification times and permissions | `CopyRequest::preserve_attrs` |
| `-P <port>` | Port number | `CopyRequest::port` |
| `-i <key>` | Identity file path | `CopyRequest::key_path` |
| `--limit <rate>` | Rate limit (e.g. `10M`, `500K`) | `RateLimiter` config |
| `-j <n>` | Concurrent transfers | `TransferPlanner` concurrency |
| `--resume` | Enable/force resume mode | `CopyRequest::resume` |
| `--include <pat>` | Include glob pattern | File filter |
| `--exclude <pat>` | Exclude glob pattern | File filter |
| `--profile <name>` | Use named profile (alternative to `profile://` syntax) | Config profile selection |
| `--json` | Machine-readable JSON output | Output mode |
| `--no-progress` | Disable FTXUI progress UI, use plain text | Output mode |

## 7. Architecture

### 7.1 Component Diagram

```
┌──────────────────────────────────────────┐
│              zazaki_scp                   │
│                                           │
│  ┌─────────────┐   ┌──────────────────┐  │
│  │  scp_cli     │   │ ProgressRenderer │  │
│  │  (arg parse) │   │    (FTXUI)       │  │
│  └──────┬───────┘   └────────┬─────────┘  │
│         │                    │            │
│         │   CopyRequest      │ progress   │
│         │                    │ callbacks  │
│         ▼                    │            │
│  ┌───────────────────────────┴──────────┐ │
│  │        libzssh_copy                   │ │
│  │  ┌──────────┐ ┌────────┐ ┌────────┐  │ │
│  │  │Transfer  │ │Rate    │ │Progress │  │ │
│  │  │Planner   │ │Limiter │ │Emitter  │  │ │
│  │  └────┬─────┘ └────────┘ └────────┘  │ │
│  │       │                               │ │
│  │  ┌────┴───────────────────────────┐   │ │
│  │  │     SftpTransfer               │   │ │
│  │  │  (libssh SFTP operations)      │   │ │
│  │  └────────────────────────────────┘   │ │
│  └───────────────────────────────────────┘ │
└──────────────────────────────────────────┘
         │
         │ links against
         ▼
┌──────────────────────────────────────────┐
│           zazaki_ssh (libraries)          │
│                                           │
│  zssh_core    zssh_config    zssh_session │
│  zssh_protocol  zssh_platform             │
│  libzssh_copy                             │
└──────────────────────────────────────────┘
```

### 7.2 Data Flow

1. `scp_cli` parses command line → produces `CopyRequest`
2. `scp_cli` loads zazaki_ssh YAML config → resolves profile/auth
3. `scp_cli` instantiates `ProgressRenderer` (FTXUI or plain text, based on `--no-progress`)
4. `scp_cli` calls `CopyService::run(request, progress_emitter)`
5. `CopyService` uses `SftpTransfer` for actual SFTP I/O
6. `SftpTransfer` emits progress events → `ProgressRenderer` updates display
7. On completion, `CopyService` returns result → `scp_cli` renders summary (or `--json` output)

### 7.3 Key Interfaces

`CopyRequest` (zazaki_ssh side, `src/zssh/copy/CopyRequest.hpp`):
```cpp
struct CopyRequest {
    std::string profile_name;
    std::string source_path;
    std::string destination_path;
    CopyDirection direction;  // LocalToRemote | RemoteToLocal
    bool recursive{false};
    bool preserve_attrs{false};
    bool resume{false};
    int port{0};
    std::string key_path;
    uint32_t concurrency{1};
    uint64_t rate_limit_bytes_per_sec{0};
    std::vector<std::string> include_patterns;
    std::vector<std::string> exclude_patterns;
};
```

`ProgressEmitter` (callback interface, zazaki_ssh side):
```cpp
struct TransferProgress {
    std::string file_path;
    uint64_t bytes_transferred;
    uint64_t total_bytes;
    uint64_t bytes_per_second;
    TransferState state;  // Queued | Transferring | Completed | Failed
};

class IProgressSink {
public:
    virtual ~IProgressSink() = default;
    virtual void on_progress(const TransferProgress& progress) = 0;
    virtual void on_file_complete(const std::string& path, bool success) = 0;
    virtual void on_transfer_complete(const TransferSummary& summary) = 0;
};
```

## 8. Build and Dependencies

### 8.1 Build System

- CMake (same as zazaki_ssh)
- C++20

### 8.2 Dependencies

| Dependency | Role | Source |
|-----------|------|--------|
| zazaki_ssh (libraries) | All SSH, config, session, copy logic | CMake `add_subdirectory` or `find_package` |
| FTXUI | Progress UI rendering | vcpkg / FetchContent |
| CLI11 | Argument parsing | header-only, FetchContent |

### 8.3 CMake Structure

```cmake
# zazaki_scp/CMakeLists.txt (conceptual)
cmake_minimum_required(VERSION 3.24)
project(zazaki_scp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

# Depend on zazaki_ssh
add_subdirectory(../zazaki_ssh zazaki_ssh_build)
# or: find_package(zazaki_ssh REQUIRED)

# FTXUI + CLI11 via FetchContent or vcpkg

add_executable(zazaki_scp
    src/main.cpp
    src/scp_cli.cpp
    src/progress_renderer.cpp
)

target_link_libraries(zazaki_scp PRIVATE
    zssh_core
    zssh_copy
    ftxui::screen
    ftxui::dom
    ftxui::component
    CLI11::CLI11
)
```

## 9. Configuration Integration

`zazaki_scp` reads the same `zssh.yaml` configuration file as `zazaki_ssh`. No separate config file.

Profile example with copy-specific defaults:
```yaml
defaults:
  copy:
    concurrency: 4
    rate_limit: "50M"
    resume: true

profiles:
  prod:
    host: prod.example.com
    port: 22
    auth:
      method: publickey
      username: deploy
      key_path: ~/.ssh/id_ed25519
      use_agent: true
```

CLI flags override config values. Profile resolution follows zazaki_ssh's established merge rules.

## 10. zazaki_ssh Side Changes Required

To support `zazaki_scp` as a thin facade, `zazaki_ssh` must:

1. **Add `libzssh_copy` as a linkable library target** (not just internal to `zssh` binary)
   - `CMakeLists.txt`: add `zssh_copy` library target
   - Publish `CopyRequest.hpp`, `CopyService.hpp`, `IProgressSink.hpp` as public headers

2. **Split CopyService from CLI-specific code**
   - `CopyService` must NOT depend on `zssh::cli` or `zssh::renderer`
   - Progress is delivered via callback interface, not direct terminal rendering
   - `zssh copy` CLI calls same `CopyService` with its own renderer; `zazaki_scp` calls it with FTXUI renderer

3. **Expand CopyRequest model**
   - Add fields for concurrency, rate limiting, resume, file filters, preserve attributes

4. **Expand ISshAdapter / LibsshAdapter**
   - Add SFTP operations: `sftp_open`, `sftp_read`, `sftp_write`, `sftp_stat`, `sftp_mkdir`, `sftp_opendir`

5. **Update Task 7 (implementation plan)**
   - Split into: Task 7a (libzssh_copy library + SFTP adapter expansion) and Task 7b (zssh copy CLI + zazaki_scp facade)

## 11. Verification

### Unit Tests (zazaki_scp)
- `scp_cli` argument parsing: scp-style syntax, profile resolution, flag mapping
- `ProgressRenderer` behavior: progress events map to correct UI state

### Unit Tests (zazaki_ssh, new)
- `SftpTransfer`: stat, open, read, write, mkdir operations against FakeSshAdapter
- `TransferPlanner`: resume offset calculation, concurrency scheduling
- `RateLimiter`: token bucket correctness under burst and steady-state
- `CopyService`: orchestration with mock progress sink

### Integration Tests (zazaki_ssh)
- Copy workflow against real SSH server (existing `CopyProxyWorkflowTest`)
- Resume after partial transfer
- Concurrent transfer correctness
- Rate limit enforcement

## 12. Out of Scope (for initial release)

- `zmodem`-style or custom transfer protocols (SFTP only)
- Compression at transfer layer (defer to SSH compression)
- Delta/sync transfer (use `rsync` for that)
- Interactive file browser/selector UI
- Scheduled/background transfers

## 13. Acceptance Criteria

1. `zazaki_scp prod://:/tmp/foo ./foo` completes a file download with FTXUI progress display
2. `zazaki_scp -r ./dir prod://:/backup/` completes recursive upload
3. `zazaki_scp --resume` correctly resumes an interrupted transfer
4. `zazaki_scp -j 4` transfers 4 files concurrently
5. `zazaki_scp --limit 1M` does not exceed 1 MB/s
6. `zazaki_scp --json` produces valid JSON output matching zazaki_ssh's schema conventions
7. All existing `zssh` tests continue to pass
8. `libzssh_copy` is linkable by `zazaki_scp` without pulling in `zssh` CLI or renderer dependencies
