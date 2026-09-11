# Plan 003: Model-View Optimization & History Truncation
> **Executor instructions**: Follow this plan step by step. Run every
> verification command and confirm the expected result before moving to the
> next step. If anything in the "STOP conditions" section occurs, stop and
> report — do not improvise.
>
> **Drift check (run first)**: `git diff --stat b68b881..HEAD -- src/core/ChessBoardModel.h src/core/ChessBoardModel.cpp src/game/MoveHistoryModel.h src/game/MoveHistoryModel.cpp src/game/GameController.cpp qml/board/ChessBoard.qml`

## Status
- **Priority**: P1
- **Effort**: M
- **Risk**: MED
- **Depends on**: none
- **Category**: perf, arch, bug
- **Planned at**: commit `b68b881`, 2026-09-11
- **Issue**: 

## Why this matters
`ChessBoard.qml` currently instantiates 64 squares with `Repeater { model: 64 }` and calls `getSquareData(sqIndex)` on every board revision, causing 640 heap allocations and heavy JavaScript marshaling per piece move or click. In addition, navigating to an earlier ply and making a move desynchronizes `m_playedMoves` from the board, sending corrupt move lists to Stockfish and desynchronizing `MoveHistoryModel`.

## Current state
- `qml/board/ChessBoard.qml:36`:
  ```qml
  Repeater {
      model: 64
      delegate: Item {
          readonly property var sqData: boardModel.getSquareData(sqIndex)
  ```
- `src/game/GameController.cpp:555-568`: `goToPly` changes `m_board` but leaves `m_playedMoves` containing all forward moves.
- `src/game/MoveHistoryModel.cpp:108`: `setCurrentPly` invalidates all rows with an empty roles vector.

## Commands you will need
| Purpose | Command | Expected on success |
|---------|---------|---------------------|
| Build | `ninja -C /root/Qt6Chess/build` | exit 0 |
| Test | `ctest --test-dir /root/Qt6Chess/build --output-on-failure` | 100% tests passed |

## Scope
**In scope**:
- `src/core/ChessBoardModel.h`
- `src/core/ChessBoardModel.cpp`
- `src/game/MoveHistoryModel.h`
- `src/game/MoveHistoryModel.cpp`
- `src/game/GameController.cpp`
- `qml/board/ChessBoard.qml`

## Steps
### Step 1: Bind `ChessBoard.qml` Directly to `boardModel` Roles
In `qml/board/ChessBoard.qml`:
Change `Repeater { model: 64 }` to:
```qml
Repeater {
    id: boardRepeater
    model: root.gameController ? root.gameController.boardModel : null
    delegate: Item {
        id: squareDelegate
        required property int index
        required property string pieceCode
        required property bool isSelected
        required property bool isLegalTarget
        required property bool isCaptureTarget
        required property bool isLastMoveFrom
        required property bool isLastMoveTo
        required property bool isInCheck
        required property bool isLightSquare
        required property int file
        required property int rank
```
Remove `readonly property var sqData`. Replace `sqData.pieceCode` with `pieceCode`, `sqData.isSelected` with `isSelected`, etc.
Remove `revision` dependency.
In `src/core/ChessBoardModel.cpp:refreshAll()`, ensure `emit dataChanged(index(0), index(63))` is called.
**Verify**: `ninja -C /root/Qt6Chess/build` → builds cleanly.

### Step 2: Implement Move History Truncation
In `src/game/MoveHistoryModel.h`:
Declare:
```cpp
void truncateAfterPly(int ply);
```
In `src/game/MoveHistoryModel.cpp`:
```cpp
void MoveHistoryModel::truncateAfterPly(int ply) {
    if (ply < 0) ply = 0;
    int targetTurns = (ply + 1) / 2;
    if (targetTurns < (int)m_turns.size()) {
        beginRemoveRows(QModelIndex(), targetTurns, m_turns.size() - 1);
        m_turns.erase(m_turns.begin() + targetTurns, m_turns.end());
        endRemoveRows();
    }
    if (ply % 2 != 0 && !m_turns.empty()) {
        // Last turn has white move only
        int lastIdx = m_turns.size() - 1;
        m_turns[lastIdx].blackSan.clear();
        m_turns[lastIdx].blackUci.clear();
        m_turns[lastIdx].blackPly = -1;
        emit dataChanged(index(lastIdx), index(lastIdx));
    }
    if (ply + 1 < (int)m_fenHistory.size()) {
        m_fenHistory.erase(m_fenHistory.begin() + (ply + 1), m_fenHistory.end());
    }
    m_currentPly = ply;
    emit currentPlyChanged();
}
```
In `src/game/GameController.cpp:tryMove()`:
If `m_currentPly < (int)m_playedMoves.size()`:
```cpp
m_playedMoves.resize(m_currentPly);
m_history->truncateAfterPly(m_currentPly);
```
In `src/game/GameController.cpp:undoMove()`:
Truncate history after `newPly`.
In `src/game/GameController.cpp:goToPly(int ply)`:
Send only moves up to `ply` to `m_uci->setPosition(m_startFen, movesUpToPly)`.
**Verify**: `ninja -C /root/Qt6Chess/build` → builds cleanly.

### Step 3: Narrow `MoveHistoryModel::dataChanged`
In `src/game/MoveHistoryModel.cpp:setCurrentPly(int ply)`:
Only emit `dataChanged` for the previous turn row and new turn row with `{IsWhiteSelectedRole, IsBlackSelectedRole}`.
**Verify**: `ninja -C /root/Qt6Chess/build` → builds cleanly.

### Step 4: Silent Batch Loading in `loadPgn()`
In `src/game/GameController.cpp:loadPgn(const QString &pgn)`:
Parse moves and apply them to `m_board` and `m_playedMoves` without calling `m_sound->playMove()` or restarting the engine on every intermediate move. After applying all moves, update view once, update engine position once, and play sound once.
**Verify**: `ninja -C /root/Qt6Chess/build` → builds cleanly.

## Done criteria
- [x] `ninja -C /root/Qt6Chess/build` exits 0.
- [x] `ctest --test-dir /root/Qt6Chess/build` exits 0.
- [x] `ChessBoard.qml` contains 0 calls to `getSquareData`.
- [x] Making a move from earlier ply truncates history properly without crash.
