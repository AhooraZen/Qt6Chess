#include "TerminalChess.h"
#include "chess.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <thread>
#include <cmath>

#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <signal.h>

#include <QProcess>
#include <QString>
#include <QStringList>
#include <QStandardPaths>
#include <QFile>

namespace TerminalChess {

// Terminal state management
static struct termios s_origTermios;
static bool s_rawModeActive = false;

static void disableRawMode()
{
    if (s_rawModeActive) {
        // Disable SGR mouse tracking & normal mouse tracking
        std::cout << "\033[?1006l\033[?1000l";
        // Show cursor
        std::cout << "\033[?25h";
        // Exit alternate screen buffer
        std::cout << "\033[?1049l";
        // Reset ANSI colors
        std::cout << "\033[0m" << std::flush;

        tcsetattr(STDIN_FILENO, TCSAFLUSH, &s_origTermios);
        s_rawModeActive = false;
    }
}

static void enableRawMode()
{
    if (tcgetattr(STDIN_FILENO, &s_origTermios) == -1) return;

    struct termios raw = s_origTermios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1; // 100ms read timeout

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) return;
    s_rawModeActive = true;

    std::atexit(disableRawMode);

    // Enter alternate screen buffer
    std::cout << "\033[?1049h";
    // Enable SGR mouse tracking (mode 1000 + 1006)
    std::cout << "\033[?1000h\033[?1006h";
    // Hide cursor during board draw
    std::cout << "\033[?25l" << std::flush;
}

static void signalHandler(int signum)
{
    (void)signum;
    disableRawMode();
    _exit(0);
}

// Find Stockfish executable
static QString locateStockfish()
{
    QString found = QStandardPaths::findExecutable(QStringLiteral("stockfish"));
    if (!found.isEmpty()) return found;

    const QStringList candidates = {
        QStringLiteral("/usr/bin/stockfish"),
        QStringLiteral("/usr/games/stockfish"),
        QStringLiteral("/usr/local/bin/stockfish"),
        QStringLiteral("/opt/stockfish/stockfish")
    };
    for (const auto &c : candidates) {
        if (QFile::exists(c)) return c;
    }
    return QString();
}

struct EvalInfo {
    bool hasScore = false;
    bool isMate = false;
    int score = 0; // Centipawns or mate distance
    int depth = 0;
    std::string bestMoveStr;
};

static EvalInfo queryStockfishEval(QProcess &engine, const chess::Board &board, int timeMs = 800)
{
    EvalInfo info;
    if (engine.state() != QProcess::Running) {
        chess::Movelist legal;
        chess::movegen::legalmoves(legal, board);
        if (!legal.empty()) info.bestMoveStr = chess::uci::moveToUci(legal[0]);
        return info;
    }

    // Drain any leftover data in buffer
    while (engine.bytesAvailable() > 0) {
        engine.readAll();
    }

    std::string fen = board.getFen();
    engine.write(QString("position fen %1\n").arg(QString::fromStdString(fen)).toUtf8());
    engine.write(QString("go movetime %1\n").arg(timeMs).toUtf8());
    engine.waitForBytesWritten(300);

    auto startTime = std::chrono::steady_clock::now();
    int timeoutMs = timeMs + 3500;

    while (true) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
        if (elapsed > timeoutMs) break;

        if (!engine.waitForReadyRead(200)) {
            continue;
        }

        while (engine.canReadLine()) {
            QString line = QString::fromUtf8(engine.readLine()).trimmed();
            if (line.startsWith("info ") && line.contains("score ")) {
                QStringList parts = line.split(' ', Qt::SkipEmptyParts);
                for (int i = 0; i < parts.size(); ++i) {
                    if (parts[i] == "depth" && i + 1 < parts.size()) {
                        info.depth = parts[i + 1].toInt();
                    } else if (parts[i] == "score" && i + 2 < parts.size()) {
                        info.hasScore = true;
                        if (parts[i + 1] == "cp") {
                            info.isMate = false;
                            info.score = parts[i + 2].toInt();
                        } else if (parts[i + 1] == "mate") {
                            info.isMate = true;
                            info.score = parts[i + 2].toInt();
                        }
                    }
                }
            } else if (line.startsWith("bestmove ")) {
                QStringList parts = line.split(' ', Qt::SkipEmptyParts);
                if (parts.size() >= 2 && parts[1] != "(none)") {
                    info.bestMoveStr = parts[1].toStdString();
                    return info;
                }
            }
        }
    }

    // Fallback if Stockfish timed out
    if (info.bestMoveStr.empty()) {
        chess::Movelist legal;
        chess::movegen::legalmoves(legal, board);
        if (!legal.empty()) {
            info.bestMoveStr = chess::uci::moveToUci(legal[0]);
        }
    }
    return info;
}


// Key & Mouse input definitions
enum KeyType {
    KEY_NONE = 0,
    KEY_UP,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_ENTER,
    KEY_SPACE,
    KEY_ESC,
    KEY_BACKSPACE,
    KEY_CHAR,
    KEY_MOUSE_CLICK,
    KEY_MOUSE_WHEEL_UP,
    KEY_MOUSE_WHEEL_DOWN
};

