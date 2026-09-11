#include <QTest>
#include "chess.hpp"
#include "ChessBoardModel.h"
#include "ChessClock.h"
#include "MoveHistoryModel.h"

class TestChessCore : public QObject {
    Q_OBJECT

private slots:
    void testInitialBoard();
    void testScholarMate();
    void testCastlingAndEnPassant();
    void testPawnPromotion();
    void testClock();
    void testMoveHistory();
};

void TestChessCore::testInitialBoard()
{
    chess::Board board;
    QCOMPARE(QString::fromStdString(board.getFen()), QStringLiteral("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"));

    chess::Movelist moves;
    chess::movegen::legalmoves(moves, board);
    QCOMPARE(moves.size(), 20); // 16 pawn moves + 4 knight moves

    ChessBoardModel model;
    std::vector<int> targets;
    model.syncWithBoard(board, -1, targets, -1, -1, -1);
    QCOMPARE(model.rowCount(), 64);

    QVariantMap e2Data = model.getSquareData(12); // e2
    QCOMPARE(e2Data["pieceCode"].toString(), QStringLiteral("wP"));
}

void TestChessCore::testScholarMate()
{
    chess::Board board;
    // 1. e4 e5
    board.makeMove(chess::uci::uciToMove(board, "e2e4"));
    board.makeMove(chess::uci::uciToMove(board, "e7e5"));
    // 2. Bc4 Nc6
    board.makeMove(chess::uci::uciToMove(board, "f1c4"));
    board.makeMove(chess::uci::uciToMove(board, "b8c6"));
    // 3. Qh5 Nf6
    board.makeMove(chess::uci::uciToMove(board, "d1h5"));
    board.makeMove(chess::uci::uciToMove(board, "g8f6"));
    // 4. Qxf7#
    board.makeMove(chess::uci::uciToMove(board, "h5f7"));

    auto [reason, result] = board.isGameOver();
    QCOMPARE(reason, chess::GameResultReason::CHECKMATE);
    QCOMPARE(result, chess::GameResult::LOSE); // Side to move (Black) lost
    QVERIFY(board.inCheck());
}

void TestChessCore::testCastlingAndEnPassant()
{
    // Test Kingside Castling
    chess::Board board("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
    chess::Move castleWhite = chess::uci::uciToMove(board, "e1g1");
    QVERIFY(castleWhite != chess::Move::NO_MOVE);
    board.makeMove(castleWhite);
    QCOMPARE(board.at(chess::Square::underlying::SQ_G1).type(), chess::PieceType::KING);
    QCOMPARE(board.at(chess::Square::underlying::SQ_F1).type(), chess::PieceType::ROOK);

    // Test En Passant
    chess::Board epBoard("rnbqkbnr/pp1ppppp/8/2p5/4P3/8/PPPP1PPP/RNBQKBNR w KQkq c6 0 2");
    // Board where en passant can happen
    chess::Board epTest("8/8/8/3Pp3/8/8/8/4K2k w - e6 0 1");
    chess::Move epMove = chess::uci::uciToMove(epTest, "d5e6");
    QVERIFY(epMove != chess::Move::NO_MOVE);
    epTest.makeMove(epMove);
    // Pawn on e5 should be removed
    QCOMPARE(epTest.at(chess::Square::underlying::SQ_E5), chess::Piece::NONE);
    QCOMPARE(epTest.at(chess::Square::underlying::SQ_E6).type(), chess::PieceType::PAWN);
}

void TestChessCore::testPawnPromotion()
{
    chess::Board board("8/4P3/8/8/8/8/8/4K2k w - - 0 1");
    chess::Move promoMove = chess::uci::uciToMove(board, "e7e8q");
    QVERIFY(promoMove != chess::Move::NO_MOVE);
    board.makeMove(promoMove);
    QCOMPARE(board.at(chess::Square::underlying::SQ_E8).type(), chess::PieceType::QUEEN);
}

void TestChessCore::testClock()
{
    ChessClock clock;
    clock.setTimeControl(180, 2);
    QCOMPARE(clock.whiteTimeMs(), 180000);
    QCOMPARE(clock.blackTimeMs(), 180000);

    clock.start(ChessClock::White);
    QVERIFY(clock.isRunning());
    QCOMPARE(clock.activeSide(), static_cast<int>(ChessClock::White));

    clock.switchTurn();
    QCOMPARE(clock.activeSide(), static_cast<int>(ChessClock::Black));
    // White received 2s increment
    QVERIFY(clock.whiteTimeMs() >= 180000);

    clock.pause();
    QVERIFY(!clock.isRunning());
}

void TestChessCore::testMoveHistory()
{
    MoveHistoryModel history;
    QCOMPARE(history.rowCount(), 0);

    history.addMove(true, "e4", "e2e4", "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1");
    QCOMPARE(history.rowCount(), 1);
    QCOMPARE(history.totalPly(), 1);

    history.addMove(false, "e5", "e7e5", "rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2");
    QCOMPARE(history.rowCount(), 1);
    QCOMPARE(history.totalPly(), 2);

    QString pgn = history.exportPgn("Carlsen", "Nakamura", "World Championship", "1/2-1/2");
    QVERIFY(pgn.contains("1. e4 e5"));
    QVERIFY(pgn.contains("[White \"Carlsen\"]"));
}

QTEST_MAIN(TestChessCore)
#include "TestChessCore.moc"
