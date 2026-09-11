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

#if defined(_WIN32)

namespace TerminalChess {
int run(int /*elo*/, int /*playerColor*/)
{
    std::cout << "Terminal TUI mode is currently supported on Linux / Unix terminals.\n"
              << "Please launch Qt6Chess without --cli to use the GUI." << std::endl;
    return 0;
}
} // namespace TerminalChess

#else

#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <sys/types.h>
#include <wchar.h>

#include <QProcess>
#include <QString>
#include <QStringList>
#include <QStandardPaths>
#include <QFile>

namespace TerminalChess {

// Terminal state management
static struct termios s_origTermios;
static bool s_rawModeActive = false;
static pid_t s_childEnginePid = 0;

static void disableRawMode()
{
    if (s_rawModeActive) {
        const char resetSeq[] = "\033[?1006l\033[?1000l\033[?25h\033[?1049l\033[0m";
        (void)::write(STDOUT_FILENO, resetSeq, sizeof(resetSeq) - 1);
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &s_origTermios);
        s_rawModeActive = false;
    }
}

static void setRawMode()
{
    struct termios raw = s_origTermios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag |= (OPOST | ONLCR); // Keep OPOST so \n translates to \r\n without staircasing
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1; // 100ms read timeout

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    s_rawModeActive = true;
}

