# Plan 001: Core Engine & Evaluation Fixes
> **Executor instructions**: Follow this plan step by step. Run every
> verification command and confirm the expected result before moving to the
> next step. If anything in the "STOP conditions" section occurs, stop and
> report — do not improvise.
>
> **Drift check (run first)**: `git diff --stat b68b881..HEAD -- src/engine/UciController.h src/engine/UciController.cpp src/game/GameController.cpp qml/components/EvalBar.qml`

## Status
- **Priority**: P1
- **Effort**: S
- **Risk**: LOW
- **Depends on**: none
- **Category**: bug, perf, security
- **Planned at**: commit `b68b881`, 2026-09-11
- **Issue**: 

## Why this matters
Currently, when Black is to move and winning (e.g. Black has mate in 1), the evaluation score reported by Stockfish is positive relative to Black, but `EvalBar.qml` assumes positive means White is winning, visually inverting the evaluation bar. Furthermore, custom FEN puzzles loaded into the game still send `position startpos` to Stockfish, causing illegal moves, while low Elo AI difficulty settings ignore `depthLimit`. Finally, missing Stockfish freezes the GUI for 3 seconds on launch.

## Current state
- `src/engine/UciController.cpp:215-221`: Centipawn scores are parsed directly without perspective normalization:
  ```cpp
  if (parts[i] == QStringLiteral("cp")) {
      m_currentEval = parts[i + 1].toDouble() / 100.0;
  }
  ```
- `src/game/GameController.cpp:397`:
  ```cpp
  m_uci->setPosition(QStringLiteral("startpos"), moves);
  ```
- `src/engine/UciController.cpp:152-154`:
  ```cpp
  void UciController::searchBestMove(int moveTimeMs, int depthLimit) {
      Q_UNUSED(depthLimit);
      sendCommand(QStringLiteral("go movetime %1").arg(moveTimeMs));
  ```
- `src/engine/UciController.cpp:52-57`:
  ```cpp
  m_process->start(enginePath);
  if (m_process->waitForStarted(3000)) {
  ```

## Commands you will need
| Purpose | Command | Expected on success |
|---------|---------|---------------------|
| Build | `ninja -C /root/Qt6Chess/build` | exit 0 |
| Test | `ctest --test-dir /root/Qt6Chess/build --output-on-failure` | 100% tests passed |

## Scope
**In scope**:
- `src/engine/UciController.h`
- `src/engine/UciController.cpp`
- `src/game/GameController.cpp`
- `qml/components/EvalBar.qml`

**Out of scope**:
- `src/core/chess.hpp`
- Any change to the public QML property names in `EvalBar` (`rawEval`, `mateIn`).

## Steps
### Step 1: Normalize UCI Evaluation to White Perspective
In `UciController.h`, add `bool m_isWhiteToMove = true;` with a setter `void setIsWhiteToMove(bool whiteToMove);`.
In `UciController.cpp:parseLine`, when parsing `cp` or `mate`, normalize:
```cpp
double score = parts[i + 1].toDouble() / 100.0;
m_currentEval = m_isWhiteToMove ? score : -score;
```
and for mate:
```cpp
int mateVal = parts[i + 1].toInt();
m_mateIn = m_isWhiteToMove ? mateVal : -mateVal;
```
In `GameController.cpp:updateEnginePosition()`, call:
```cpp
m_uci->setIsWhiteToMove(m_board.sideToMove() == chess::Color::WHITE);
```
**Verify**: `ninja -C /root/Qt6Chess/build` → builds cleanly.

### Step 2: Pass Custom Initial FEN to Stockfish
In `GameController.h`, add `QString m_startFen = QStringLiteral("startpos");`.
In `GameController.cpp:loadFen(const QString &fen)`:
Set `m_startFen = fen.trimmed();`.
In `GameController.cpp:newGame(...)`:
Set `m_startFen = QStringLiteral("startpos");`.
In `GameController.cpp:updateEnginePosition()`:
Call `m_uci->setPosition(m_startFen, moves);`.
**Verify**: `ninja -C /root/Qt6Chess/build` → builds cleanly.

### Step 3: Implement Depth Limit in `searchBestMove`
In `UciController.cpp:searchBestMove(int moveTimeMs, int depthLimit)`:
```cpp
if (depthLimit > 0) {
    sendCommand(QStringLiteral("go movetime %1 depth %2").arg(moveTimeMs).arg(depthLimit));
} else {
    sendCommand(QStringLiteral("go movetime %1").arg(moveTimeMs));
}
```
**Verify**: `ninja -C /root/Qt6Chess/build` → builds cleanly.

### Step 4: Non-blocking Engine Startup and FEN Sanitization
In `UciController.cpp:startEngine`:
Check:
```cpp
if (enginePath.isEmpty() || !QFile::exists(enginePath)) {
    return false;
}
```
Use `m_process->setProcessChannelMode(QProcess::MergedChannels);`.
In `UciController.cpp:setPosition`:
Strip any newline or carriage return from `fen`:
```cpp
QString cleanFen = fen;
cleanFen.remove(QLatin1Char('\r')).remove(QLatin1Char('\n'));
```
**Verify**: `ninja -C /root/Qt6Chess/build` → builds cleanly.

### Step 5: Rate-limit UCI Info Signal Emissions
In `UciController.h`, add `QElapsedTimer m_infoTimer;`.
In `UciController.cpp`, during `parseLine` on `info score`:
Only emit `currentEvalChanged`, `depthChanged`, `npsChanged`, `primaryArrowChanged` if `!m_infoTimer.isValid() || m_infoTimer.elapsed() >= 50` (20 Hz) or if `bestmove` arrived.
Restart `m_infoTimer.start();`.
**Verify**: `ninja -C /root/Qt6Chess/build` → builds cleanly.

## Done criteria
- [x] `ninja -C /root/Qt6Chess/build` exits 0.
- [x] `ctest --test-dir /root/Qt6Chess/build` exits 0.
- [x] No `Q_UNUSED(depthLimit)` remains in `src/engine/UciController.cpp`.
- [x] `updateEnginePosition` sends `m_startFen` instead of hardcoded `"startpos"`.
- [x] `git diff --stat` shows only in-scope files modified.
