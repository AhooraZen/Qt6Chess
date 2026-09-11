# Plan 006: Product Direction — MultiPV, Themes & Ingestion
> **Executor instructions**: Follow this plan step by step. Run every
> verification command and confirm the expected result before moving to the
> next step. If anything in the "STOP conditions" section occurs, stop and
> report — do not improvise.
>
> **Drift check (run first)**: `git diff --stat b68b881..HEAD -- src/engine/UciController.h src/engine/UciController.cpp qml/components/EnginePanel.qml qml/board/ChessBoard.qml qml/Main.qml qml/components/MoveHistory.qml`

## Status
- **Priority**: P2
- **Effort**: M
- **Risk**: LOW
- **Depends on**: plans/001-engine-eval-sync.md, plans/003-model-view-history.md
- **Category**: direction, features
- **Planned at**: commit `b68b881`, 2026-09-11
- **Issue**: 

## Why this matters
Desktop chess players expect to see alternative candidate lines (MultiPV) during analysis to understand why alternative moves fail. Users also expect visual customization (themes: Wood, Slate, Midnight, Emerald) and the ability to import FEN puzzles and PGN games from clipboard or files instead of only being able to export them.

## Current state
- `src/engine/UciController.h:68`: Declares `lineUpdated` signal with `pvRank, eval, isMate, mateIn, depth, pv`, but QML ignores it.
- `qml/board/ChessBoard.qml:11-16`: Light/dark tile colors are hardcoded to `#eeeed2` and `#769656`.
- `qml/Main.qml:183`: Only "Copy FEN" exists; no "Load / Paste FEN" action.
- `qml/components/MoveHistory.qml:28`: Only "Copy PGN" exists; no "Load / Paste PGN" action.

## Commands you will need
| Purpose | Command | Expected on success |
|---------|---------|---------------------|
| Build | `ninja -C /root/Qt6Chess/build` | exit 0 |
| Test | `ctest --test-dir /root/Qt6Chess/build --output-on-failure` | 100% tests passed |

## Scope
**In scope**:
- `src/engine/UciController.h`
- `src/engine/UciController.cpp`
- `qml/components/EnginePanel.qml`
- `qml/board/ChessBoard.qml`
- `qml/Main.qml`
- `qml/components/MoveHistory.qml`

## Steps
### Step 1: Expose MultiPV Candidate Lines to QML
In `src/engine/UciController.h`:
Store `QVariantList m_candidateLines;` with `Q_PROPERTY(QVariantList candidateLines READ candidateLines NOTIFY candidateLinesChanged)`.
In `src/engine/UciController.cpp`:
On `info depth ... pv ...`, update the list of candidate line maps:
`{ "rank": pvRank, "score": evalStr, "pv": pvString }`.
Emit `candidateLinesChanged()`.
In `qml/components/EnginePanel.qml`:
Add a `ListView` displaying candidate lines with their evaluation badges and principal variation moves.
**Verify**: `ninja -C /root/Qt6Chess/build` → builds cleanly.

### Step 2: Board Theme Presets
In `qml/board/ChessBoard.qml`:
Add `property int boardTheme: 0 // 0: Emerald, 1: Wood, 2: Slate, 3: Midnight`.
Define theme color maps:
- Emerald: `#eeeed2` / `#769656`
- Wood: `#f0d9b5` / `#b58863`
- Slate: `#dee3e6` / `#8ca2ad`
- Midnight: `#c4cfa1` / `#4d6a79`
In `qml/Main.qml`:
Add a Theme selector dropdown in the sidebar to switch `boardTheme`.
**Verify**: `ninja -C /root/Qt6Chess/build` → builds cleanly.

### Step 3: FEN and PGN Ingestion Dialogs
In `qml/Main.qml`:
Add "Paste FEN" button next to "Copy FEN". When clicked, open a small dialog or prompt to paste FEN and call `gameController.loadFen(pastedFen)`.
In `qml/components/MoveHistory.qml`:
Add "Paste PGN" button next to "Copy PGN". When clicked, prompt user or paste clipboard text into `gameController.loadPgn(pastedPgn)`.
**Verify**: `ninja -C /root/Qt6Chess/build` → builds cleanly.

## Done criteria
- [x] `ninja -C /root/Qt6Chess/build` exits 0.
- [x] `EnginePanel.qml` displays MultiPV candidate lines.
- [x] `ChessBoard.qml` supports selectable board themes.
- [x] FEN paste and PGN paste dialogs function in GUI.
