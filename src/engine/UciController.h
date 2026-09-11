#pragma once

#include <QObject>
#include <QProcess>
#include <QStringList>
#include <QMap>

struct EngineLine {
    int multiPv = 1;
    int depth = 0;
    double evalCp = 0.0;
    int mateIn = 0;
    bool isMate = false;
    qint64 nodes = 0;
    qint64 nps = 0;
    QString pv;
    QString bestMove;
    QString fromSquare;
    QString toSquare;
};

class UciController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool isRunning READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(bool isAnalyzing READ isAnalyzing NOTIFY analyzingChanged)
    Q_PROPERTY(double currentEval READ currentEval NOTIFY currentEvalChanged)
    Q_PROPERTY(bool isMate READ isMate NOTIFY currentEvalChanged)
    Q_PROPERTY(int mateIn READ mateIn NOTIFY currentEvalChanged)
    Q_PROPERTY(int depth READ depth NOTIFY depthChanged)
    Q_PROPERTY(qint64 nps READ nps NOTIFY npsChanged)
    Q_PROPERTY(QString engineName READ engineName NOTIFY engineNameChanged)
    Q_PROPERTY(int multiPv READ multiPv WRITE setMultiPv NOTIFY multiPvChanged)

public:
    explicit UciController(QObject *parent = nullptr);
    ~UciController() override;

    bool isRunning() const;
    bool isAnalyzing() const { return m_isAnalyzing; }
    double currentEval() const { return m_currentEval; }
    bool isMate() const { return m_isMate; }
    int mateIn() const { return m_mateIn; }
    int depth() const { return m_depth; }
    qint64 nps() const { return m_nps; }
    QString engineName() const { return m_engineName; }
    int multiPv() const { return m_multiPv; }

    void setMultiPv(int lines);

public slots:
    bool startEngine(const QString &customPath = QString());
    void stopEngine();
    void setPosition(const QString &fen, const QStringList &moves = QStringList());
    void startInfiniteAnalysis();
    void stopAnalysis();
    void searchBestMove(int moveTimeMs = 1500, int depthLimit = 20);
    void setElo(int elo);

signals:
    void runningChanged();
    void analyzingChanged();
    void currentEvalChanged();
    void depthChanged();
    void npsChanged();
    void engineNameChanged();
    void multiPvChanged();
    void bestMoveFound(const QString &bestMove, const QString &ponder);
    void lineUpdated(int pvIndex, double eval, bool isMate, int mateIn, int depth, const QString &pv, const QString &fromSq, const QString &toSq);
    void primaryArrowChanged(const QString &fromSq, const QString &toSq);
    void secondaryArrowChanged(const QString &fromSq, const QString &toSq);

private slots:
    void onReadyReadStandardOutput();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);

private:
    QProcess *m_process;
    bool m_isAnalyzing = false;
    double m_currentEval = 0.0;
    bool m_isMate = false;
    int m_mateIn = 0;
    int m_depth = 0;
    qint64 m_nps = 0;
    int m_multiPv = 2;
    QString m_engineName = QStringLiteral("Stockfish");
    QString m_currentPositionCmd;
    QMap<int, EngineLine> m_lines;


    void sendCommand(const QString &cmd);
    void parseLine(const QString &line);
    QString findDefaultEnginePath() const;
};
