# ArcNova — Linux-styled Command Shell for Windows

<p align="center">
  <strong>Run <code>ls</code>, <code>grep</code>, <code>rm -rf</code>, and your everyday tools natively on Windows, without WSL.</strong>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/C%2B%2B-17-00599C?style=flat&logo=cplusplus&logoColor=white" alt="C++ 17">
  <img src="https://img.shields.io/badge/Windows-10%20%7C%2011%20x64-0078D4?style=flat" alt="Windows 10 | 11 x64">
  <img src="https://img.shields.io/badge/MinGW--w64-g%2B%2B-A42E2B?style=flat&logo=gnu&logoColor=white" alt="MinGW-w64 g++">
  <img src="https://img.shields.io/badge/Shell-Linux--styled-4EAA25?style=flat&logo=gnubash&logoColor=white" alt="Linux-styled shell">
  <img src="https://img.shields.io/badge/Dependencies-0-2EA44F?style=flat" alt="Zero dependencies">
</p>

```text
        ▲
       ╱ ╲
      ╱ ◯ ╲       A R C N O V A   S H E L L
     ╱─────╲      v1.0.0 • Windows_NT x64
      ─────
```

---

## Overview

ArcNova is a custom command-line shell for Windows that understands the commands Linux users type by reflex: `ls -la`, `pwd`, `cat`, `touch`, `rm -rf`, `cp -r`, `grep -in`, and more. These commands are implemented directly in C++ on top of the Win32 API and `std::filesystem`, so they run natively on NTFS without WSL, Cygwin, MSYS2, or any emulation layer.

Anything ArcNova does not implement itself is passed to Windows, so tools such as `git`, `python`, `npm`, and `ping` keep working inside the same window.

The result is a single standalone `ArcNova.exe` with a retro-modern terminal interface: a welcome card, a dynamic prompt, colour-coded directory listings, and a signature `arcfetch` system summary.

### Why ArcNova

| Problem with existing Windows terminals | How ArcNova addresses it |
| :--- | :--- |
| `cmd.exe` rejects common commands such as `ls -la`, `touch`, and `cat`. | Built-in UNIX-style commands are executed by native C++ code. |
| PowerShell aliases (`ls` → `Get-ChildItem`) reject POSIX flags such as `-la` and `-rf`. | A dedicated parser understands combined POSIX-style flags. |
| WSL is heavy for quick file tasks. | A small native binary that starts instantly and uses little memory. |
| The default console is visually plain. | Unicode welcome card, path-aware prompt, and coloured output. |

---

## Features

- **Native built-in commands** for navigation, file management, text search, and system information.
- **POSIX-style argument parsing** with single and double quotes, combined flags (`-la`, `-rf`), the `--` end-of-flags marker, and `~` home expansion.
- **Dynamic prompt** that abbreviates your home directory to `~`, always uses forward slashes, and changes colour based on the previous command's result.
- **External program execution** through `CreateProcessW`, with live output, inherited console input, and correct exit codes.
- **Safety-first file operations**: protected system paths cannot be deleted, junctions are not followed during recursive deletion, and copy/move never silently overwrite existing files.
- **Crash-resistant design**: filesystem operations use `std::error_code`, and invalid input produces a clear error message instead of terminating the shell.
- **Desktop integration**: embedded multi-resolution icon and a Start Menu installer, so ArcNova can be launched from Windows Search.

---

## Interface

### The prompt

```text
arcnova ~/Documents/Projects ❯
```

