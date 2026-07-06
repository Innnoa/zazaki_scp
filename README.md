# zazaki_scp

Enhanced `scp`-style file transfer tool with progress display, resume support, concurrency, and rate limiting. A thin CLI facade over [zazaki_ssh](https://github.com/Innnoa/zazaki_ssh)'s reusable copy runtime.

## Features

- Progress bar with transfer rate display
- Resume partial transfers (断点续传)
- Concurrent multi-file transfer (`-j`)
- Rate limiting (`--limit`)
- File filter rules (`--include` / `--exclude`)
- JSON output mode (`--json`)
- Profile-based SSH configuration (shared with zazaki_ssh)

## Build

```bash
cmake -S . -B build -DZSCP_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## Usage

```bash
# Upload
zazaki_scp ./local.txt prod://:/tmp/remote.txt

# Download
zazaki_scp prod://:/var/log/app.log ./logs/

# With flags
zazaki_scp -r -p --resume -j 4 --limit 10M ./dir prod://:/backup/
```

## Configuration

Uses the same `zssh.yaml` as zazaki_ssh. Place it in the current directory or `~/.config/zssh/zssh.yaml`:

```yaml
profiles:
  prod:
    host: example.com
    port: 22
    auth:
      method: publickey
      username: deploy
      key_path: ~/.ssh/id_ed25519
      use_agent: true
```

## Architecture

```
zazaki_scp (thin CLI)
  → scp_cli (parse args → CopyRequest)
  → zazaki_ssh::CopyService (orchestration)
  → zazaki_ssh::LibsshAdapter (libssh SFTP)
```

All SSH transport, authentication, and SFTP logic lives in zazaki_ssh's `libzssh_copy` runtime.