struct InputEvent {
    KeyType type = KEY_NONE;
    char ch = 0;
    int mouseX = 0;
    int mouseY = 0;
    int mouseButton = 0;
    bool mousePress = false;
};

static InputEvent readInputEvent()
{
    InputEvent ev;
    char c = 0;
    if (read(STDIN_FILENO, &c, 1) <= 0) {
        return ev;
    }

    if (c == 27) { // Escape sequence
        char seq[64];
        seq[0] = 0;
        int n = 0;

        // Non-blocking read remaining sequence
        struct timeval tv = {0, 40000}; // 40ms
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);

        if (select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv) > 0) {
            char c2 = 0;
            if (read(STDIN_FILENO, &c2, 1) > 0) {
                if (c2 == '[') {
                    // Could be arrow or mouse
                    char c3 = 0;
                    if (read(STDIN_FILENO, &c3, 1) > 0) {
                        if (c3 == 'A') { ev.type = KEY_UP; return ev; }
                        if (c3 == 'B') { ev.type = KEY_DOWN; return ev; }
                        if (c3 == 'C') { ev.type = KEY_RIGHT; return ev; }
                        if (c3 == 'D') { ev.type = KEY_LEFT; return ev; }

                        if (c3 == '<') {
                            // SGR 1006 mouse event: <btn;x;y;M/m
                            std::string sgr;
                            while (true) {
                                char sc = 0;
                                if (read(STDIN_FILENO, &sc, 1) <= 0) break;
                                sgr.push_back(sc);
                                if (sc == 'M' || sc == 'm') break;
                                if (sgr.size() > 32) break;
                            }
                            if (!sgr.empty() && (sgr.back() == 'M' || sgr.back() == 'm')) {
                                char action = sgr.back();
                                sgr.pop_back();
                                std::replace(sgr.begin(), sgr.end(), ';', ' ');
                                std::stringstream ss(sgr);
                                int btn = 0, x = 0, y = 0;
                                if (ss >> btn >> x >> y) {
                                    ev.mouseX = x;
                                    ev.mouseY = y;
                                    ev.mouseButton = btn;
                                    ev.mousePress = (action == 'M');
                                    if (btn == 64) ev.type = KEY_MOUSE_WHEEL_UP;
                                    else if (btn == 65) ev.type = KEY_MOUSE_WHEEL_DOWN;
                                    else if (btn == 0 && ev.mousePress) ev.type = KEY_MOUSE_CLICK;
                                    return ev;
                                }
                            }
                        }
                    }
                }
            }
        }
        ev.type = KEY_ESC;
        return ev;
    }

    if (c == '\r' || c == '\n') {
        ev.type = KEY_ENTER;
        return ev;
    }
    if (c == ' ') {
        ev.type = KEY_SPACE;
        return ev;
    }
    if (c == 127 || c == '\b') {
        ev.type = KEY_BACKSPACE;
        return ev;
    }

    ev.type = KEY_CHAR;
    ev.ch = c;
    return ev;
}

// Captured pieces helper
struct CapturedCount {
    int whitePawns = 0, whiteKnights = 0, whiteBishops = 0, whiteRooks = 0, whiteQueens = 0;
    int blackPawns = 0, blackKnights = 0, blackBishops = 0, blackRooks = 0, blackQueens = 0;
    int materialDiff = 0; // > 0 White lead, < 0 Black lead
};

static CapturedCount computeCaptured(const chess::Board &board)
{
    CapturedCount c;
    int wp = 0, wn = 0, wb = 0, wr = 0, wq = 0;
    int bp = 0, bn = 0, bb = 0, br = 0, bq = 0;

    for (int sq = 0; sq < 64; ++sq) {
        chess::Piece p = board.at(chess::Square(sq));
        if (p == chess::Piece::NONE) continue;
        if (p.color() == chess::Color::WHITE) {
            if (p.type() == chess::PieceType::PAWN) wp++;
            else if (p.type() == chess::PieceType::KNIGHT) wn++;
            else if (p.type() == chess::PieceType::BISHOP) wb++;
            else if (p.type() == chess::PieceType::ROOK) wr++;
            else if (p.type() == chess::PieceType::QUEEN) wq++;
        } else {
            if (p.type() == chess::PieceType::PAWN) bp++;
            else if (p.type() == chess::PieceType::KNIGHT) bn++;
            else if (p.type() == chess::PieceType::BISHOP) bb++;
            else if (p.type() == chess::PieceType::ROOK) br++;
            else if (p.type() == chess::PieceType::QUEEN) bq++;
        }
    }

    // Pieces captured by White (missing black pieces)
    c.blackPawns = std::max(0, 8 - bp);
    c.blackKnights = std::max(0, 2 - bn);
    c.blackBishops = std::max(0, 2 - bb);
    c.blackRooks = std::max(0, 2 - br);
    c.blackQueens = std::max(0, 1 - bq);

    // Pieces captured by Black (missing white pieces)
    c.whitePawns = std::max(0, 8 - wp);
    c.whiteKnights = std::max(0, 2 - wn);
    c.whiteBishops = std::max(0, 2 - wb);
    c.whiteRooks = std::max(0, 2 - wr);
    c.whiteQueens = std::max(0, 1 - wq);

    int whiteVal = wp * 1 + wn * 3 + wb * 3 + wr * 5 + wq * 9;
    int blackVal = bp * 1 + bn * 3 + bb * 3 + br * 5 + bq * 9;
    c.materialDiff = whiteVal - blackVal;

    return c;
}