| Element | Meaning |
| :--- | :--- |
| `~/Documents/Projects` | Current directory. Paths inside `%USERPROFILE%` are shortened to `~`, and `\` is shown as `/`. |
| Green `❯` | The previous command succeeded (exit code `0`). |
| Red `❯` | The previous command failed, was not found, or was interrupted. |

Pressing Enter on an empty line simply shows a new prompt. Pressing `Ctrl+C` cancels the current line or running program without closing ArcNova.

### Directory listing colours

| Entry type | Appearance |
| :--- | :--- |
| Directory | Bright blue, followed by `/` |
| Executable (`.exe`, `.bat`) | Bright green |
| Hidden (starts with `.` or has the Windows Hidden attribute) | Dim grey, shown only with `-a` |
| Regular file | Default white |

---

## Command Reference

Type `help` inside ArcNova to see this list at any time.

### Navigation

| Command | Description |
| :--- | :--- |
| `pwd [-W]` | Print the current directory using `/`. With `-W`, print the native Windows form using `\`. |
| `cd [path]` | Change directory. Without arguments, go to `%USERPROFILE%`. Supports `..`, `~`, `~/path`, and quoted paths. |
| `cd -` | Return to the previous directory and print its path. |
| `ls [-a] [-l] [-h] [path]` | List directory contents, sorted by name. |

`ls` flags can be combined, for example `ls -lah`:

| Flag | Effect |
| :--- | :--- |
| `-a` | Include hidden entries. |
| `-l` | Long format: simulated permissions, size, and last-modified time. |
| `-h` | Human-readable sizes (`B`, `KiB`, `MiB`, `GiB`, and so on). |

```text
arcnova ~/Projects ❯ ls -lh
drwxr-xr-x            - 2026-10-14 09:12 src/
-rwxr-xr-x       2.1MiB 2026-10-14 09:30 ArcNova.exe
-rw-r--r--         512B 2026-10-14 08:55 notes.txt
```

> Permissions in long format are a Linux-style visual simulation. They do not represent Windows ACLs.

### File and folder management

| Command | Description |
| :--- | :--- |
| `touch <file>...` | Create one or more empty files, or update the modification time of existing files without changing their contents. |
| `mkdir [-p] <dir>...` | Create directories. With `-p`, create all missing parent directories and ignore directories that already exist. |
| `rm [-r] [-f] <path>...` | Remove files. Directories require `-r`. With `-f`, missing targets are ignored. |
| `cp [-r] <source> <destination>` | Copy a file, or a directory when `-r` is given. |
| `mv <source> <destination>` | Move or rename a file or directory. |
| `cat [-n] <file>...` | Print text files. With `-n`, number every line. |

If the destination of `cp` or `mv` is an existing directory, the source is placed inside it using its original name.

```text
arcnova ~ ❯ mkdir -p "Projects/New Folder/assets"
arcnova ~ ❯ touch notes.txt todo.txt
arcnova ~ ❯ cp -r Projects Projects-backup
arcnova ~ ❯ mv todo.txt Projects/
arcnova ~ ❯ cat -n notes.txt
arcnova ~ ❯ rm -rf Projects-backup
```

### Text and system utilities

| Command | Description |
| :--- | :--- |
| `grep [-i] [-n] <pattern> <file>` | Print lines containing `pattern`, with each match highlighted. `-i` ignores letter case and `-n` shows line numbers. |
| `echo <text>...` | Print the arguments separated by spaces. Use `echo -- -text` to print text that begins with `-`. |
| `whoami` | Print the current Windows user name. |
| `arcfetch` | Show the ArcNova logo beside live system information. |

`grep` searches for literal text (not regular expressions). Its exit code is `0` when a match is found, `1` when there is no match, and `2` on an error.

```text
arcnova ~/logs ❯ grep -in "error" application.log
12:Connection error: timeout
48:ERROR while saving settings
```

### arcfetch

`arcfetch` reads all values live each time it runs:

| Field | Source |
| :--- | :--- |
| OS | Windows edition and build number from the registry |
| Hostname | `GetComputerNameExW` |
| User | `GetUserNameW` |
| CPU | Processor name from the Windows registry |
| RAM | Used / total memory and usage percentage from `GlobalMemoryStatusEx` |
| Uptime | Hours and minutes from `GetTickCount64` |

### Shell control

| Command | Description |
| :--- | :--- |
| `clear`, `cls` | Clear the screen and redraw the welcome banner. |
| `help` | Show the built-in command table. |
| `exit` | Close ArcNova with exit code `0`. |

---

## External Programs

Any command that is not built into ArcNova is passed to Windows:

```text
arcnova ~/Projects/app ❯ git status
arcnova ~/Projects/app ❯ python main.py
arcnova ~/Projects/app ❯ npm install
arcnova ~ ❯ ping google.com
```

How external commands are run:

1. ArcNova first starts the program directly with `CreateProcessW`, letting Windows locate it through `%PATH%`.
2. If the program cannot be started directly (for example a `.bat`/`.cmd` script or a `cmd.exe` built-in such as `ver`), the original command line is passed to `cmd.exe /d /s /c`.
3. Command lines containing pipes or redirection (`|`, `>`, `<`, `&`) are always sent to `cmd.exe`.
4. ArcNova waits for the program to finish. Its exit code decides the prompt colour.

Built-in commands always take priority. To use the Windows version of a command that ArcNova also provides, call it explicitly, for example `cmd /c echo hello`.

---

## Argument Parsing

| Input | Parsed result |
| :--- | :--- |
| `mkdir "New Folder"` | One argument: `New Folder` |
| `echo 'single quoted text'` | One argument: `single quoted text` |
| `ls -l -a` or `ls -la` | Flags: `l` and `a` |
| `cd ~/Documents` | `C:/Users/<you>/Documents` |
| `rm -- -strange-name.txt` | Argument `-strange-name.txt` (everything after `--` is treated as an argument) |
| `cat C:\Users\me\file.txt` | Backslashes are kept, so normal Windows paths work |

An unclosed quote, such as `echo "hello`, is reported as a parse error and the command is not run.

