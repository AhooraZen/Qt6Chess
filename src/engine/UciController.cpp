#include "UciController.h"
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QDebug>

UciController::UciController(QObject *parent)
    : QObject(parent), m_process(new QProcess(this))
{
    connect(m_process, &QProcess::readyReadStandardOutput, this, &UciController::onReadyReadStandardOutput);
    connect(m_process, &QProcess::finished, this, &UciController::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this, &UciController::onProcessError);
}

UciController::~UciController()
{
    stopEngine();
}

bool UciController::isRunning() const
{
    return m_process && m_process->state() == QProcess::Running;
}

#include <QStandardPaths>

QString UciController::findDefaultEnginePath() const
{
    QString inPath = QStandardPaths::findExecutable(QStringLiteral("stockfish"));
    if (!inPath.isEmpty())
        return inPath;

    const QStringList candidates = {
        QStringLiteral("/usr/bin/stockfish"),
        QStringLiteral("/usr/games/stockfish"),
        QStringLiteral("/usr/local/bin/stockfish"),
        QStringLiteral("/opt/stockfish/stockfish")
    };
    for (const QString &path : candidates) {
        if (QFile::exists(path))
            return path;
    }
    return QString();
}


bool UciController::startEngine(const QString &customPath)
{
    if (isRunning())
        stopEngine();

    QString enginePath = customPath.isEmpty() ? findDefaultEnginePath() : customPath;
    if (enginePath.isEmpty() || !QFile::exists(enginePath)) {
        return false;
    }

    m_process->setProcessChannelMode(QProcess::MergedChannels);
    m_process->start(enginePath);
    if (!m_process->waitForStarted(1500)) {
        qWarning() << "Failed to start UCI engine at:" << enginePath;
        return false;
    }

    sendCommand(QStringLiteral("uci"));
    sendCommand(QStringLiteral("setoption name MultiPV value 2"));
    sendCommand(QStringLiteral("isready"));
    if (!m_currentPositionCmd.isEmpty()) {
        sendCommand(m_currentPositionCmd);
    }

    emit runningChanged();
    return true;

}

void UciController::stopEngine()
{
    if (isRunning()) {
        sendCommand(QStringLiteral("stop"));
        sendCommand(QStringLiteral("quit"));
        if (!m_process->waitForFinished(1000)) {
            m_process->kill();
        }
    }
    m_isAnalyzing = false;
    emit analyzingChanged();
    emit runningChanged();
}

void UciController::sendCommand(const QString &cmd)
{
    if (isRunning()) {
        m_process->write((cmd + "\n").toUtf8());
    }
}

void UciController::setMultiPv(int lines)
{
    m_multiPv = std::clamp(lines, 1, 5);
    sendCommand(QString("setoption name MultiPV value %1").arg(m_multiPv));
    emit multiPvChanged();
}

void UciController::setElo(int elo)
{
    if (elo < 3000) {
        sendCommand(QStringLiteral("setoption name UCI_LimitStrength value true"));
        sendCommand(QString("setoption name UCI_Elo value %1").arg(std::clamp(elo, 800, 3190)));
    } else {
        sendCommand(QStringLiteral("setoption name UCI_LimitStrength value false"));
    }
}

void UciController::setPosition(const QString &fen, const QStringList &moves)
{
    m_lines.clear();
    m_candidateLines.clear();
    emit candidateLinesChanged();

    QString cleanFen = fen.trimmed();
    cleanFen.remove(QLatin1Char('\r')).remove(QLatin1Char('\n'));

    QString cmd;
    if (cleanFen.isEmpty() || cleanFen == QStringLiteral("startpos")) {
        cmd = QStringLiteral("position startpos");
    } else {
        cmd = QString("position fen %1").arg(cleanFen);
    }

    if (!moves.isEmpty()) {
        cmd += QStringLiteral(" moves ") + moves.join(" ");
    }
    m_currentPositionCmd = cmd;
    if (!isRunning()) {
        startEngine();
    } else {
        sendCommand(cmd);
    }
}

void UciController::startInfiniteAnalysis()
{
    if (!isRunning()) {
        if (!startEngine()) return;
    }
    sendCommand(QStringLiteral("stop"));
    if (!m_currentPositionCmd.isEmpty()) {
        sendCommand(m_currentPositionCmd);
    }
    sendCommand(QStringLiteral("go infinite"));
    m_isAnalyzing = true;
    emit analyzingChanged();
}

void UciController::stopAnalysis()
{
    if (isRunning()) {
        sendCommand(QStringLiteral("stop"));
    }
    m_isAnalyzing = false;
    emit analyzingChanged();
}

void UciController::searchBestMove(int moveTimeMs, int depthLimit)
{
    if (!isRunning()) {
        if (!startEngine()) return;
    }
    sendCommand(QStringLiteral("stop"));
    if (!m_currentPositionCmd.isEmpty()) {
        sendCommand(m_currentPositionCmd);
    }
    if (depthLimit > 0) {
        sendCommand(QString("go movetime %1 depth %2").arg(moveTimeMs).arg(depthLimit));
    } else {
        sendCommand(QString("go movetime %1").arg(moveTimeMs));
    }
}



