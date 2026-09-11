# Plan 004: Terminal TUI Signal Safety & Child Process Cleanup
> **Executor instructions**: Follow this plan step by step. Run every
> verification command and confirm the expected result before moving to the
> next step. If anything in the "STOP conditions" section occurs, stop and
> report — do not improvise.
>
> **Drift check (run first)**: `git diff --stat b68b881..HEAD -- src/cli/TerminalChess.cpp src/cli/TerminalChess.h`

## Status
- **Priority**: P2
- **Effort**: S
- **Risk**: LOW
- **Depends on**: none
- **Category**: security, stability
- **Planned at**: commit `b68b881`, 2026-09-11
- **Issue**: 

## Why this matters
In TUI mode (`qt6chess --cli`), `signalHandler()` calls `disableRawMode()` which uses `std::cout << ...` and `std::flush`. In POSIX C++, calling `std::cout` inside a signal handler is undefined behavior and can deadlock if interrupted during an I/O operation. Additionally, `SIGHUP` (closing terminal window) is unhandled, leaving the terminal corrupted, and the Stockfish child process is orphaned on `_exit(0)`.

## Current state
- `src/cli/TerminalChess.cpp:31-46`:
  ```cpp
  static void disableRawMode() {
      if (s_rawModeActive) {
          std::cout << "\033[?1006l\033[?1000l";
          ...
  ```
- `src/cli/TerminalChess.cpp:354-360`: Only `SIGINT` and `SIGTERM` are registered.

## Commands you will need
| Purpose | Command | Expected on success |
|---------|---------|---------------------|
| Build | `ninja -C /root/Qt6Chess/build` | exit 0 |
| Test TUI | `printf "q" | ./build/bin/Qt6Chess --cli` | exit 0 |

## Scope
**In scope**:
- `src/cli/TerminalChess.cpp`
- `src/cli/TerminalChess.h`

## Steps
### Step 1: Make `disableRawMode()` Async-Signal-Safe
In `src/cli/TerminalChess.cpp`:
Replace `std::cout` escape writes with direct `::write()`:
```cpp
static void disableRawMode()
{
    if (s_rawModeActive) {
        const char resetSeq[] = "\033[?1006l\033[?1000l\033[?25h\033[?1049l\033[0m";
        (void)::write(STDOUT_FILENO, resetSeq, sizeof(resetSeq) - 1);
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &s_origTermios);
        s_rawModeActive = false;
    }
}
```
**Verify**: `ninja -C /root/Qt6Chess/build` → builds cleanly.

### Step 2: Register `SIGHUP` and `SIGQUIT`
In `src/cli/TerminalChess.cpp:run()`:
```cpp
sigaction(SIGINT, &sa, nullptr);
sigaction(SIGTERM, &sa, nullptr);
sigaction(SIGHUP, &sa, nullptr);
sigaction(SIGQUIT, &sa, nullptr);
```
**Verify**: `ninja -C /root/Qt6Chess/build` → builds cleanly.

### Step 3: Clean Child Engine Process Termination
In `src/cli/TerminalChess.cpp`:
Store child process PID in `static pid_t s_childEnginePid = 0;`.
When starting Stockfish: `s_childEnginePid = static_cast<pid_t>(stockfish.processId());`.
In `signalHandler(int signum)`:
```cpp
static void signalHandler(int signum)
{
    (void)signum;
    if (s_childEnginePid > 0) {
        ::kill(s_childEnginePid, SIGTERM);
    }
    disableRawMode();
    _exit(0);
}
```
**Verify**: `ninja -C /root/Qt6Chess/build` → builds cleanly.

## Done criteria
- [x] `ninja -C /root/Qt6Chess/build` exits 0.
- [x] `printf "q" | ./build/bin/Qt6Chess --cli` exits 0.
- [x] `disableRawMode` uses `::write` instead of `std::cout`.
