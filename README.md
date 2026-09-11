# Qt6Chess

A modern, high-performance, native desktop Chess application for Linux/KDE powered by **Qt 6.8+ (C++20 & QML / Qt Quick SceneGraph)** and **Stockfish 17**.

## Highlights
- **High-Performance C++20 Core**: Bitboard move generator with sub-microsecond legal move generation and full FIDE chess rules.
- **Hardware-Accelerated QML**: 60/120 FPS animations, smooth piece sliding, fluid drag-and-drop, and dynamic tactical arrows.
- **Stockfish UCI Integration**: Live continuous evaluation bar, MultiPV top candidate lines, and best move hints.
- **Multiple Game Modes**: Play vs Computer (Elo 800 - 3200+), Local 2-Player, Free Analysis Board, and PGN Database Explorer.
- **Themes & Audio**: Scalable vector SVG piece sets, board themes (Emerald, Wood, Midnight, Slate), and low-latency audio effects.

## Building
```bash
cmake -B build -GNinja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/bin/Qt6Chess
```
