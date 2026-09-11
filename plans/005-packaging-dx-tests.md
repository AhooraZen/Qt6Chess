# Plan 005: Packaging, DX & Test Suite Expansion
> **Executor instructions**: Follow this plan step by step. Run every
> verification command and confirm the expected result before moving to the
> next step. If anything in the "STOP conditions" section occurs, stop and
> report — do not improvise.
>
> **Drift check (run first)**: `git diff --stat b68b881..HEAD -- PKGBUILD LICENSE README.md tests/TestChessCore.cpp`

## Status
- **Priority**: P2
- **Effort**: S
- **Risk**: LOW
- **Depends on**: plans/001-engine-eval-sync.md, plans/003-model-view-history.md
- **Category**: dx, packaging, tests
- **Planned at**: commit `b68b881`, 2026-09-11
- **Issue**: 

## Why this matters
The `PKGBUILD` currently includes `ccache` in `makedepends` (violating Arch guidelines), lacks a `LICENSE` file installation for MIT compliance, and `README.md` documents an outdated binary path. Automated tests only test `chess.hpp` rather than testing `ChessBoardModel`, `MoveHistoryModel`, and `ChessClock`.

## Current state
- `PKGBUILD:22`: `ccache` in `makedepends`.
- No `LICENSE` file exists in the repository.
- `README.md:16`: Specifies `./build/Qt6Chess` instead of `./build/bin/Qt6Chess`.
- `tests/TestChessCore.cpp`: Missing contract checks for custom model roles and history truncation.

## Commands you will need
| Purpose | Command | Expected on success |
|---------|---------|---------------------|
| Build | `ninja -C /root/Qt6Chess/build` | exit 0 |
| Test | `ctest --test-dir /root/Qt6Chess/build --output-on-failure` | 100% tests passed |

## Scope
**In scope**:
- `LICENSE` (create)
- `PKGBUILD`
- `README.md`
- `tests/TestChessCore.cpp`

## Steps
### Step 1: Create MIT License File
Create `/root/Qt6Chess/LICENSE` with standard MIT license text (Copyright 2026 AhooraZen).
**Verify**: `ls -l /root/Qt6Chess/LICENSE` exists.

### Step 2: Fix PKGBUILD Packaging Guidelines
In `/root/Qt6Chess/PKGBUILD`:
1. Remove `'ccache'` from `makedepends`.
2. In `package()`, add:
```bash
install -Dm644 "$srcdir/Qt6Chess-$pkgver/LICENSE" "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
```
**Verify**: `grep "ccache" /root/Qt6Chess/PKGBUILD` returns 0 matches.

### Step 3: Fix README.md Execution Path
In `README.md`:
Update lines referencing `./build/Qt6Chess` to `./build/bin/Qt6Chess`.
**Verify**: `grep "build/Qt6Chess" README.md` returns 0 matches.

### Step 4: Expand TestChessCore Unit Tests
In `tests/TestChessCore.cpp`:
Add:
- `testBoardModelRoles()`: Check `pieceCode`, `isSelected`, `file`, `rank` roles via `data()`.
- `testMoveHistoryTruncation()`: Add 4 moves, call `truncateAfterPly(2)`, verify count and FEN history.
- `testClockTimeoutSignal()`: Use `QSignalSpy` on `timeOut`.
**Verify**: `ctest --test-dir /root/Qt6Chess/build --output-on-failure` → all tests pass.

## Done criteria
- [x] `LICENSE` file exists at repository root.
- [x] `ninja -C /root/Qt6Chess/build` exits 0.
- [x] `ctest --test-dir /root/Qt6Chess/build --output-on-failure` passes 100%.
- [x] `PKGBUILD` no longer lists `ccache` in `makedepends`.