static void enableRawMode()
{
    if (tcgetattr(STDIN_FILENO, &s_origTermios) == -1) return;

    setRawMode();

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
    if (s_childEnginePid > 0) {
        ::kill(s_childEnginePid, SIGTERM);
    }
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
    sigaction(SIGHUP, &sa, nullptr);
    sigaction(SIGQUIT, &sa, nullptr);

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
    int pieceGlyphWidth = (wcwidth(0x2654) == 2) ? 2 : 1;

    // Connect Stockfish
    QString sfPath = locateStockfish();
    QProcess stockfish;
    bool sfAvailable = false;

    if (!sfPath.isEmpty()) {
        stockfish.start(sfPath);
        if (stockfish.waitForStarted(3000)) {
            s_childEnginePid = static_cast<pid_t>(stockfish.processId());
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
        if (relX < 0 || relX >= 48 || relY < 0 || relY >= 24) return -1;
        int displayFile = relX / 6;
        int displayRank = relY / 3;
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
        out.reserve(8192);
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
        out += "\033[1;36m ♞ Qt6Chess Terminal TUI \033[0m\033[90m│ C++20 SceneGraph & ANSI SGR Edition\033[0m\033[K\r\n";
        out += "\033[90m" + repeatStr("─", std::min(termCols, 98)) + "\033[0m\033[K\r\n";

        // If Help Modal is open
        if (showHelp) {
            out += "\033[K\r\n";
            out += "\033[1;33m  ┌───────────────────────── Qt6Chess TUI Help ─────────────────────────┐\033[0m\033[K\r\n";
            out += "  │                                                                     │\033[K\r\n";
            out += "  │  \033[1;32mMouse Controls:\033[0m                                                    │\033[K\r\n";
            out += "  │    • \033[1mLeft Click on Piece\033[0m  : Select piece and view legal targets        │\033[K\r\n";
            out += "  │    • \033[1mLeft Click on Target\033[0m : Make the chess move                       │\033[K\r\n";
            out += "  │    • \033[1mLeft Click Elsewhere\033[0m: Deselect / Cancel move                     │\033[K\r\n";
            out += "  │                                                                     │\033[K\r\n";
            out += "  │  \033[1;32mKeyboard Controls:\033[0m                                                 │\033[K\r\n";
            out += "  │    • \033[1mArrow Keys / WASD / HJKL\033[0m : Move cyan board cursor                │\033[K\r\n";
            out += "  │    • \033[1mSpace / Enter\033[0m            : Select piece or execute target move   │\033[K\r\n";
            out += "  │    • \033[1mu\033[0m                        : Undo last move(s)                     │\033[K\r\n";
            out += "  │    • \033[1mf\033[0m                        : Flip board orientation (White/Black)  │\033[K\r\n";
            out += "  │    • \033[1me\033[0m                        : Query Stockfish evaluation & bestmove │\033[K\r\n";
            out += "  │    • \033[1m/\033[0m                        : Enter SAN move directly (e.g. 'Nf3')  │\033[K\r\n";
            out += "  │    • \033[1mp\033[0m                        : Toggle 1-col / 2-col piece width      │\033[K\r\n";
            out += "  │    • \033[1mr / n\033[0m                    : Restart / New Game                    │\033[K\r\n";
            out += "  │    • \033[1mh / ?\033[0m                    : Toggle this Help popup                │\033[K\r\n";
            out += "  │    • \033[1mq / Esc\033[0m                  : Quit Qt6Chess                         │\033[K\r\n";
            out += "  │                                                                     │\033[K\r\n";
            out += "  │  \033[1;32mPacman / Arch Note:\033[0m                                                │\033[K\r\n";
            out += "  │    Stockfish is optional! If installed, AI play and live eval       │\033[K\r\n";
            out += "  │    activate automatically. Otherwise, Pass & Play mode works!       │\033[K\r\n";
            out += "  │                                                                     │\033[K\r\n";
            out += "\033[1;33m  └────────────────────── [Press any key to close] ─────────────────────┘\033[0m\033[K\r\n";
            out += "\033[J";
            std::cout << out << std::flush;

            InputEvent helpEv = readInputEvent();
            if (helpEv.type != KEY_NONE) {
                showHelp = false;
            }
            continue;
        }

        // Prepare side panel lines (exactly 24 lines, 1:1 row match with 24 board lines)
        std::vector<std::string> sideLines(24, "");

        // 0: Match Info header
        sideLines[0] = "\033[1;36mMatch Info\033[0m";

        // 1: Stockfish 17.1 (Elo) or Two-Player
        if (sfAvailable) {
            sideLines[1] = "  \033[32m●\033[0m \033[1mStockfish 17.1\033[0m \033[90m(Elo " + std::to_string(elo) + ")\033[0m";
        } else {
            sideLines[1] = "  \033[33m○\033[0m \033[1mTwo-Player Mode\033[0m \033[90m(Engine offline)\033[0m";
        }

        // 2: Turn indicator (▶ White/Black to move + Check alert if inCheck)
        bool isWhiteTurn = (board.sideToMove() == chess::Color::WHITE);
        std::string turnBadge = isWhiteTurn ? "  \033[1;37m▶ White to move\033[0m" : "  \033[1;33m▶ Black to move\033[0m";
        if (board.inCheck()) {
            turnBadge += " \033[1;31m[CHECK!]\033[0m";
        }
        sideLines[2] = turnBadge;

        // 3: Divider
        sideLines[3] = "\033[90m──────────────────────────────────────\033[0m";

        // 4: Position Evaluation header
        sideLines[4] = "\033[1;36mPosition Evaluation\033[0m";

        // 5: Visual eval bar [████████░░░░░░░░] +0.2 (d16)
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
            std::string bar = "  \033[37m[";
            for (int b = 0; b < 16; ++b) {
                if (b < filledBlocks) bar += "\033[32m█";
                else bar += "\033[90m░";
            }
            bar += "\033[37m]\033[0m " + evalStr + " \033[90m(d" + std::to_string(currentEval.depth) + ")\033[0m";
            sideLines[5] = bar;
        } else {
            sideLines[5] = "  \033[90m[░░░░░░░░░░░░░░░░] (Press 'e' for eval)\033[0m";
        }

        // 6: Best move recommendation
        if (!currentEval.bestMoveStr.empty()) {
            sideLines[6] = "  \033[90mBest move: \033[1;36m" + currentEval.bestMoveStr + "\033[0m";
        } else {
            sideLines[6] = "  \033[90mBest move: -\033[0m";
        }

        // 7: Divider
        sideLines[7] = "\033[90m──────────────────────────────────────\033[0m";

        // 8: Captured Material header
        sideLines[8] = "\033[1;36mCaptured Material\033[0m";

        // 9: White captured list
        CapturedCount capt = computeCaptured(board);
        std::string capWhite = "  \033[1mWhite:\033[0m ";
        int countW = 0;
        for (int i = 0; i < capt.blackQueens; ++i) { capWhite += "♛"; countW++; }
        for (int i = 0; i < capt.blackRooks; ++i) { capWhite += "♜"; countW++; }
        for (int i = 0; i < capt.blackBishops; ++i) { capWhite += "♝"; countW++; }
        for (int i = 0; i < capt.blackKnights; ++i) { capWhite += "♞"; countW++; }
        for (int i = 0; i < capt.blackPawns; ++i) { capWhite += "♟"; countW++; }
        if (countW == 0) capWhite += "\033[90m(none)\033[0m";
        sideLines[9] = capWhite;

        // 10: Black captured list
        std::string capBlack = "  \033[1mBlack:\033[0m ";
        int countB = 0;
        for (int i = 0; i < capt.whiteQueens; ++i) { capBlack += "♕"; countB++; }
        for (int i = 0; i < capt.whiteRooks; ++i) { capBlack += "♖"; countB++; }
        for (int i = 0; i < capt.whiteBishops; ++i) { capBlack += "♗"; countB++; }
        for (int i = 0; i < capt.whiteKnights; ++i) { capBlack += "♘"; countB++; }
        for (int i = 0; i < capt.whitePawns; ++i) { capBlack += "♙"; countB++; }
        if (countB == 0) capBlack += "\033[90m(none)\033[0m";
        sideLines[10] = capBlack;

        // 11: Material diff score
        if (capt.materialDiff > 0) {
            sideLines[11] = "  \033[90mAdvantage: \033[1;32m+" + std::to_string(capt.materialDiff) + " White\033[0m";
        } else if (capt.materialDiff < 0) {
            sideLines[11] = "  \033[90mAdvantage: \033[1;33m+" + std::to_string(-capt.materialDiff) + " Black\033[0m";
        } else {
            sideLines[11] = "  \033[90mAdvantage: \033[37mEven (0)\033[0m";
        }

        // 12: Divider
        sideLines[12] = "\033[90m──────────────────────────────────────\033[0m";

        // 13: Move History header
        sideLines[13] = "\033[1;36mMove History\033[0m";

        // 14..18: Last 5 turns formatted (e.g. 1. e4    e5)
        int totalPly = static_cast<int>(sanHistory.size());
        int totalTurns = (totalPly + 1) / 2;
        int startTurn = std::max(0, totalTurns - 5);
        if (totalTurns == 0) {
            sideLines[14] = "  \033[90m(No moves yet)\033[0m";
            sideLines[15] = "";
            sideLines[16] = "";
            sideLines[17] = "";
            sideLines[18] = "";
        } else {
            for (int t = 0; t < 5; ++t) {
                int turnNum = startTurn + t;
                if (turnNum < totalTurns) {
                    int wIdx = turnNum * 2;
                    int bIdx = turnNum * 2 + 1;
                    std::string turnStr = "  \033[90m" + std::to_string(turnNum + 1) + ".\033[0m " + sanHistory[wIdx];
                    int currentLen = (turnNum + 1 >= 10 ? 2 : 1) + 2 + static_cast<int>(sanHistory[wIdx].size());
                    while (currentLen < 12) {
                        turnStr += " ";
                        currentLen++;
                    }
                    if (bIdx < totalPly) {
                        turnStr += sanHistory[bIdx];
                    }
                    sideLines[14 + t] = turnStr;
                } else {
                    sideLines[14 + t] = "";
                }
            }
        }

        // 19: Divider
        sideLines[19] = "\033[90m──────────────────────────────────────\033[0m";

        // 20: Quick Controls header
        sideLines[20] = "\033[1;36mQuick Controls\033[0m";

        // 21: Mouse / Keyboard movement info
        sideLines[21] = "  \033[90m[Click / Enter]\033[0m Move  \033[90m[Arrows/WASD]\033[0m Aim";

        // 22: [u] Undo  [f] Flip  [e] Eval
        sideLines[22] = "  \033[1;33m[u]\033[0m Undo   \033[1;33m[f]\033[0m Flip   \033[1;33m[e]\033[0m Eval";

        // 23: [/] SAN   [r] Reset [q] Quit
        sideLines[23] = "  \033[1;33m[/]\033[0m SAN    \033[1;33m[r]\033[0m Reset  \033[1;33m[q]\033[0m Quit";

        // File headers (a - h) top (strictly 6 display columns per file, 5 spaces margin)
        out += "     ";
        for (int c = 0; c < 8; ++c) {
            int file = flipped ? (7 - c) : c;
            char fileChar = 'a' + file;
            out += "  \033[90m" + std::string(1, fileChar) + "\033[0m   ";
        }
        out += "     \033[90m│\033[0m\033[K\r\n";

        // Board rendering: 8 ranks, 3 lines per rank = 24 lines
        for (int displayRank = 0; displayRank < 8; ++displayRank) {
            int rank = flipped ? displayRank : (7 - displayRank);

            for (int subLine = 0; subLine < 3; ++subLine) {
                // Left rank coordinate (5 display columns)
                if (subLine == 1) {
                    out += "  \033[90m" + std::to_string(rank + 1) + "\033[0m  ";
                } else {
                    out += "     ";
                }

                // 8 squares across (6 display columns each = 48 display columns)
                for (int displayFile = 0; displayFile < 8; ++displayFile) {
                    int file = flipped ? (7 - displayFile) : displayFile;
                    int sqIndex = rank * 8 + file;
                    bool isLight = ((file + rank) % 2 != 0);

                    bool isSelected = (sqIndex == selectedSquare);
                    bool isCursor = (file == cursorFile && rank == cursorRank);
                    bool isLastMove = (lastFrom != chess::Square::underlying::NO_SQ &&
                                      (chess::Square(sqIndex) == lastFrom || chess::Square(sqIndex) == lastTo));
                    bool isInCheckKing = (board.inCheck() && chess::Square(sqIndex) == board.kingSq(board.sideToMove()));

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
                        bg = "\033[48;2;246;246;105m"; // Amber Gold
                    } else if (isInCheckKing) {
                        bg = "\033[48;2;230;60;60m";   // Scarlet Red
                    } else if (isCaptureTarget) {
                        bg = "\033[48;2;215;75;75m";   // Crimson
                    } else if (isLegalTarget) {
                        bg = "\033[48;2;135;160;105m"; // Soft Green
                    } else if (isLastMove) {
                        bg = "\033[48;2;205;210;106m"; // Subtle Amber
                    } else if (isLight) {
                        bg = "\033[48;2;240;217;181m"; // Warm Cream
                    } else {
                        bg = "\033[48;2;181;136;99m";  // Rich Walnut
                    }

                    // Piece glyph and color
                    std::string pieceGlyph = " ";
                    std::string fg = "\033[38;2;255;255;255;1m"; // Porcelain White
                    if (piece != chess::Piece::NONE) {
                        if (piece.color() == chess::Color::WHITE) {
                            fg = "\033[38;2;255;255;255;1m"; // Porcelain White
                            if (piece.type() == chess::PieceType::PAWN) pieceGlyph = "♙";
                            else if (piece.type() == chess::PieceType::KNIGHT) pieceGlyph = "♘";
                            else if (piece.type() == chess::PieceType::BISHOP) pieceGlyph = "♗";
                            else if (piece.type() == chess::PieceType::ROOK) pieceGlyph = "♖";
                            else if (piece.type() == chess::PieceType::QUEEN) pieceGlyph = "♕";
                            else if (piece.type() == chess::PieceType::KING) pieceGlyph = "♔";
                        } else {
                            fg = "\033[38;2;25;25;25;1m"; // Obsidian Black
                            if (piece.type() == chess::PieceType::PAWN) pieceGlyph = "♟";
                            else if (piece.type() == chess::PieceType::KNIGHT) pieceGlyph = "♞";
                            else if (piece.type() == chess::PieceType::BISHOP) pieceGlyph = "♝";
                            else if (piece.type() == chess::PieceType::ROOK) pieceGlyph = "♜";
                            else if (piece.type() == chess::PieceType::QUEEN) pieceGlyph = "♛";
                            else if (piece.type() == chess::PieceType::KING) pieceGlyph = "♚";
                        }
                    }

                    // Render square (strictly 6 display columns)
                    if (isCursor) {
                        if (subLine == 0) {
                            out += bg + "\033[1;36m┌────┐\033[0m";
                        } else if (subLine == 1) {
                            out += bg + "\033[1;36m│\033[0m";
                            if (piece == chess::Piece::NONE) {
                                if (isLegalTarget) {
                                    if (pieceGlyphWidth == 1) {
                                        out += bg + " \033[38;2;40;40;40;1m•\033[0m" + bg + "  ";
                                    } else {
                                        out += bg + " \033[38;2;40;40;40;1m•\033[0m" + bg + " ";
                                    }
                                } else {
                                    out += bg + "    ";
                                }
                            } else {
                                if (pieceGlyphWidth == 1) {
                                    out += bg + " " + fg + pieceGlyph + "\033[0m" + bg + "  ";
                                } else {
                                    out += bg + " " + fg + pieceGlyph + "\033[0m" + bg + " ";
                                }
                            }
                            out += bg + "\033[1;36m│\033[0m";
                        } else { // subLine == 2
                            out += bg + "\033[1;36m└────┘\033[0m";
                        }
                    } else {
                        if (subLine == 0 || subLine == 2) {
                            out += bg + "      \033[0m";
                        } else { // subLine == 1
                            if (piece == chess::Piece::NONE) {
                                if (isLegalTarget) {
                                    if (pieceGlyphWidth == 1) {
                                        out += bg + "  \033[38;2;40;40;40;1m•\033[0m" + bg + "   \033[0m";
                                    } else {
                                        out += bg + "  \033[38;2;40;40;40;1m•\033[0m" + bg + "  \033[0m";
                                    }
                                } else {
                                    out += bg + "      \033[0m";
                                }
                            } else {
                                if (pieceGlyphWidth == 1) {
                                    out += bg + "  " + fg + pieceGlyph + "\033[0m" + bg + "   \033[0m";
                                } else {
                                    out += bg + "  " + fg + pieceGlyph + "\033[0m" + bg + "  \033[0m";
                                }
                            }
                        }
                    }
                }

                // Right rank coordinate (5 display columns)
                if (subLine == 1) {
                    out += "  \033[90m" + std::to_string(rank + 1) + "\033[0m  ";
                } else {
                    out += "     ";
                }

                // Separator and Side panel (1:1 row match)
                out += "\033[90m│ \033[0m";
                int sideIdx = displayRank * 3 + subLine;
                if (sideIdx < static_cast<int>(sideLines.size())) {
                    out += sideLines[sideIdx];
                }
                out += "\033[K\r\n";
            }
        }

        // File headers (a - h) bottom (strictly 6 display columns per file, 5 spaces margin)
        out += "     ";
        for (int c = 0; c < 8; ++c) {
            int file = flipped ? (7 - c) : c;
            char fileChar = 'a' + file;
            out += "  \033[90m" + std::string(1, fileChar) + "\033[0m   ";
        }
        out += "     \033[90m│\033[0m\033[K\r\n";

        // Status message bar
        out += "\033[90m" + repeatStr("─", std::min(termCols, 98)) + "\033[0m\033[K\r\n";
        out += " \033[1mStatus:\033[0m " + statusMessage + "\033[K\r\n";
        out += " \033[90m[🖱 Click/Enter] Move  [Arrows/WASD] Cursor  [u] Undo  [f] Flip  [e] Eval  [/] SAN  [p] Width  [?] Help  [q] Quit\033[0m\033[K\r\n";
        out += "\033[J";

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
            std::cout << "\033[30;1H \033[1mStatus:\033[0m " << statusMessage << "\033[K" << std::flush;

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

        // Handle Piece Width Toggle
        if (ev.type == KEY_CHAR && (ev.ch == 'p' || ev.ch == 'P')) {
            pieceGlyphWidth = (pieceGlyphWidth == 1) ? 2 : 1;
            statusMessage = (pieceGlyphWidth == 2) ? "Piece width set to 2 columns." : "Piece width set to 1 column.";
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
                std::cout << "\033[30;1H \033[1mStatus:\033[0m " << statusMessage << "\033[K" << std::flush;
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
            std::cout << "\033[30;1H\033[K \033[1;36mSAN/Command > \033[0m" << std::flush;

            // Switch to canonical reading for single line
            struct termios textTermios = s_origTermios;
            tcsetattr(STDIN_FILENO, TCSANOW, &textTermios);

            std::string cmd;
            std::getline(std::cin, cmd);

            // Re-enable raw mode
            setRawMode();
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
    s_childEnginePid = 0;

    std::cout << "\nThanks for playing Qt6Chess!\n";
    return 0;
}

} // namespace TerminalChess
#endif // !_WIN32
