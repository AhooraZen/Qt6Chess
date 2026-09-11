#include "TerminalChess.h"
#include "chess.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QThread>

namespace TerminalChess {

static void printBanner()
{
    std::cout << "\033[1;36m";
    std::cout << "╔═════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                     Qt6Chess - Terminal TUI                     ║\n";
    std::cout << "║            Powered by Stockfish 17.1 & chess.hpp                ║\n";
    std::cout << "╚═════════════════════════════════════════════════════════════════╝\n";
    std::cout << "\033[0m";
    std::cout << "Commands: [move (e.g. e4, Nf3, e2e4)] | eval | undo | fen | help | quit\n";
}

static void printBoard(const chess::Board &board, bool flipped = false)
{
    std::cout << "\n       a   b   c   d   e   f   g   h\n";
    std::cout << "     ┌───┬───┬───┬───┬───┬───┬───┬───┐\n";

    for (int r = 7; r >= 0; --r) {
        int rank = flipped ? (7 - r) : r;
        std::cout << "   " << (rank + 1) << " │";
        for (int c = 0; c < 8; ++c) {
            int file = flipped ? (7 - c) : c;
            int sqIndex = rank * 8 + file;
            bool isLight = ((file + rank) % 2 != 0);

            chess::Piece p = board.at(chess::Square(sqIndex));
            std::string pieceGlyph = " ";
            std::string colorCode;

            if (p != chess::Piece::NONE) {
                if (p.color() == chess::Color::WHITE) {
                    colorCode = "\033[38;5;231;1m"; // Bright White
                    if (p.type() == chess::PieceType::PAWN) pieceGlyph = "♙";
                    else if (p.type() == chess::PieceType::KNIGHT) pieceGlyph = "♘";
                    else if (p.type() == chess::PieceType::BISHOP) pieceGlyph = "♗";
                    else if (p.type() == chess::PieceType::ROOK) pieceGlyph = "♖";
                    else if (p.type() == chess::PieceType::QUEEN) pieceGlyph = "♕";
                    else if (p.type() == chess::PieceType::KING) pieceGlyph = "♔";
                } else {
                    colorCode = "\033[38;5;208;1m"; // Warm Orange/Gold for Black pieces
                    if (p.type() == chess::PieceType::PAWN) pieceGlyph = "♟";
                    else if (p.type() == chess::PieceType::KNIGHT) pieceGlyph = "♞";
                    else if (p.type() == chess::PieceType::BISHOP) pieceGlyph = "♝";
                    else if (p.type() == chess::PieceType::ROOK) pieceGlyph = "♜";
                    else if (p.type() == chess::PieceType::QUEEN) pieceGlyph = "♛";
                    else if (p.type() == chess::PieceType::KING) pieceGlyph = "♚";
                }
            }

            // Alternating square backgrounds
            std::string bgCode = isLight ? "\033[48;5;239m" : "\033[48;5;235m";
            std::cout << bgCode << " " << colorCode << pieceGlyph << "\033[0m" << bgCode << " \033[0m│";
        }
        std::cout << " " << (rank + 1) << "\n";
        if (r > 0) {
            std::cout << "     ├───┼───┼───┼───┼───┼───┼───┼───┤\n";
        }
    }
    std::cout << "     └───┴───┴───┴───┴───┴───┴───┴───┘\n";
    std::cout << "       a   b   c   d   e   f   g   h\n\n";
}

static QString queryStockfishBestMove(QProcess &engine, const QString &fen, int moveTimeMs = 1200)
{
    engine.write(QString("position fen %1\n").arg(fen).toUtf8());
    engine.write(QString("go movetime %1\n").arg(moveTimeMs).toUtf8());

    QString bestMove;
    while (engine.waitForReadyRead(3000)) {
        while (engine.canReadLine()) {
            QString line = QString::fromUtf8(engine.readLine()).trimmed();
            if (line.startsWith("bestmove ")) {
                QStringList parts = line.split(' ', Qt::SkipEmptyParts);
                if (parts.size() >= 2) {
                    bestMove = parts[1];
                }
                return bestMove;
            }
        }
    }
    return bestMove;
}

int run(int elo, int playerColor)
{
    printBanner();

    chess::Board board;
    std::vector<chess::Move> history;
    bool flipped = (playerColor == 2); // Black perspective

    // Start Stockfish
    QProcess stockfish;
    stockfish.start(QStringLiteral("stockfish"));
    if (!stockfish.waitForStarted(2000)) {
        std::cout << "\033[33mNote: Stockfish binary not found in PATH; AI play disabled, local PvP mode active.\033[0m\n";
    } else {
        stockfish.write("uci\n");
        stockfish.write(QString("setoption name UCI_LimitStrength value true\n").toUtf8());
        stockfish.write(QString("setoption name UCI_Elo value %1\n").arg(elo).toUtf8());
        stockfish.write("isready\n");
        stockfish.waitForReadyRead(1000);
        std::cout << "\033[32mStockfish 17.1 connected! Difficulty Elo: " << elo << "\033[0m\n";
    }

    printBoard(board, flipped);

    while (true) {
        auto [reason, result] = board.isGameOver();
        if (result != chess::GameResult::NONE) {
            std::cout << "\033[1;31mGame Over: ";
            if (reason == chess::GameResultReason::CHECKMATE) {
                std::cout << ((board.sideToMove() == chess::Color::WHITE) ? "Black" : "White") << " wins by Checkmate!\033[0m\n";
            } else {
                std::cout << "Draw!\033[0m\n";
            }
            break;
        }

        bool isWhiteTurn = (board.sideToMove() == chess::Color::WHITE);
        bool isHumanTurn = (playerColor == 3) || // 3 = PvP
                           (playerColor == 1 && isWhiteTurn) ||
                           (playerColor == 2 && !isWhiteTurn);

        if (!isHumanTurn && stockfish.state() == QProcess::Running) {
            std::cout << "\033[1;33mStockfish is thinking...\033[0m" << std::flush;
            QString fen = QString::fromStdString(board.getFen());
            QString bestMoveStr = queryStockfishBestMove(stockfish, fen);

            if (bestMoveStr.isEmpty()) {
                std::cout << "\nEngine returned no move.\n";
                break;
            }

            try {
                chess::Move m = chess::uci::uciToMove(board, bestMoveStr.toStdString());
                std::string san = chess::uci::moveToSan(board, m);
                board.makeMove(m);
                history.push_back(m);
                std::cout << "\rStockfish played: \033[1;32m" << san << " (" << bestMoveStr.toStdString() << ")\033[0m\n";
                printBoard(board, flipped);
                continue;
            } catch (...) {
                std::cout << "\nFailed to parse engine move: " << bestMoveStr.toStdString() << "\n";
                break;
            }
        }

        // Prompt human move
        std::cout << "\033[1;37m" << (isWhiteTurn ? "White" : "Black") << " to move\033[0m > " << std::flush;
        std::string input;
        if (!std::getline(std::cin, input)) break;

        // Trim whitespace
        while (!input.empty() && (input.back() == '\r' || input.back() == ' ' || input.back() == '\n')) {
            input.pop_back();
        }
        while (!input.empty() && input.front() == ' ') {
            input.erase(input.begin());
        }

        if (input.empty()) continue;

        if (input == "quit" || input == "exit" || input == "q") {
            std::cout << "Goodbye!\n";
            break;
        }

        if (input == "help") {
            std::cout << "Enter standard move notation (e.g., 'e4', 'Nf3', 'e2e4', 'O-O')\n";
            std::cout << "Commands: eval, undo, fen, moves, flip, quit\n";
            continue;
        }

        if (input == "flip") {
            flipped = !flipped;
            printBoard(board, flipped);
            continue;
        }

        if (input == "fen") {
            std::cout << "FEN: " << board.getFen() << "\n";
            continue;
        }

        if (input == "undo") {
            if (!history.empty()) {
                int steps = (stockfish.state() == QProcess::Running && playerColor != 3 && history.size() >= 2) ? 2 : 1;
                for (int i = 0; i < steps && !history.empty(); ++i) {
                    board.unmakeMove(history.back());
                    history.pop_back();
                }
                printBoard(board, flipped);
            } else {
                std::cout << "No moves to undo.\n";
            }
            continue;
        }

        if (input == "moves") {
            chess::Movelist legal;
            chess::movegen::legalmoves(legal, board);
            std::cout << "Legal moves (" << legal.size() << "): ";
            for (const auto &m : legal) {
                std::cout << chess::uci::moveToSan(board, m) << " ";
            }
            std::cout << "\n";
            continue;
        }

        // Try parsing move: First as SAN, then as UCI
        chess::Move candidateMove = chess::Move::NO_MOVE;
        try {
            candidateMove = chess::uci::parseSan(board, input);
        } catch (...) {
            try {
                candidateMove = chess::uci::uciToMove(board, input);
            } catch (...) {
                candidateMove = chess::Move::NO_MOVE;
            }
        }

        if (candidateMove == chess::Move::NO_MOVE) {
            std::cout << "\033[31mIllegal or unrecognized move: '" << input << "'. Type 'moves' or 'help'.\033[0m\n";
            continue;
        }

        // Verify legality
        chess::Movelist legal;
        chess::movegen::legalmoves(legal, board);
        bool isLegal = false;
        for (const auto &m : legal) {
            if (m == candidateMove) {
                isLegal = true;
                break;
            }
        }

        if (!isLegal) {
            std::cout << "\033[31mMove '" << input << "' is not legal in this position.\033[0m\n";
            continue;
        }

        std::string san = chess::uci::moveToSan(board, candidateMove);
        board.makeMove(candidateMove);
        history.push_back(candidateMove);

        std::cout << "Played: \033[1;32m" << san << "\033[0m\n";
        printBoard(board, flipped);
    }

    if (stockfish.state() == QProcess::Running) {
        stockfish.write("quit\n");
        stockfish.waitForFinished(1000);
    }

    return 0;
}

} // namespace TerminalChess
