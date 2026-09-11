#include "GameController.h"
#include <QGuiApplication>
#include <QClipboard>
#include <QDebug>
#include <QRegularExpression>

GameController::GameController(QObject *parent)
    : QObject(parent),
      m_boardModel(new ChessBoardModel(this)),
      m_uci(new UciController(this)),
      m_clock(new ChessClock(this)),
      m_history(new MoveHistoryModel(this)),
      m_soundManager(new SoundManager(this)),
      m_statusText(QStringLiteral("White to move"))
{
    connect(m_uci, &UciController::bestMoveFound, this, &GameController::onAiBestMoveFound);
    connect(m_clock, &ChessClock::timeOut, this, &GameController::onClockTimeOut);

    updateBoardView();
}

QString GameController::activeColor() const
{
    return (m_board.sideToMove() == chess::Color::WHITE) ? QStringLiteral("white") : QStringLiteral("black");
}

QString GameController::currentFen() const
{
    return QString::fromStdString(m_board.getFen());
}

void GameController::setFlipped(bool f)
{
    if (m_flipped != f) {
        m_flipped = f;
        emit flippedChanged();
    }
}

void GameController::flipBoard()
{
    setFlipped(!m_flipped);
}

void GameController::setGameMode(int mode)
{
    if (m_gameMode != mode) {
        m_gameMode = mode;
        emit gameModeChanged();
        newGame(mode, m_playerColor);
    }
}

void GameController::setAiElo(int elo)
{
    if (m_aiElo != elo) {
        m_aiElo = elo;
        m_uci->setElo(elo);
        emit aiSettingsChanged();
    }
}

void GameController::setPlayerColor(int color)
{
    if (m_playerColor != color) {
        m_playerColor = color;
        setFlipped(color == ColorBlack);
        emit aiSettingsChanged();
        newGame(m_gameMode, color);
    }
}

void GameController::newGame(int mode, int color, int timeMin, int incSec)
{
    m_gameMode = mode;
    m_playerColor = color;
    m_flipped = (color == ColorBlack);
    m_selectedSquare = -1;
    m_legalTargets.clear();
    m_lastFrom = -1;
    m_lastTo = -1;
    m_isGameOver = false;
    m_gameResult.clear();
    m_isThinking = false;
    m_isPromotionPending = false;
    m_pendingFrom = -1;
    m_pendingTo = -1;
    m_playedMoves.clear();

    m_board = chess::Board(chess::constants::STARTPOS);

    m_history->clear();

    if (timeMin > 0) {
        m_clock->setTimeControl(timeMin * 60, incSec);
        m_clock->start(ChessClock::White);
    } else {
        m_clock->setUnlimited();
    }

    m_uci->stopAnalysis();
    m_uci->setElo(m_aiElo);

    m_statusText = QStringLiteral("White to move");

    emit selectionChanged();
    emit flippedChanged();
    emit statusChanged();
    emit turnChanged();
    emit gameOverChanged();
    emit thinkingChanged();
    emit promotionPendingChanged();
    emit boardChanged();

    updateBoardView();

    if (m_gameMode == ModeAnalysis) {
        m_uci->setPosition(QStringLiteral("startpos"));
        m_uci->startInfiniteAnalysis();
    } else {
        triggerAiMoveIfNeeded();
    }
}

std::vector<int> GameController::calculateLegalTargets(int fromSq)
{
    std::vector<int> targets;
    if (fromSq < 0 || fromSq >= 64) return targets;

    chess::Movelist moves;
    chess::movegen::legalmoves(moves, m_board);
    for (const auto &m : moves) {
        if (m.from().index() == fromSq) {
            int toIdx = m.to().index();
            if (std::find(targets.begin(), targets.end(), toIdx) == targets.end()) {
                targets.push_back(toIdx);
            }
        }
    }
    return targets;
}

bool GameController::isPawnPromotion(int fromSq, int toSq) const
{
    chess::Piece p = m_board.at(chess::Square(fromSq));
    if (p.type() != chess::PieceType::PAWN) return false;

    int toRank = toSq / 8;
    return (p.color() == chess::Color::WHITE && toRank == 7) ||
           (p.color() == chess::Color::BLACK && toRank == 0);
}