---

## Safety

ArcNova is designed so that user mistakes cannot crash the shell or damage the operating system.

**Deletion safeguards in `rm`:**

- Refuses to delete a drive root such as `C:\` or `C:/`.
- Refuses to delete critical Windows locations, including `Windows`, `Program Files`, `Program Files (x86)`, `ProgramData`, the `Users` folder itself, `$Recycle.Bin`, and `System Volume Information`, as well as any folder that contains them.
- Resolves paths before checking them, so tricks such as `C:\Temp\..\Windows` are still blocked.
- Refuses recursive deletion when the target or anything inside it is a junction, symbolic link, or other reparse point, so deletion cannot escape the intended folder.
- If a safety check cannot be completed, the deletion is refused.

**Overwrite protection:**

- `cp` and `mv` refuse to overwrite an existing destination.
- Copying or moving a directory into itself is rejected.

**Robustness:**

- Every filesystem operation reports errors through `std::error_code` instead of throwing.
- Invalid flags, missing arguments, and missing files produce a clear message such as `ArcNova: cd: missing: The system cannot find the file specified.`

> These safeguards reduce risk but are not a substitute for backups. Deleted files do not go to the Recycle Bin.

---

## Requirements

| Requirement | Details |
| :--- | :--- |
| Operating system | Windows 10 or Windows 11, 64-bit |
| Compiler | MinGW-w64 `g++` with C++17 support, plus `windres` (included with MinGW-w64) |
| Terminal | Windows Console Host or Windows Terminal (ANSI colour and UTF-8 are enabled automatically) |

A convenient way to get the toolchain is [MSYS2](https://www.msys2.org/) with the UCRT64 environment:

```bash
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-make
```

Then add `C:\msys64\ucrt64\bin` to your `PATH`.

---

## Building

### Option 1: one-click build (recommended)

```bat
build.bat
```

This script:

1. Checks that `g++` and `windres` are available.
2. Compiles `assets/resource.rc` so the icon is embedded in the executable.
3. Compiles all sources into a standalone, statically linked `ArcNova.exe`.

### Option 2: Makefile

```bat
mingw32-make
mingw32-make clean
```

The Makefile builds the shell without the embedded icon, which is useful for quick development builds.

### Run

```bat
ArcNova.exe
```

---

## Installation and Start Menu Integration

After building, run:

```bat
install.bat
```

The installer:

1. Copies `ArcNova.exe` to `%LOCALAPPDATA%\Programs\ArcNova\`.
2. Creates an `ArcNova` shortcut with the triangle-and-circle icon in your Start Menu (`%APPDATA%\Microsoft\Windows\Start Menu\Programs\`).

Administrator rights are not required. Afterwards, press the **Windows** key, type **ArcNova**, and press **Enter**. If the entry does not appear immediately, wait a moment for Windows Search to index it.

To update ArcNova, close any open ArcNova windows, then run `build.bat` and `install.bat` again.

### Uninstalling

Delete these two items:

```text
%LOCALAPPDATA%\Programs\ArcNova\
%APPDATA%\Microsoft\Windows\Start Menu\Programs\ArcNova.lnk
```

---

## Project Structure

```text
arcnova/
├── Makefile                    Development build for MinGW-w64
├── build.bat                   One-click release build with embedded icon
├── install.bat                 Per-user install and Start Menu shortcut
├── assets/
│   ├── arcnova.ico             Multi-resolution application icon (16-256 px)
│   ├── generate_icon.ps1       Regenerates arcnova.ico
│   └── resource.rc             Embeds the icon into ArcNova.exe
├── include/
│   ├── ui/
│   │   ├── colors.hpp          ANSI colour constants
│   │   └── banner.hpp          Welcome card renderer
│   ├── core/
│   │   ├── terminal.hpp        Console setup and Ctrl+C handling
│   │   ├── parser.hpp          Tokenizer, quotes, flags, tilde expansion
│   │   ├── executor.hpp        External program execution
│   │   └── shell.hpp           REPL state, prompt, command registry
│   └── commands/
│       ├── fs_commands.hpp     pwd, cd, ls, touch, mkdir, rm, cp, mv, cat
│       └── sys_commands.hpp    grep, echo, whoami, arcfetch
├── src/
│   ├── main.cpp                Entry point and read-eval-print loop
│   ├── core/                   Implementations of the core modules
│   └── commands/               Implementations of the built-in commands
└── tests/
    ├── parser_tests.cpp        Parser unit tests
    ├── executor_tests.cpp      External execution tests
    ├── sys_commands_tests.cpp  grep, echo, whoami, arcfetch tests
    ├── fs_smoke.ps1            pwd, cd, ls end-to-end tests
    └── mutation_smoke.ps1      touch, mkdir, rm, cp, mv, cat end-to-end tests
