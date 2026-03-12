# C Background Directory Synchronization Daemon

![C](https://img.shields.io/badge/c-%2300599C.svg?style=for-the-badge&logo=c&logoColor=white)
![Linux](https://img.shields.io/badge/Linux-FCC624?style=for-the-badge&logo=linux&logoColor=black)
![OpenSSL](https://img.shields.io/badge/OpenSSL-721412?style=for-the-badge&logo=openssl&logoColor=white)

This repository provides a C-based client-daemon architecture for monitoring and backing up files and directories in real-time. The main Command Line Interface (CLI) application dispatches independent background daemon processes that continuously track file modifications using MD5 hash comparisons and automatically create timestamped backups.

---

## Repository Structure
```
.
├── Makefile
├── main.c
├── sync.h
├── add.c
├── remove.c
├── list.c
├── help.c
├── utils.c
└── daemon.c
```
- **Makefile** Defines the build macros for the main `sync` target and the background `daemon` target. It handles compiling each `.c` source file into a `.o` object file to ensure efficient builds.
- **main.c & utils.c** The core CLI entry point that initializes the environment, parses user input, and manages the central `.log` of all active daemons.
- **daemon.c** Compiled as a standalone binary, this program detaches from the terminal to run in the background. It periodically scans assigned paths, calculates MD5 hashes to detect file modifications, and creates physical backups in the `~/backup` directory.
- **add.c** Spawns a new daemon process via `fork()` and `execl()` for a specified file or directory.
- **remove.c** Sends a `SIGUSR1` signal to terminate a specific running daemon and cleans up its backup artifacts.
- **list.c** Parses the decentralized logs and visually prints a hierarchical tree of tracked files and their modification history.
- **help.c** Provides usage instructions and CLI guidance.

---

## Prerequisites
> **IMPORTANT** : This tool relies on the Linux `proc` filesystem (e.g., `/proc/self/exe`) and specific POSIX system calls. Execution on non-Linux environments (like Windows natively) is **NOT** recommended.
1. **Linux OS** (Requires POSIX system calls like `fork`, `execl`, `kill`, and `scandir`).
2. **GCC** (version >= 7.0).
3. **OpenSSL Library** (required for `libcrypto` to calculate MD5 hashes).

---

## Installation & Setup

1. **Install dependencies** (OpenSSL and build tools):
   ```bash
   sudo apt update
   sudo apt install -y build-essential libssl-dev
2. **Clone or copy** this repository to your local machine.
3. **Compile the program**:
   ```bash
    make
    ```
    This will generate two executable binaries: `sync` (the CLI tool) and `daemon` (the background worker)

    **Note** : By default, the program sets the backup storage directory to `~/backup` relative to the user's home diretory.
4. **Clean build files** (optional):
    ```bash
    make clean
    ```

---

## Usage
Start the interactive CLI shell by running the main binary:
```bash
./sync
```
Inside the interactive prompt (`> `), you can manage the monitoring daemons:
- `add <PATH> [OPTIONS]` : Start a new background daemon to monitor the specified path.
  - `-d` : Monitor files inside a directory (shallow).

  - `-r` : Monitor files in a directory recursively.

  - `-t <PERIOD>` : Set the monitoring interval in seconds (default is 1 sec).

- `remove <DAEMON_PID>` : Terminate the daemon process associated with the given PID and remove its backups.

- `list [DAEMON_PID]` : Show a list of all active daemons. If a PID is provided, prints a detailed tree of backed-up files for that specific daemon.

- `help` : Show available commands.

- `exit` : Exit the CLI shell (running daemons will continue in the background).
  
---

## Directory Layout Example
When active, the system structure looks like this:
```
~/backup/
├── monitor_list.log       # Centralized list tracking all active daemon PIDs and paths
├── 1234.log               # Specific action log for daemon PID 1234
├── 1234/                  # Backup storage for daemon PID 1234
│   └── example.c_20260311153000
└── 5678/                  # Backup storage for daemon PID 5678 (Recursive)
    └── src/
        └── main.c_20260311153500
```
---

## Example Workflow

1. **Start the program**:
   ```bash
   ./sync
   >
2. **Start monioring a directory recursively (checking every 3 seconds)**:
   ```bash
   > add /home/user/project_dir -r -t 3
    monitoring started (/home/user/project_dir) : 5539
3. **Modify files in a separate terminal**:  
   In another window, user creates or edits files inside /home/user/project_dir
4. **List all active monitoring daemons**:
   ```bash
   > list
    5539 : /home/user/project_dir
5. **view the modification tree for a specific daemon**:
    ```bash
    > list 5539
    project_dir
    ┣ main.c
    ┃ ┣ [create] [2026-03-11 15:30:00]
    ┃ ┗ [modify] [2026-03-11 15:35:10]
    ┗ subdir/
    ┗ utils.c
        ┗ [create] [2026-03-11 15:30:00]
7. **Terminate the daemon**:
   ```bash
   > remove 5539
    monitoring ended (/home/user/project_dir) : 5539
---

## Troubleshooting
- **"fork error" or "execl error"** : Ensure both `sync` and `daemon` binaries are compiled and located in the same directory. The `add` command specifically looks for `./daemon` in the current working directory.

- **Permission Denied / Path restriction errors** : The utility strictly forbids monitoring paths outside of your `$HOME` directory or paths inside the `~/backup` directory to prevent recursive loops. Ensure your target path is valid.

- **"fatal error: openssl/md5.h"** :  Install the OpenSSL development package (`libssl-dev`).

---

## Customization
- Change Default Polling Interval If not specified via the `-t` option, the daemon checks for file modifications every 1 second. You can modify this default behavior in `add.c`. Find this line and modify.
    ```C
    int period = 1;.
    ```
- Adjusting Buffer Sizes When handling extremely large files, the daemon reads in chunks of 4096 bytes (`sizeof(buf)` in `daemon.c`). You can adjust this macro to optimize I/O performance depending on your typical file sizes.

---

## License & Acknowledgments

This source code is licensed under the **MIT License**. See the `LICENSE` file for details.  

**Acknowledgments:**
- **Code Implementation**: Wooyong Eom, CSE, Soongsil Univ.
- **Project Specification**: The architecture, requirements, and specifications for this project were provided as an assignment for the Linux System Programming course at Soongsil University, by Prof. Hong Jiman, OSLAB. This repository contains my original implementation of those requirements.
---
_Last Updated: March 11, 2026_