void GameController::selectSquare(int sqIndex)
{
    if (m_isGameOver || m_isThinking || m_isPromotionPending) return;

    // Check if human can move
    bool isWhiteTurn = (m_board.sideToMove() == chess::Color::WHITE);
    if (m_gameMode == ModePlayerVsAi) {
        if ((isWhiteTurn && m_playerColor == ColorBlack) ||
            (!isWhiteTurn && m_playerColor == ColorWhite)) {
            return; // AI's turn
        }
    }

    if (m_selectedSquare < 0) {
        chess::Piece p = m_board.at(chess::Square(sqIndex));
        if (p != chess::Piece::NONE && p.color() == m_board.sideToMove()) {
            m_selectedSquare = sqIndex;
            m_legalTargets = calculateLegalTargets(sqIndex);
            emit selectionChanged();
            updateBoardView();
        }
    } else {
        if (m_selectedSquare == sqIndex) {
            m_selectedSquare = -1;
            m_legalTargets.clear();
            emit selectionChanged();
            updateBoardView();
            return;
        }

        // Clicked another friendly piece
        chess::Piece clickedPiece = m_board.at(chess::Square(sqIndex));
        if (clickedPiece != chess::Piece::NONE && clickedPiece.color() == m_board.sideToMove()) {
            m_selectedSquare = sqIndex;
            m_legalTargets = calculateLegalTargets(sqIndex);
            emit selectionChanged();
            updateBoardView();
            return;
        }

        // Try to execute move
        if (std::find(m_legalTargets.begin(), m_legalTargets.end(), sqIndex) != m_legalTargets.end()) {
            if (isPawnPromotion(m_selectedSquare, sqIndex)) {
                m_isPromotionPending = true;
                m_pendingFrom = m_selectedSquare;
                m_pendingTo = sqIndex;
                emit promotionPendingChanged();
            } else {
                tryMove(m_selectedSquare, sqIndex, QStringLiteral("q"));
            }
        } else {
            m_selectedSquare = -1;
            m_legalTargets.clear();
            emit selectionChanged();
            updateBoardView();
        }
    }
}

void GameController::submitPromotion(const QString &pieceType)
{
    if (!m_isPromotionPending) return;
    QString promo = pieceType.toLower();
    if (promo.isEmpty()) promo = QStringLiteral("q");

    int fromSq = m_pendingFrom;
    int toSq = m_pendingTo;

    m_isPromotionPending = false;
    m_pendingFrom = -1;
    m_pendingTo = -1;
    emit promotionPendingChanged();

    tryMove(fromSq, toSq, promo);
}

void GameController::cancelPromotion()
{
    m_isPromotionPending = false;
    m_pendingFrom = -1;
    m_pendingTo = -1;
    m_selectedSquare = -1;
    m_legalTargets.clear();
    emit promotionPendingChanged();
    emit selectionChanged();
    updateBoardView();
}

bool GameController::tryMove(int fromSq, int toSq, const QString &promoChar)
{
    if (fromSq < 0 || fromSq >= 64 || toSq < 0 || toSq >= 64) return false;

    QString fromStr = QString("%1%2").arg(QChar('a' + (fromSq % 8))).arg((fromSq / 8) + 1);
    QString toStr = QString("%1%2").arg(QChar('a' + (toSq % 8))).arg((toSq / 8) + 1);
    QString uciMoveStr = fromStr + toStr;

    if (isPawnPromotion(fromSq, toSq)) {
        uciMoveStr += promoChar.toLower();
    }

    chess::Move targetMove;
    try {
        targetMove = chess::uci::uciToMove(m_board, uciMoveStr.toStdString());
    } catch (...) {
        return false;
    }

    if (targetMove == chess::Move::NO_MOVE) return false;

    chess::Movelist legalMoves;
    chess::movegen::legalmoves(legalMoves, m_board);
    bool isLegal = false;
    for (const auto &m : legalMoves) {
        if (m == targetMove) {
            isLegal = true;
            break;
        }
    }
    if (!isLegal) return false;

    // Capture / Castle check
    bool isCapture = (m_board.at(chess::Square(toSq)) != chess::Piece::NONE || targetMove.typeOf() == chess::Move::ENPASSANT);
    bool isCastle = (targetMove.typeOf() == chess::Move::CASTLING);
    bool isWhite = (m_board.sideToMove() == chess::Color::WHITE);

    std::string sanStr = chess::uci::moveToSan(m_board, targetMove);

    m_board.makeMove(targetMove);
    m_playedMoves.push_back(targetMove);

    m_lastFrom = fromSq;
    m_lastTo = toSq;
    m_selectedSquare = -1;
    m_legalTargets.clear();

    QString currentFenStr = QString::fromStdString(m_board.getFen());
    m_history->addMove(isWhite, QString::fromStdString(sanStr), uciMoveStr, currentFenStr);
    m_clock->switchTurn();

    // Audio triggers
    if (m_board.inCheck()) {
        m_soundManager->playCheck();
    } else if (isCastle) {
        m_soundManager->playCastle();
    } else if (isCapture) {
        m_soundManager->playCapture();
    } else {
        m_soundManager->playMove();
    }

    emit selectionChanged();
    emit turnChanged();
    emit boardChanged();

    checkGameEnd();
    updateBoardView();
    updateEnginePosition();

    if (!m_isGameOver && m_gameMode == ModePlayerVsAi) {
        triggerAiMoveIfNeeded();
    }

    return true;
}

