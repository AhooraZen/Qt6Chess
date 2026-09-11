#pragma once

#include <QObject>
#include <QStringList>
#include <vector>
#include "chess.hpp"
#include "ChessBoardModel.h"
#include "UciController.h"
#include "ChessClock.h"
#include "MoveHistoryModel.h"
#include "SoundManager.h"

class GameController : public QObject {
    Q_OBJECT

    Q_PROPERTY(ChessBoardModel* boardModel READ boardModel CONSTANT)
    Q_PROPERTY(UciController* uciController READ uciController CONSTANT)
    Q_PROPERTY(ChessClock* clock READ clock CONSTANT)
    Q_PROPERTY(MoveHistoryModel* historyModel READ historyModel CONSTANT)
    Q_PROPERTY(SoundManager* soundManager READ soundManager CONSTANT)

    Q_PROPERTY(int selectedSquare READ selectedSquare NOTIFY selectionChanged)
    Q_PROPERTY(bool flipped READ isFlipped WRITE setFlipped NOTIFY flippedChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)
    Q_PROPERTY(QString activeColor READ activeColor NOTIFY turnChanged)
    Q_PROPERTY(bool isGameOver READ isGameOver NOTIFY gameOverChanged)
    Q_PROPERTY(QString gameResult READ gameResult NOTIFY gameOverChanged)
    Q_PROPERTY(bool isThinking READ isThinking NOTIFY thinkingChanged)
    Q_PROPERTY(bool isPromotionPending READ isPromotionPending NOTIFY promotionPendingChanged)
    Q_PROPERTY(int gameMode READ gameMode WRITE setGameMode NOTIFY gameModeChanged)
    Q_PROPERTY(int aiElo READ aiElo WRITE setAiElo NOTIFY aiSettingsChanged)
    Q_PROPERTY(int playerColor READ playerColor WRITE setPlayerColor NOTIFY aiSettingsChanged)
    Q_PROPERTY(QString currentFen READ currentFen NOTIFY boardChanged)

public:
    enum GameMode {
        ModePlayerVsAi = 0,
        ModePlayerVsPlayer = 1,
        ModeAnalysis = 2
    };
    Q_ENUM(GameMode)

    enum ColorSide {
        ColorWhite = 1,
        ColorBlack = 2,
        ColorBoth = 3
    };
    Q_ENUM(ColorSide)

    explicit GameController(QObject *parent = nullptr);

    ChessBoardModel* boardModel() const { return m_boardModel; }
    UciController* uciController() const { return m_uci; }
    ChessClock* clock() const { return m_clock; }
    MoveHistoryModel* historyModel() const { return m_history; }
    SoundManager* soundManager() const { return m_soundManager; }

    int selectedSquare() const { return m_selectedSquare; }
    bool isFlipped() const { return m_flipped; }
    void setFlipped(bool f);

    QString statusText() const { return m_statusText; }
    QString activeColor() const;
    bool isGameOver() const { return m_isGameOver; }
    QString gameResult() const { return m_gameResult; }
    bool isThinking() const { return m_isThinking; }
    bool isPromotionPending() const { return m_isPromotionPending; }

    int gameMode() const { return m_gameMode; }
    void setGameMode(int mode);

    int aiElo() const { return m_aiElo; }
    void setAiElo(int elo);

    int playerColor() const { return m_playerColor; }
    void setPlayerColor(int color);

    QString currentFen() const;

public slots:
    void newGame(int mode = ModePlayerVsAi, int color = ColorWhite, int timeMin = 3, int incSec = 2);
    void selectSquare(int sqIndex);
    bool tryMove(int fromSq, int toSq, const QString &promoChar = QStringLiteral("q"));
    void submitPromotion(const QString &pieceType);
    void cancelPromotion();
    void undoMove();
    void flipBoard();
    void loadFen(const QString &fen);
    void loadPgn(const QString &pgn);
    void goToPly(int ply);
    void copyFenToClipboard();
    void copyPgnToClipboard();
    void resignCurrentPlayer();
    void switchToAnalysisMode();

signals:
    void selectionChanged();
    void flippedChanged();
    void statusChanged();
    void turnChanged();
    void gameOverChanged();
    void thinkingChanged();
    void promotionPendingChanged();
    void gameModeChanged();
    void aiSettingsChanged();
    void boardChanged();

private slots:
    void onAiBestMoveFound(const QString &bestMove, const QString &ponder);
    void onClockTimeOut(int side);

private:
    ChessBoardModel *m_boardModel;
    UciController *m_uci;
    ChessClock *m_clock;
    MoveHistoryModel *m_history;
    SoundManager *m_soundManager;

    chess::Board m_board;
    int m_selectedSquare = -1;
    std::vector<int> m_legalTargets;
    int m_lastFrom = -1;
    int m_lastTo = -1;

    bool m_flipped = false;
    QString m_statusText;
    QString m_startFen = QStringLiteral("startpos");
    bool m_isGameOver = false;
    QString m_gameResult;
    bool m_isThinking = false;
    bool m_isPromotionPending = false;
    int m_pendingFrom = -1;
    int m_pendingTo = -1;

    int m_gameMode = ModePlayerVsAi;
    int m_aiElo = 1500;
    int m_playerColor = ColorWhite; // human color in PvAI

    std::vector<chess::Move> m_playedMoves;

    void updateBoardView();
    void checkGameEnd();
    void triggerAiMoveIfNeeded();
    void updateEnginePosition();
    std::vector<int> calculateLegalTargets(int fromSq);
    bool isPawnPromotion(int fromSq, int toSq) const;
};