static std::string repeatStr(const std::string &str, int n)
{
    std::string res;
    for (int i = 0; i < n; ++i) res += str;
    return res;
}

// Main TUI Runner
int run(int elo, int playerColor)
{
    struct sigaction sa;
    sa.sa_handler = signalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    enableRawMode();

    chess::Board board;
    std::vector<chess::Move> history;
    std::vector<std::string> sanHistory;

    bool flipped = (playerColor == 2); // Black perspective
    int cursorFile = 4; // e
    int cursorRank = 3; // 4 (starts at e4)
    int selectedSquare = -1;
    std::vector<chess::Move> legalFromSelected;

    chess::Square lastFrom = chess::Square::underlying::NO_SQ;
    chess::Square lastTo = chess::Square::underlying::NO_SQ;

    bool showHelp = false;
    std::string statusMessage = "Welcome! Use Mouse click or Keyboard (Arrows/Enter) to play.";
    EvalInfo currentEval;

    // Connect Stockfish
    QString sfPath = locateStockfish();
    QProcess stockfish;
    bool sfAvailable = false;

    if (!sfPath.isEmpty()) {
        stockfish.start(sfPath);
        if (stockfish.waitForStarted(3000)) {
            stockfish.write("uci\n");
            stockfish.write(QString("setoption name UCI_LimitStrength value true\n").toUtf8());
            stockfish.write(QString("setoption name UCI_Elo value %1\n").arg(elo).toUtf8());
            stockfish.write("isready\n");
            stockfish.waitForBytesWritten(500);

            auto tStart = std::chrono::steady_clock::now();
            while (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - tStart).count() < 4000) {
                if (stockfish.waitForReadyRead(300)) {
                    bool ready = false;
                    while (stockfish.canReadLine()) {
                        if (QString::fromUtf8(stockfish.readLine()).trimmed() == QStringLiteral("readyok")) {
                            ready = true;
                            break;
                        }
                    }
                    if (ready) break;
                }
            }

            sfAvailable = true;
            statusMessage = "Stockfish connected (Elo " + std::to_string(elo) + ").";
        }
    }


    if (!sfAvailable) {
        statusMessage = "Stockfish not detected in PATH. Local Two-Player (Pass & Play) active.";
    }

    auto getSquareAtScreen = [&](int screenX, int screenY) -> int {
        int boardLeft = 5;
        int boardTop = 4;
        int relX = screenX - boardLeft;
        int relY = screenY - boardTop;
        if (relX < 0 || relX >= 32 || relY < 0 || relY >= 16) return -1;
        int displayFile = relX / 4;
        int displayRank = relY / 2;
        int file = flipped ? (7 - displayFile) : displayFile;
        int rank = flipped ? displayRank : (7 - displayRank);
        return rank * 8 + file;
    };

    auto updateLegalMovesForSelected = [&]() {
        legalFromSelected.clear();
        if (selectedSquare < 0 || selectedSquare >= 64) return;
        chess::Movelist moves;
        chess::movegen::legalmoves(moves, board);
        for (const auto &m : moves) {
            if (m.from() == chess::Square(selectedSquare)) {
                legalFromSelected.push_back(m);
            }
        }
    };

    // Render loop
    while (true) {
        // Clear screen and go to (1, 1)
        std::string out;
        out.reserve(4096);
        out += "\033[H";

        // Dimensions
        struct winsize ws;
        int termCols = 80;
        int termRows = 24;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
            if (ws.ws_col > 0) termCols = ws.ws_col;
            if (ws.ws_row > 0) termRows = ws.ws_row;
        }

        // Header Title
        out += "\033[1;36m ♞ Qt6Chess Terminal TUI \033[0m\033[90m│ C++20 SceneGraph & ANSI SGR Edition\033[0m\n";
        out += "\033[90m" + repeatStr("─", std::min(termCols, 78)) + "\033[0m\n";

        // If Help Modal is open
        if (showHelp) {
            out += "\n";
            out += "\033[1;33m  ┌───────────────────────── Qt6Chess TUI Help ─────────────────────────┐\033[0m\n";
            out += "  │                                                                     │\n";
            out += "  │  \033[1;32mMouse Controls:\033[0m                                                    │\n";
            out += "  │    • \033[1mLeft Click on Piece\033[0m  : Select piece and view legal targets        │\n";
            out += "  │    • \033[1mLeft Click on Target\033[0m : Make the chess move                       │\n";
            out += "  │    • \033[1mLeft Click Elsewhere\033[0m: Deselect / Cancel move                     │\n";
            out += "  │                                                                     │\n";
            out += "  │  \033[1;32mKeyboard Controls:\033[0m                                                 │\n";
            out += "  │    • \033[1mArrow Keys / WASD / HJKL\033[0m : Move yellow board cursor              │\n";
            out += "  │    • \033[1mSpace / Enter\033[0m            : Select piece or execute target move   │\n";
            out += "  │    • \033[1mu\033[0m                        : Undo last move(s)                     │\n";
            out += "  │    • \033[1mf\033[0m                        : Flip board orientation (White/Black)  │\n";
            out += "  │    • \033[1me\033[0m                        : Query Stockfish evaluation & bestmove │\n";
            out += "  │    • \033[1m/\033[0m                        : Enter SAN move directly (e.g. 'Nf3')  │\n";
            out += "  │    • \033[1mr / n\033[0m                    : Restart / New Game                    │\n";
            out += "  │    • \033[1mh / ?\033[0m                    : Toggle this Help popup                │\n";
            out += "  │    • \033[1mq / Esc\033[0m                  : Quit Qt6Chess                         │\n";
            out += "  │                                                                     │\n";
            out += "  │  \033[1;32mPacman / Arch Note:\033[0m                                                │\n";
            out += "  │    Stockfish is optional! If installed, AI play and live eval       │\n";
            out += "  │    activate automatically. Otherwise, Pass & Play mode works!       │\n";
            out += "  │                                                                     │\n";
            out += "\033[1;33m  └────────────────────── [Press any key to close] ─────────────────────┘\033[0m\n";
            for (int i = 0; i < 6; ++i) out += "\n";
            std::cout << out << std::flush;

            InputEvent helpEv = readInputEvent();
            if (helpEv.type != KEY_NONE) {
                showHelp = false;
            }
            continue;
        }

        // Prepare side panel lines
        std::vector<std::string> sideLines;
        sideLines.push_back("");

        // Engine badge
        if (sfAvailable) {
            sideLines.push_back("\033[32m●\033[0m \033[1mStockfish 17.1\033[0m \033[90m(Elo " + std::to_string(elo) + ")\033[0m");
        } else {
            sideLines.push_back("\033[33m○\033[0m \033[1mTwo-Player Mode\033[0m \033[90m(Engine offline)\033[0m");
        }

        // Side to move
        bool isWhiteTurn = (board.sideToMove() == chess::Color::WHITE);
        std::string turnBadge = isWhiteTurn ? "\033[1;37m▶ White to move\033[0m" : "\033[1;33m▶ Black to move\033[0m";
        if (board.inCheck()) {
            turnBadge += " \033[1;31m[CHECK!]\033[0m";
        }
        sideLines.push_back(turnBadge);

        // Evaluation meter
        if (currentEval.hasScore) {
            std::string evalStr;
            int filledBlocks = 8;
            if (currentEval.isMate) {
                evalStr = "\033[1;35mMate in " + std::to_string(currentEval.score) + "\033[0m";
                filledBlocks = (currentEval.score > 0) ? 16 : 0;
            } else {
                double pawns = currentEval.score / 100.0;
                std::stringstream ss;
                ss << std::fixed << std::setprecision(1) << (pawns > 0 ? "+" : "") << pawns;
                evalStr = "\033[1;32m" + ss.str() + "\033[0m";
                filledBlocks = std::clamp(8 + static_cast<int>(std::round(pawns * 2.0)), 0, 16);
            }
            std::string bar = "\033[37m[";
            for (int b = 0; b < 16; ++b) {
                if (b < filledBlocks) bar += "\033[32m█";
                else bar += "\033[90m░";
            }
            bar += "\033[37m]\033[0m " + evalStr + " \033[90m(d" + std::to_string(currentEval.depth) + ")\033[0m";
            sideLines.push_back("Eval: " + bar);
            if (!currentEval.bestMoveStr.empty()) {
                sideLines.push_back("\033[90mBest move: \033[1;36m" + currentEval.bestMoveStr + "\033[0m");
            } else {
                sideLines.push_back("");
            }
        } else {
            sideLines.push_back("Eval: \033[90m[Press 'e' for evaluation]\033[0m");
            sideLines.push_back("");
        }

        // Material Advantage / Captured pieces
        CapturedCount capt = computeCaptured(board);
        std::string capWhite = "Capt: ";
        for (int i = 0; i < capt.blackQueens; ++i) capWhite += "♛";
        for (int i = 0; i < capt.blackRooks; ++i) capWhite += "♜";
        for (int i = 0; i < capt.blackBishops; ++i) capWhite += "♝";
        for (int i = 0; i < capt.blackKnights; ++i) capWhite += "♞";
        for (int i = 0; i < capt.blackPawns; ++i) capWhite += "♟";
        if (capt.materialDiff > 0) capWhite += " (+" + std::to_string(capt.materialDiff) + ")";
        sideLines.push_back(capWhite);

        std::string capBlack = "      ";
        for (int i = 0; i < capt.whiteQueens; ++i) capBlack += "♕";
        for (int i = 0; i < capt.whiteRooks; ++i) capBlack += "♖";
        for (int i = 0; i < capt.whiteBishops; ++i) capBlack += "♗";
        for (int i = 0; i < capt.whiteKnights; ++i) capBlack += "♘";
        for (int i = 0; i < capt.whitePawns; ++i) capBlack += "♙";
        if (capt.materialDiff < 0) capBlack += " (+" + std::to_string(-capt.materialDiff) + ")";
        sideLines.push_back(capBlack);

        sideLines.push_back("\033[90m───────────────────────────────────────\033[0m");
        sideLines.push_back("\033[1mRecent Moves:\033[0m");

        // Last moves history
        int totalSan = static_cast<int>(sanHistory.size());
        int startMoveIndex = std::max(0, totalSan - 6);
        if (sanHistory.empty()) {
            sideLines.push_back("\033[90m  (No moves yet)\033[0m");
        } else {
            for (int m = startMoveIndex; m < totalSan; m += 2) {
                int moveNum = (m / 2) + 1;
                std::string line = "  " + std::to_string(moveNum) + ". " + sanHistory[m];
                while (line.size() < 14) line += " ";
                if (m + 1 < totalSan) {
                    line += sanHistory[m + 1];
                }
                sideLines.push_back(line);
            }
        }

        while (sideLines.size() < 18) {
            sideLines.push_back("");
        }

        // File headers (a - h)
        out += "     ";
        for (int c = 0; c < 8; ++c) {
            int file = flipped ? (7 - c) : c;
            char fileChar = 'a' + file;
            out += " ";
            out += fileChar;
            out += "  ";
        }
        out += "   ";
        if (!sideLines.empty()) out += sideLines[0];
        out += "\n";

        // Board rendering: 8 ranks, 2 lines per rank = 16 lines
        for (int displayRank = 0; displayRank < 8; ++displayRank) {
            int rank = flipped ? displayRank : (7 - displayRank);

            for (int subLine = 0; subLine < 2; ++subLine) {
                // Rank coordinate on left (on line 0)
                if (subLine == 0) {
                    out += "  \033[90m" + std::to_string(rank + 1) + "\033[0m  ";
                } else {
                    out += "     ";
                }

                for (int displayFile = 0; displayFile < 8; ++displayFile) {
                    int file = flipped ? (7 - displayFile) : displayFile;
                    int sqIndex = rank * 8 + file;
                    bool isLight = ((file + rank) % 2 != 0);

                    bool isSelected = (sqIndex == selectedSquare);
                    bool isCursor = (file == cursorFile && rank == cursorRank);
                    bool isLastMove = (lastFrom != chess::Square::underlying::NO_SQ &&
                                      (chess::Square(sqIndex) == lastFrom || chess::Square(sqIndex) == lastTo));

                    bool isLegalTarget = false;
                    for (const auto &lm : legalFromSelected) {
                        if (lm.to() == chess::Square(sqIndex)) {
                            isLegalTarget = true;
                            break;
                        }
                    }

                    chess::Piece piece = board.at(chess::Square(sqIndex));
                    bool isCaptureTarget = (isLegalTarget && piece != chess::Piece::NONE);

                    // Background color determination (24-bit TrueColor)
                    std::string bg;
                    if (isSelected) {
                        bg = "\033[48;2;246;246;105m"; // Golden yellow highlight
                    } else if (isCaptureTarget) {
                        bg = "\033[48;2;200;75;75m"; // Vivid red capture target
                    } else if (isLegalTarget) {
                        bg = "\033[48;2;135;160;105m"; // Subtle green target
                    } else if (isLastMove) {
                        bg = "\033[48;2;205;210;106m"; // Olive amber last move
                    } else if (isLight) {
                        bg = "\033[48;2;240;217;181m"; // Warm cream wood light tile
                    } else {
                        bg = "\033[48;2;181;136;99m";  // Rich walnut dark tile
                    }

                    // Piece glyph and color
                    std::string pieceGlyph = " ";
                    std::string fg = "\033[38;2;255;255;255;1m"; // White pieces
                    if (piece != chess::Piece::NONE) {
                        if (piece.color() == chess::Color::WHITE) {
                            fg = "\033[38;2;255;255;255;1m"; // Bright porcelain white
                            if (piece.type() == chess::PieceType::PAWN) pieceGlyph = "♙";
                            else if (piece.type() == chess::PieceType::KNIGHT) pieceGlyph = "♘";
                            else if (piece.type() == chess::PieceType::BISHOP) pieceGlyph = "♗";
                            else if (piece.type() == chess::PieceType::ROOK) pieceGlyph = "♖";
                            else if (piece.type() == chess::PieceType::QUEEN) pieceGlyph = "♕";
                            else if (piece.type() == chess::PieceType::KING) pieceGlyph = "♔";
                        } else {
                            fg = "\033[38;2;20;20;20;1m"; // Deep obsidian black
                            if (piece.type() == chess::PieceType::PAWN) pieceGlyph = "♟";
                            else if (piece.type() == chess::PieceType::KNIGHT) pieceGlyph = "♞";
                            else if (piece.type() == chess::PieceType::BISHOP) pieceGlyph = "♝";
                            else if (piece.type() == chess::PieceType::ROOK) pieceGlyph = "♜";
                            else if (piece.type() == chess::PieceType::QUEEN) pieceGlyph = "♛";
                            else if (piece.type() == chess::PieceType::KING) pieceGlyph = "♚";
                        }
                    } else if (isLegalTarget && subLine == 1) {
                        pieceGlyph = "•";
                        fg = "\033[38;2;40;40;40;1m";
                    }

                    // Render square (4 characters wide)
                    out += bg;
                    if (subLine == 0) {
                        // Top line of square: cursor brackets or empty
                        if (isCursor) {
                            out += "\033[1;36m┌──┐\033[0m" + bg;
                        } else {
                            out += "    ";
                        }
                    } else {
                        // Bottom line with piece glyph
                        if (isCursor) {
                            out += "\033[1;36m│\033[0m" + bg + fg + pieceGlyph + "\033[0m" + bg + " \033[1;36m│\033[0m";
                        } else {
                            out += " " + fg + pieceGlyph + "\033[0m" + bg + "  ";
                        }
                    }
                    out += "\033[0m";
                }

                // Rank coordinate on right
                if (subLine == 0) {
                    out += "  \033[90m" + std::to_string(rank + 1) + "\033[0m";
                } else {
                    out += "     ";
                }

                // Side panel lines
                int sideIdx = displayRank * 2 + subLine + 1;
                if (sideIdx < static_cast<int>(sideLines.size())) {
                    out += "  " + sideLines[sideIdx];
                }
                out += "\n";
            }
        }

        // File headers (a - h) bottom
        out += "     ";
        for (int c = 0; c < 8; ++c) {
            int file = flipped ? (7 - c) : c;
            char fileChar = 'a' + file;
            out += " ";
            out += fileChar;
            out += "  ";
        }
        out += "\n";

        // Status message bar
        out += "\033[90m" + repeatStr("─", std::min(termCols, 78)) + "\033[0m\n";
        out += " \033[1mStatus:\033[0m " + statusMessage + "\n";
        out += " \033[90m[🖱 Click/Enter] Move  [Arrows/WASD] Cursor  [u] Undo  [f] Flip  [e] Eval  [/] SAN  [?] Help  [q] Quit\033[0m\n";

        // Flush buffer
        std::cout << out << std::flush;

        // Check game over
        auto [reason, result] = board.isGameOver();
        if (result != chess::GameResult::NONE) {
            std::string overMsg;
            if (reason == chess::GameResultReason::CHECKMATE) {
                overMsg = (board.sideToMove() == chess::Color::WHITE) ? "Black wins by Checkmate!" : "White wins by Checkmate!";
            } else if (reason == chess::GameResultReason::STALEMATE) {
                overMsg = "Draw by Stalemate!";
            } else if (reason == chess::GameResultReason::INSUFFICIENT_MATERIAL) {
                overMsg = "Draw by Insufficient Material!";
            } else if (reason == chess::GameResultReason::FIFTY_MOVE_RULE) {
                overMsg = "Draw by Fifty-move Rule!";
            } else {
                overMsg = "Game Over!";
            }
            statusMessage = "\033[1;31m" + overMsg + "\033[0m (Press 'r' for new game, 'q' to quit)";
        }

        // Check if Stockfish's turn to move
        bool isHumanTurn = (playerColor == 3) ||
                           (playerColor == 1 && isWhiteTurn) ||
                           (playerColor == 2 && !isWhiteTurn);

        if (!isHumanTurn && sfAvailable && result == chess::GameResult::NONE) {
            statusMessage = "\033[1;33mStockfish is thinking...\033[0m";
            std::cout << "\033[H\033[" << (termRows - 1) << ";1H \033[1mStatus:\033[0m " << statusMessage << std::flush;

            EvalInfo engineMove = queryStockfishEval(stockfish, board, 1000);
            chess::Move finalMove = chess::Move::NO_MOVE;
            if (!engineMove.bestMoveStr.empty()) {
                try {
                    finalMove = chess::uci::uciToMove(board, engineMove.bestMoveStr);
                } catch (...) {
                    finalMove = chess::Move::NO_MOVE;
                }
            }

            // If engine move failed to parse or was empty, pick first legal move
            if (finalMove == chess::Move::NO_MOVE) {
                chess::Movelist legal;
                chess::movegen::legalmoves(legal, board);
                if (!legal.empty()) finalMove = legal[0];
            }

            if (finalMove != chess::Move::NO_MOVE) {
                std::string san = chess::uci::moveToSan(board, finalMove);
                lastFrom = finalMove.from();
                lastTo = finalMove.to();
                board.makeMove(finalMove);
                history.push_back(finalMove);
                sanHistory.push_back(san);
                currentEval = engineMove;
                statusMessage = "Stockfish played: \033[1;32m" + san + "\033[0m (" + chess::uci::moveToUci(finalMove) + ")";
            }
            continue;
        }


        // Wait for player input
        InputEvent ev = readInputEvent();
        if (ev.type == KEY_NONE) continue;

        // Handle Quit
        if (ev.type == KEY_CHAR && (ev.ch == 'q' || ev.ch == 'Q')) {
            break;
        }
        if (ev.type == KEY_ESC) {
            if (selectedSquare != -1) {
                selectedSquare = -1;
                legalFromSelected.clear();
                statusMessage = "Deselected.";
                continue;
            } else {
                break;
            }
        }

        // Handle Help
        if (ev.type == KEY_CHAR && (ev.ch == 'h' || ev.ch == 'H' || ev.ch == '?')) {
            showHelp = true;
            continue;
        }

        // Handle Flip
        if (ev.type == KEY_CHAR && (ev.ch == 'f' || ev.ch == 'F')) {
            flipped = !flipped;
            statusMessage = flipped ? "Flipped board to Black perspective." : "Flipped board to White perspective.";
            continue;
        }

        // Handle Undo
        if (ev.type == KEY_CHAR && (ev.ch == 'u' || ev.ch == 'U')) {
            if (!history.empty()) {
                int undoSteps = (sfAvailable && playerColor != 3 && history.size() >= 2) ? 2 : 1;
                for (int i = 0; i < undoSteps && !history.empty(); ++i) {
                    board.unmakeMove(history.back());
                    history.pop_back();
                    if (!sanHistory.empty()) sanHistory.pop_back();
                }
                selectedSquare = -1;
                legalFromSelected.clear();
                if (!history.empty()) {
                    lastFrom = history.back().from();
                    lastTo = history.back().to();
                } else {
                    lastFrom = chess::Square::underlying::NO_SQ;
                    lastTo = chess::Square::underlying::NO_SQ;
                }
                statusMessage = "Move undone.";
            } else {
                statusMessage = "No moves to undo.";
            }
            continue;
        }

        // Handle Restart / New Game
        if (ev.type == KEY_CHAR && (ev.ch == 'r' || ev.ch == 'R' || ev.ch == 'n' || ev.ch == 'N')) {
            board = chess::Board();
            history.clear();
            sanHistory.clear();
            selectedSquare = -1;
            legalFromSelected.clear();
            lastFrom = chess::Square::underlying::NO_SQ;
            lastTo = chess::Square::underlying::NO_SQ;
            currentEval = EvalInfo();
            statusMessage = "Started new game.";
            continue;
        }

        // Handle Eval request
        if (ev.type == KEY_CHAR && (ev.ch == 'e' || ev.ch == 'E')) {
            if (sfAvailable) {
                statusMessage = "\033[1;33mEvaluating position...\033[0m";
                std::cout << "\033[H\033[" << (termRows - 1) << ";1H \033[1mStatus:\033[0m " << statusMessage << std::flush;
                currentEval = queryStockfishEval(stockfish, board, 1200);
                statusMessage = "Evaluated! Best move: \033[1;36m" + currentEval.bestMoveStr + "\033[0m";
            } else {
                statusMessage = "Stockfish is offline. Install stockfish for evaluation.";
            }
            continue;
        }

        // Handle Command / SAN move prompt ('/')
        if (ev.type == KEY_CHAR && (ev.ch == '/' || ev.ch == ':')) {
            // Restore normal cursor for typing
            std::cout << "\033[?25h";
            std::cout << "\033[" << termRows << ";1H\033[2K \033[1;36mSAN/Command > \033[0m" << std::flush;

            // Switch to canonical reading for single line
            struct termios textTermios = s_origTermios;
            tcsetattr(STDIN_FILENO, TCSANOW, &textTermios);

            std::string cmd;
            std::getline(std::cin, cmd);

            // Re-enable raw mode
            struct termios raw = s_origTermios;
            raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
            raw.c_oflag &= ~(OPOST);
            raw.c_cflag |= (CS8);
            raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
            raw.c_cc[VMIN] = 0;
            raw.c_cc[VTIME] = 1;
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
            std::cout << "\033[?25l";

            while (!cmd.empty() && (cmd.back() == '\r' || cmd.back() == ' ')) cmd.pop_back();
            while (!cmd.empty() && cmd.front() == ' ') cmd.erase(cmd.begin());

            if (!cmd.empty()) {
                if (cmd == "quit" || cmd == "q") break;
                if (cmd == "help" || cmd == "?") { showHelp = true; continue; }
                if (cmd == "flip") { flipped = !flipped; continue; }
                if (cmd == "undo") {
                    if (!history.empty()) {
                        board.unmakeMove(history.back());
                        history.pop_back();
                        if (!sanHistory.empty()) sanHistory.pop_back();
                    }
                    continue;
                }

                // Try parse SAN or UCI move
                chess::Move cand = chess::Move::NO_MOVE;
                try {
                    cand = chess::uci::parseSan(board, cmd);
                } catch (...) {
                    try { cand = chess::uci::uciToMove(board, cmd); } catch (...) {}
                }

                if (cand != chess::Move::NO_MOVE) {
                    chess::Movelist legal;
                    chess::movegen::legalmoves(legal, board);
                    bool ok = false;
                    for (const auto &m : legal) {
                        if (m == cand) { ok = true; break; }
                    }
                    if (ok) {
                        std::string san = chess::uci::moveToSan(board, cand);
                        lastFrom = cand.from();
                        lastTo = cand.to();
                        board.makeMove(cand);
                        history.push_back(cand);
                        sanHistory.push_back(san);
                        selectedSquare = -1;
                        legalFromSelected.clear();
                        statusMessage = "Played \033[1;32m" + san + "\033[0m";
                    } else {
                        statusMessage = "\033[31mMove '" + cmd + "' is illegal in this position.\033[0m";
                    }
                } else {
                    statusMessage = "\033[31mUnrecognized move or command: '" + cmd + "'\033[0m";
                }
            }
            continue;
        }

        // Handle Arrow navigation
        if (ev.type == KEY_UP || (ev.type == KEY_CHAR && (ev.ch == 'w' || ev.ch == 'k'))) {
            if (flipped) {
                if (cursorRank > 0) cursorRank--;
            } else {
                if (cursorRank < 7) cursorRank++;
            }
            continue;
        }
        if (ev.type == KEY_DOWN || (ev.type == KEY_CHAR && (ev.ch == 's' || ev.ch == 'j'))) {
            if (flipped) {
                if (cursorRank < 7) cursorRank++;
            } else {
                if (cursorRank > 0) cursorRank--;
            }
            continue;
        }
        if (ev.type == KEY_LEFT || (ev.type == KEY_CHAR && (ev.ch == 'a' || ev.ch == 'h'))) {
            if (flipped) {
                if (cursorFile < 7) cursorFile++;
            } else {
                if (cursorFile > 0) cursorFile--;
            }
            continue;
        }
        if (ev.type == KEY_RIGHT || (ev.type == KEY_CHAR && (ev.ch == 'd' || ev.ch == 'l'))) {
            if (flipped) {
                if (cursorFile > 0) cursorFile--;
            } else {
                if (cursorFile < 7) cursorFile++;
            }
            continue;
        }

        // Determine target square from mouse or keyboard selection
        int clickedSquare = -1;
        if (ev.type == KEY_MOUSE_CLICK) {
            clickedSquare = getSquareAtScreen(ev.mouseX, ev.mouseY);
            if (clickedSquare >= 0) {
                cursorFile = clickedSquare % 8;
                cursorRank = clickedSquare / 8;
            }
        } else if (ev.type == KEY_ENTER || ev.type == KEY_SPACE) {
            clickedSquare = cursorRank * 8 + cursorFile;
        }

        if (clickedSquare >= 0 && clickedSquare < 64) {
            chess::Piece clickedPiece = board.at(chess::Square(clickedSquare));

            if (selectedSquare == -1) {
                // First click: select piece if friendly
                if (clickedPiece != chess::Piece::NONE && clickedPiece.color() == board.sideToMove()) {
                    selectedSquare = clickedSquare;
                    updateLegalMovesForSelected();
                    statusMessage = "Selected " + std::string(1, 'a' + (clickedSquare % 8)) + std::to_string((clickedSquare / 8) + 1) +
                                    " (" + std::to_string(legalFromSelected.size()) + " legal moves).";
                } else {
                    statusMessage = "Select one of your pieces to move.";
                }
            } else {
                // Second click: either make move or change selection
                if (clickedSquare == selectedSquare) {
                    selectedSquare = -1;
                    legalFromSelected.clear();
                    statusMessage = "Deselected.";
                } else {
                    // Check if clicked square is a legal destination
                    chess::Move matchingMove = chess::Move::NO_MOVE;
                    for (const auto &m : legalFromSelected) {
                        if (m.to() == chess::Square(clickedSquare)) {
                            matchingMove = m;
                            break;
                        }
                    }

                    if (matchingMove != chess::Move::NO_MOVE) {
                        // Handle promotion if pawn reaches promotion rank
                        chess::Piece selectedPiece = board.at(chess::Square(selectedSquare));
                        if (selectedPiece.type() == chess::PieceType::PAWN &&
                            ((clickedSquare / 8 == 7 && selectedPiece.color() == chess::Color::WHITE) ||
                             (clickedSquare / 8 == 0 && selectedPiece.color() == chess::Color::BLACK))) {

                            // Default promote to Queen
                            matchingMove = chess::Move::make<chess::Move::PROMOTION>(
                                chess::Square(selectedSquare), chess::Square(clickedSquare), chess::PieceType::QUEEN
                            );
                        }

                        std::string san = chess::uci::moveToSan(board, matchingMove);
                        lastFrom = matchingMove.from();
                        lastTo = matchingMove.to();
                        board.makeMove(matchingMove);
                        history.push_back(matchingMove);
                        sanHistory.push_back(san);

                        selectedSquare = -1;
                        legalFromSelected.clear();
                        statusMessage = "Played \033[1;32m" + san + "\033[0m";
                    } else if (clickedPiece != chess::Piece::NONE && clickedPiece.color() == board.sideToMove()) {
                        // Change piece selection to new friendly piece
                        selectedSquare = clickedSquare;
                        updateLegalMovesForSelected();
                        statusMessage = "Selected " + std::string(1, 'a' + (clickedSquare % 8)) + std::to_string((clickedSquare / 8) + 1) +
                                        " (" + std::to_string(legalFromSelected.size()) + " legal moves).";
                    } else {
                        // Invalid move
                        selectedSquare = -1;
                        legalFromSelected.clear();
                        statusMessage = "\033[31mIllegal square for selected piece.\033[0m";
                    }
                }
            }
        }
    }

    disableRawMode();

    if (stockfish.state() == QProcess::Running) {
        stockfish.write("quit\n");
        stockfish.waitForFinished(1000);
    }

    std::cout << "\nThanks for playing Qt6Chess!\n";
    return 0;
}

} // namespace TerminalChess