void GameController::triggerAiMoveIfNeeded()
{
    if (m_isGameOver) return;

    bool isWhiteTurn = (m_board.sideToMove() == chess::Color::WHITE);
    bool isAiTurn = (m_playerColor == ColorWhite && !isWhiteTurn) ||
                    (m_playerColor == ColorBlack && isWhiteTurn);

    if (isAiTurn) {
        m_isThinking = true;
        emit thinkingChanged();
        m_statusText = QStringLiteral("Stockfish is thinking...");
        emit statusChanged();

        int moveTime = (m_aiElo < 1200) ? 600 : (m_aiElo < 1800) ? 1200 : 2000;
        int maxDepth = (m_aiElo < 1200) ? 8 : (m_aiElo < 1800) ? 14 : 22;
        m_uci->searchBestMove(moveTime, maxDepth);
    }
}

void GameController::onAiBestMoveFound(const QString &bestMove, const QString &)
{
    if (!m_isThinking || m_isGameOver) return;
    m_isThinking = false;
    emit thinkingChanged();

    if (bestMove.length() >= 4) {
        int fromFile = bestMove[0].toLatin1() - 'a';
        int fromRank = bestMove[1].toLatin1() - '1';
        int toFile = bestMove[2].toLatin1() - 'a';
        int toRank = bestMove[3].toLatin1() - '1';
        int fromSq = fromRank * 8 + fromFile;
        int toSq = toRank * 8 + toFile;
        QString promo = (bestMove.length() >= 5) ? bestMove.mid(4, 1) : QStringLiteral("q");

        tryMove(fromSq, toSq, promo);
    }
}

void GameController::updateEnginePosition()
{
    QStringList moves;
    for (const auto &m : m_playedMoves) {
        moves.append(QString::fromStdString(chess::uci::moveToUci(m)));
    }
    m_uci->setPosition(QStringLiteral("startpos"), moves);
    if (m_gameMode == ModeAnalysis && !m_isGameOver) {
        m_uci->startInfiniteAnalysis();
    }
}

void GameController::checkGameEnd()
{
    auto [reason, result] = m_board.isGameOver();
    if (result != chess::GameResult::NONE) {
        m_isGameOver = true;
        m_clock->pause();
        m_uci->stopAnalysis();
        m_soundManager->playEnd();

        if (reason == chess::GameResultReason::CHECKMATE) {
            QString winner = (m_board.sideToMove() == chess::Color::WHITE) ? QStringLiteral("Black") : QStringLiteral("White");
            m_gameResult = QString("%1 won by Checkmate!").arg(winner);
        } else if (reason == chess::GameResultReason::STALEMATE) {
            m_gameResult = QStringLiteral("Draw by Stalemate");
        } else if (reason == chess::GameResultReason::THREEFOLD_REPETITION) {
            m_gameResult = QStringLiteral("Draw by Threefold Repetition");
        } else if (reason == chess::GameResultReason::FIFTY_MOVE_RULE) {
            m_gameResult = QStringLiteral("Draw by 50-Move Rule");
        } else if (reason == chess::GameResultReason::INSUFFICIENT_MATERIAL) {
            m_gameResult = QStringLiteral("Draw by Insufficient Material");
        } else {
            m_gameResult = QStringLiteral("Game Drawn");
        }
        m_statusText = m_gameResult;
        emit gameOverChanged();
        emit statusChanged();
    } else {
        if (m_board.inCheck()) {
            m_statusText = QString("%1 is in Check!").arg(m_board.sideToMove() == chess::Color::WHITE ? "White" : "Black");
        } else {
            m_statusText = QString("%1 to move").arg(m_board.sideToMove() == chess::Color::WHITE ? "White" : "Black");
        }
        emit statusChanged();
    }
}

void GameController::onClockTimeOut(int side)
{
    if (m_isGameOver) return;
    m_isGameOver = true;
    m_soundManager->playEnd();
    QString winner = (side == ChessClock::White) ? QStringLiteral("Black") : QStringLiteral("White");
    m_gameResult = QString("%1 won on time!").arg(winner);
    m_statusText = m_gameResult;
    emit gameOverChanged();
    emit statusChanged();
}

