# Plan 002: Game State, Clock & Dialog Safety
> **Executor instructions**: Follow this plan step by step. Run every
> verification command and confirm the expected result before moving to the
> next step. If anything in the "STOP conditions" section occurs, stop and
> report — do not improvise.
>
> **Drift check (run first)**: `git diff --stat b68b881..HEAD -- src/game/GameController.h src/game/GameController.cpp src/main.cpp qml/Main.qml qml/dialogs/PromotionDialog.qml qml/dialogs/GameOverDialog.qml`

## Status
- **Priority**: P1
- **Effort**: S
- **Risk**: LOW
- **Depends on**: none
- **Category**: bug, dx
- **Planned at**: commit `b68b881`, 2026-09-11
- **Issue**: 

## Why this matters
Pressing Escape while the promotion dialog is shown leaves `m_isPromotionPending` permanently true, freezing all board input. The "Analyze Position" button in the GameOverDialog currently wipes the finished board back to move 1. Clocks continue ticking in Analysis mode and after resignations. Duplicate `appController` context properties create confusion.

## Current state
- `qml/dialogs/PromotionDialog.qml:13`:
  ```qml
  closePolicy: Popup.CloseOnEscape
  ```
  Closing with Escape doesn't inform `GameController::cancelPromotion()`.
- `qml/dialogs/GameOverDialog.qml:55-58`:
  ```qml
  onClicked: {
      root.gameController.gameMode = 2
      root.close()
  }
  ```
  `setGameMode` calls `newGame()` which resets `m_board` to `startpos`.
- `src/game/GameController.cpp:98-103`: Clock starts unconditionally even in `ModeAnalysis`.
- `src/main.cpp:55-56`:
  ```cpp
  engine.rootContext()->setContextProperty("gameController", &gameController);
  engine.rootContext()->setContextProperty("appController", &gameController);
  ```

## Commands you will need
| Purpose | Command | Expected on success |
|---------|---------|---------------------|
| Build | `ninja -C /root/Qt6Chess/build` | exit 0 |
| Test | `ctest --test-dir /root/Qt6Chess/build --output-on-failure` | 100% tests passed |

## Scope
**In scope**:
- `src/game/GameController.h`
- `src/game/GameController.cpp`
- `src/main.cpp`
- `qml/Main.qml`
- `qml/dialogs/PromotionDialog.qml`
- `qml/dialogs/GameOverDialog.qml`

## Steps
### Step 1: Promotion Dialog Escape Cancellation
In `qml/dialogs/PromotionDialog.qml`:
Add an `onClosed` handler:
```qml
onClosed: {
    if (root.gameController && root.gameController.isPromotionPending) {
        root.gameController.cancelPromotion()
    }
}
```
**Verify**: Inspect `qml/dialogs/PromotionDialog.qml`.

### Step 2: Implement `switchToAnalysisMode()` Without Wiping Board
In `src/game/GameController.h`, declare Q_INVOKABLE slot:
```cpp
Q_INVOKABLE void switchToAnalysisMode();
```
In `src/game/GameController.cpp`:
```cpp
void GameController::switchToAnalysisMode() {
    m_gameMode = ModeAnalysis;
    emit gameModeChanged();
    m_clock->pause();
    m_isGameOver = false;
    emit isGameOverChanged();
    updateEnginePosition();
    m_uci->startAnalysis();
}
```
In `qml/dialogs/GameOverDialog.qml:55-58`:
Replace `root.gameController.gameMode = 2` with:
```qml
root.gameController.switchToAnalysisMode()
```
**Verify**: `ninja -C /root/Qt6Chess/build` → builds cleanly.

### Step 3: Prevent Clock Ticking in Analysis Mode
In `src/game/GameController.cpp:newGame(...)`:
```cpp
if (mode == ModeAnalysis) {
    m_clock->setUnlimited();
} else if (timeMin > 0) {
    m_clock->setTimeControl(timeMin, incSec);
    m_clock->start(ChessClock::White);
} else {
    m_clock->setUnlimited();
}
```
In `src/game/GameController.cpp:onClockTimeOut(int side)`:
```cpp
if (m_gameMode == ModeAnalysis || m_isGameOver) return;
m_clock->pause();
m_uci->stopAnalysis();
m_isThinking = false;
```
In `src/game/GameController.cpp:resignCurrentPlayer()`:
```cpp
m_clock->pause();
m_uci->stopAnalysis();
m_isThinking = false;
```
**Verify**: `ninja -C /root/Qt6Chess/build` → builds cleanly.

### Step 4: Standardize on `gameController` Context Property
In `src/main.cpp:55-56`:
Remove line 56 (`appController`).
In `qml/Main.qml`:
Replace all occurrences of `appController` with `gameController`.
**Verify**: `grep -rn "appController" qml/ src/` returns 0 results.

## Done criteria
- [x] `ninja -C /root/Qt6Chess/build` exits 0.
- [x] `ctest --test-dir /root/Qt6Chess/build` exits 0.
- [x] `grep -rn "appController" qml/ src/` returns empty.
- [x] `switchToAnalysisMode` exists in `GameController.h` and is invoked by `GameOverDialog.qml`.
