# Reddit Post Drafts — Qt6Chess

---

## 🎯 Short & Punchy Post (Recommended)

### Title Ideas (pick one based on subreddit):
- **r/unixporn / r/commandline**:
  > *I suck at chess, but I hate Electron apps. So I made a 30MB native Qt6 & ANSI terminal chess app in C++20.*
- **r/archlinux / r/linux**:
  > *Qt6Chess: A lightweight native C++20/Qt6 chess client with Stockfish 17 & interactive ANSI terminal mode (30MB RAM, Arch pkg)*
- **r/chess**:
  > *Built a lightweight, offline desktop chess & analysis board with Stockfish 17 MultiPV (Zero Electron)*

### Post Body:

I'm terrible at chess (as the video clearly proves), but I hate 600MB Electron-wrapped chess apps even more.

So I built **Qt6Chess** in C++20 and Qt 6.8:

- **Starts in <50ms, sits at ~30MB RAM** (no Chromium, no web bloat)
- **Hardware-accelerated GUI**: Smooth 60/120 FPS animations, clean SVG vector pieces, and 4 themes (Emerald, Wood, Slate, Midnight)
- **Interactive Terminal TUI (`qt6chess --cli`)**: Mouse click support right inside the terminal, TrueColor ANSI pieces, and keyboard navigation
- **Stockfish 17.1 Built-in**: Real-time evaluation bar, MultiPV (top 3 candidate lines), tactical arrows, and customizable Elo (800 to 3000+)
- **Offline Study**: Instant FEN & PGN import/export with move history branching
- **Pre-compiled Arch Linux package** ready to install with `pacman -U`

**Repo**: https://github.com/AhooraZen/Qt6Chess  
**Releases & Arch Package**: https://github.com/AhooraZen/Qt6Chess/releases/tag/v1.0.3

Arch one-liner:
```bash
sudo pacman -U https://github.com/AhooraZen/Qt6Chess/releases/download/v1.0.3/qt6chess-1.0.3-1-x86_64.pkg.tar.zst
```

Give it a spin and let me know what breaks! (And feel free to roast my opening in the comments).

---

## 💡 Subreddit Checklist:
1. **r/commandline** — Post with a screen recording of mouse clicking in `qt6chess --cli`.
2. **r/unixporn** — Post on weekend / screenshot day with your desktop rice + TUI and GUI side-by-side.
3. **r/archlinux** — Post as a native community tool showcase.
4. **r/chess** — Focus on the zero-distraction offline analysis and Stockfish MultiPV.