void GameController::resignCurrentPlayer()
{
    if (m_isGameOver) return;
    m_isGameOver = true;
    m_clock->pause();
    m_soundManager->playEnd();
    QString winner = (m_board.sideToMove() == chess::Color::WHITE) ? QStringLiteral("Black") : QStringLiteral("White");
    m_gameResult = QString("%1 won by Resignation").arg(winner);
    m_statusText = m_gameResult;
    emit gameOverChanged();
    emit statusChanged();
}

void GameController::undoMove()
{
    if (m_playedMoves.empty() || m_isThinking) return;

    int steps = (m_gameMode == ModePlayerVsAi && m_playedMoves.size() >= 2) ? 2 : 1;
    for (int i = 0; i < steps && !m_playedMoves.empty(); ++i) {
        m_board.unmakeMove(m_playedMoves.back());
        m_playedMoves.pop_back();
    }

    m_isGameOver = false;
    m_gameResult.clear();
    m_selectedSquare = -1;
    m_legalTargets.clear();

    if (!m_playedMoves.empty()) {
        m_lastFrom = m_playedMoves.back().from().index();
        m_lastTo = m_playedMoves.back().to().index();
    } else {
        m_lastFrom = -1;
        m_lastTo = -1;
    }

    m_history->setCurrentPly(static_cast<int>(m_playedMoves.size()));
    updateBoardView();
    updateEnginePosition();
    checkGameEnd();

    emit selectionChanged();
    emit turnChanged();
    emit boardChanged();
    emit gameOverChanged();
}

void GameController::loadFen(const QString &fen)
{
    try {
        chess::Board newBoard(fen.toStdString());
        m_board = newBoard;
        m_playedMoves.clear();
        m_history->clear();
        m_selectedSquare = -1;
        m_legalTargets.clear();
        m_lastFrom = -1;
        m_lastTo = -1;
        m_isGameOver = false;
        m_gameResult.clear();

        updateBoardView();
        checkGameEnd();
        updateEnginePosition();

        emit boardChanged();
        emit turnChanged();
        emit gameOverChanged();
    } catch (...) {
        qWarning() << "Failed to parse FEN:" << fen;
    }
}

void GameController::loadPgn(const QString &pgn)
{
    // Reset and step through PGN moves
    newGame(ModeAnalysis);
    // Simple SAN / move sequence parsing
    QString cleaned = pgn;
    // Remove headers
    cleaned.remove(QRegularExpression(QStringLiteral("\\[.*?\\]")));
    // Remove comments
    cleaned.remove(QRegularExpression(QStringLiteral("\\{.*?\\}")));
    // Remove move numbers e.g. "1."
    cleaned.remove(QRegularExpression(QStringLiteral("\\d+\\.\\.\\.|\\d+\\.")));

    QStringList tokens = cleaned.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    for (const QString &token : tokens) {
        if (token == "1-0" || token == "0-1" || token == "1/2-1/2" || token == "*") break;
        try {
            chess::Move m = chess::uci::parseSan(m_board, token.toStdString());
            if (m != chess::Move::NO_MOVE) {
                std::string uciStr = chess::uci::moveToUci(m);
                int from = m.from().index();
                int to = m.to().index();
                QString promo = (uciStr.length() >= 5) ? QString::fromStdString(uciStr.substr(4, 1)) : QStringLiteral("q");
                tryMove(from, to, promo);
            }
        } catch (...) {
            break;
        }
    }
}

void GameController::goToPly(int ply)
{
    QString targetFen = m_history->getFenAtPly(ply);
    if (!targetFen.isEmpty()) {
        try {
            m_board = chess::Board(targetFen.toStdString());
            m_history->setCurrentPly(ply);
            updateBoardView();
            updateEnginePosition();
            emit boardChanged();
            emit turnChanged();
        } catch (...) {}
    }
}

void GameController::copyFenToClipboard()
{
    QClipboard *cb = QGuiApplication::clipboard();
    if (cb) {
        cb->setText(currentFen());
    }
}

void GameController::copyPgnToClipboard()
{
    QClipboard *cb = QGuiApplication::clipboard();
    if (cb) {
        QString pgn = m_history->exportPgn(
            (m_playerColor == ColorWhite) ? "Player" : "Stockfish",
            (m_playerColor == ColorBlack) ? "Player" : "Stockfish",
            "Qt6Chess Match",
            m_gameResult
        );
        cb->setText(pgn);
    }
}

void GameController::updateBoardView()
{
    int kingInCheckSq = -1;
    if (m_board.inCheck()) {
        kingInCheckSq = m_board.kingSq(m_board.sideToMove()).index();
    }

    m_boardModel->syncWithBoard(m_board,
                                m_selectedSquare,
                                m_legalTargets,
                                m_lastFrom,
                                m_lastTo,
                                kingInCheckSq);
}