```

### How a command flows through ArcNova

```text
Input line
   │
   ▼
Parser ── splits words, handles quotes, collects flags, expands ~
   │
   ▼
Shell command registry
   ├── Built-in found ──► native C++ handler (fs_commands / sys_commands)
   └── Not found ───────► Executor ──► CreateProcessW, or cmd.exe fallback
   │
   ▼
Exit code ──► prompt colour (green / red)
```

---

## Running the Tests

Run these commands from the project root.

**Parser tests**

```bat
g++ -std=c++17 -Wall -Wextra -Iinclude tests/parser_tests.cpp src/core/parser.cpp -o parser_tests.exe
parser_tests.exe
```

**External executor tests**

```bat
g++ -std=c++17 -Wall -Wextra -Iinclude tests/executor_tests.cpp src/core/executor.cpp -o executor_tests.exe
executor_tests.exe
```

**System command tests** (the argument is a temporary file the test creates and deletes)

```bat
g++ -std=c++17 -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=UTF-8 -Iinclude tests/sys_commands_tests.cpp src/commands/sys_commands.cpp -o sys_tests.exe -ladvapi32
sys_tests.exe "%TEMP%\arcnova-sys-test.txt"
```

**Filesystem end-to-end tests** (build `ArcNova.exe` first; tests run in a temporary folder)

```powershell
powershell -ExecutionPolicy Bypass -File tests/fs_smoke.ps1 -Executable .\ArcNova.exe
powershell -ExecutionPolicy Bypass -File tests/mutation_smoke.ps1 -Executable .\ArcNova.exe
```

Each test prints a line beginning with `PASS` when it succeeds.

---

## Known Limitations

- Pipes and redirection are handled by `cmd.exe`, so they only apply to external programs. A line that starts with a built-in command, such as `ls | findstr src`, runs the built-in without the pipe.
- `grep` matches literal text, and `-i` ignores case for English letters (A-Z) only.
- `mv` cannot move items between different drives. Use `cp -r` followed by `rm -r` instead.
- `rm` deletes permanently and does not use the Recycle Bin.
- There is no command history, tab completion, or environment variable expansion yet.

---

## Roadmap

- Command history with arrow-key navigation
- Tab completion for commands and paths
- Native pipes and redirection between built-in commands
- Environment variable expansion (`$VAR`, `%VAR%`)
- Configurable prompt and colour themes

---

## Author

**Christian Bagus Prasetya**

Repository: [github.com/yann-gamedev/arcnova](https://github.com/yann-gamedev/arcnova)