void UciController::onReadyReadStandardOutput()
{
    while (m_process->canReadLine()) {
        QString line = QString::fromUtf8(m_process->readLine()).trimmed();
        parseLine(line);
    }
}

void UciController::parseLine(const QString &line)
{
    if (line.startsWith(QStringLiteral("id name "))) {
        m_engineName = line.mid(8);
        emit engineNameChanged();
        return;
    }

    if (line.startsWith(QStringLiteral("bestmove "))) {
        QStringList parts = line.split(' ', Qt::SkipEmptyParts);
        if (parts.size() >= 2) {
            QString bestMove = parts[1];
            QString ponder;
            if (parts.size() >= 4 && parts[2] == QStringLiteral("ponder")) {
                ponder = parts[3];
            }
            emit bestMoveFound(bestMove, ponder);
        }
        return;
    }

    if (line.startsWith(QStringLiteral("info ")) && line.contains(QStringLiteral(" score "))) {
        QStringList tokens = line.split(' ', Qt::SkipEmptyParts);

        int pvRank = 1;
        int depthVal = m_depth;
        qint64 npsVal = m_nps;
        double cpScore = 0.0;
        int mateScore = 0;
        bool hasMate = false;
        QString pvStr;

        for (int i = 0; i < tokens.size(); ++i) {
            if (tokens[i] == QStringLiteral("depth") && i + 1 < tokens.size()) {
                depthVal = tokens[i + 1].toInt();
            } else if (tokens[i] == QStringLiteral("multipv") && i + 1 < tokens.size()) {
                pvRank = tokens[i + 1].toInt();
            } else if (tokens[i] == QStringLiteral("nps") && i + 1 < tokens.size()) {
                npsVal = tokens[i + 1].toLongLong();
            } else if (tokens[i] == QStringLiteral("score") && i + 2 < tokens.size()) {
                if (tokens[i + 1] == QStringLiteral("cp")) {
                    cpScore = tokens[i + 2].toDouble() / 100.0;
                    hasMate = false;
                } else if (tokens[i + 1] == QStringLiteral("mate")) {
                    mateScore = tokens[i + 2].toInt();
                    hasMate = true;
                }
            } else if (tokens[i] == QStringLiteral("pv")) {
                QStringList pvMoves;
                for (int j = i + 1; j < tokens.size(); ++j) {
                    pvMoves.append(tokens[j]);
                }
                pvStr = pvMoves.join(" ");
                break;
            }
        }

        QString firstMove = pvStr.section(' ', 0, 0);
        QString fromSq = firstMove.length() >= 2 ? firstMove.left(2) : "";
        QString toSq = firstMove.length() >= 4 ? firstMove.mid(2, 2) : "";

        double normalizedEval = m_isWhiteToMove ? cpScore : -cpScore;
        int normalizedMate = m_isWhiteToMove ? mateScore : -mateScore;

        if (pvRank == 1) {
            m_depth = depthVal;
            m_nps = npsVal;
            m_currentEval = normalizedEval;
            m_isMate = hasMate;
            m_mateIn = normalizedMate;
        }

        EngineLine &lineData = m_lines[pvRank];
        lineData.multiPv = pvRank;
        lineData.depth = depthVal;
        lineData.evalCp = normalizedEval;
        lineData.mateIn = normalizedMate;
        lineData.isMate = hasMate;
        lineData.nps = npsVal;
        lineData.pv = pvStr;
        lineData.bestMove = firstMove;
        lineData.fromSquare = fromSq;
        lineData.toSquare = toSq;

        bool shouldEmit = !m_infoTimer.isValid() || m_infoTimer.elapsed() >= 50;
        if (shouldEmit) {
            m_infoTimer.restart();

            if (pvRank == 1) {
                emit depthChanged();
                emit npsChanged();
                emit currentEvalChanged();
                emit primaryArrowChanged(fromSq, toSq);
            } else if (pvRank == 2) {
                emit secondaryArrowChanged(fromSq, toSq);
            }

            emit lineUpdated(pvRank, normalizedEval, hasMate, normalizedMate, depthVal, pvStr, fromSq, toSq);

            QVariantList cLines;
            for (auto it = m_lines.begin(); it != m_lines.end(); ++it) {
                QVariantMap m;
                m[QStringLiteral("rank")] = it.key();
                m[QStringLiteral("depth")] = it.value().depth;
                m[QStringLiteral("eval")] = it.value().evalCp;
                m[QStringLiteral("isMate")] = it.value().isMate;
                m[QStringLiteral("mateIn")] = it.value().mateIn;
                m[QStringLiteral("pv")] = it.value().pv;
                m[QStringLiteral("bestMove")] = it.value().bestMove;
                cLines.append(m);
            }
            m_candidateLines = cLines;
            emit candidateLinesChanged();
        }
    }
}

void UciController::onProcessFinished(int, QProcess::ExitStatus)
{
    m_isAnalyzing = false;
    emit runningChanged();
    emit analyzingChanged();
}

void UciController::onProcessError(QProcess::ProcessError error)
{
    qWarning() << "UCI Process error:" << error;
    m_isAnalyzing = false;
    emit runningChanged();
    emit analyzingChanged();
}
