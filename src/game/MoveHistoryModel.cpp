#include "MoveHistoryModel.h"
#include <QDateTime>

MoveHistoryModel::MoveHistoryModel(QObject *parent)
    : QAbstractListModel(parent)
{
    m_fenHistory.push_back(QStringLiteral("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"));
}

int MoveHistoryModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return static_cast<int>(m_turns.size());
}

QVariant MoveHistoryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_turns.size()))
        return QVariant();

    const auto &turn = m_turns[index.row()];
    int whitePly = index.row() * 2 + 1;
    int blackPly = index.row() * 2 + 2;

    switch (role) {
    case TurnNumberRole:
        return turn.turnNumber;
    case WhiteSanRole:
        return turn.whiteSan;
    case BlackSanRole:
        return turn.blackSan;
    case WhiteUciRole:
        return turn.whiteUci;
    case BlackUciRole:
        return turn.blackUci;
    case IsWhiteSelectedRole:
        return (m_currentPly == whitePly);
    case IsBlackSelectedRole:
        return (m_currentPly == blackPly);
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> MoveHistoryModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[TurnNumberRole] = "turnNumber";
    roles[WhiteSanRole] = "whiteSan";
    roles[BlackSanRole] = "blackSan";
    roles[WhiteUciRole] = "whiteUci";
    roles[BlackUciRole] = "blackUci";
    roles[IsWhiteSelectedRole] = "isWhiteSelected";
    roles[IsBlackSelectedRole] = "isBlackSelected";
    return roles;
}

void MoveHistoryModel::addMove(bool isWhite, const QString &san, const QString &uci, const QString &fen)
{
    if (isWhite) {
        beginInsertRows(QModelIndex(), static_cast<int>(m_turns.size()), static_cast<int>(m_turns.size()));
        MoveTurn turn;
        turn.turnNumber = static_cast<int>(m_turns.size()) + 1;
        turn.whiteSan = san;
        turn.whiteUci = uci;
        turn.whiteFen = fen;
        m_turns.push_back(turn);
        endInsertRows();
    } else {
        if (!m_turns.empty()) {
            m_turns.back().blackSan = san;
            m_turns.back().blackUci = uci;
            m_turns.back().blackFen = fen;
            QModelIndex idx = index(static_cast<int>(m_turns.size()) - 1);
            emit dataChanged(idx, idx, {BlackSanRole, BlackUciRole, IsBlackSelectedRole});
        }
    }

    m_fenHistory.push_back(fen);
    m_totalPly++;
    m_currentPly = m_totalPly;

    emit countChanged();
    emit currentPlyChanged();
}

void MoveHistoryModel::clear()
{
    beginResetModel();
    m_turns.clear();
    m_fenHistory.clear();
    m_fenHistory.push_back(QStringLiteral("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"));
    m_totalPly = 0;
    m_currentPly = 0;
    endResetModel();

    emit countChanged();
    emit currentPlyChanged();
}

void MoveHistoryModel::setCurrentPly(int ply)
{
    int clamped = std::clamp(ply, 0, m_totalPly);
    if (m_currentPly != clamped) {
        int oldPly = m_currentPly;
        m_currentPly = clamped;
        emit currentPlyChanged();

        int oldTurn = (oldPly > 0) ? (oldPly - 1) / 2 : 0;
        int newTurn = (clamped > 0) ? (clamped - 1) / 2 : 0;
        int minTurn = std::min(oldTurn, newTurn);
        int maxTurn = std::max(oldTurn, newTurn);

        if (!m_turns.empty() && minTurn < static_cast<int>(m_turns.size())) {
            maxTurn = std::min(maxTurn, static_cast<int>(m_turns.size()) - 1);
            emit dataChanged(index(minTurn), index(maxTurn), {IsWhiteSelectedRole, IsBlackSelectedRole});
        }
    }
}

void MoveHistoryModel::truncateAfterPly(int ply)
{
    if (ply < 0) ply = 0;
    if (ply >= m_totalPly) return;

    int targetTurns = (ply + 1) / 2;
    if (targetTurns < static_cast<int>(m_turns.size())) {
        beginRemoveRows(QModelIndex(), targetTurns, static_cast<int>(m_turns.size()) - 1);
        m_turns.erase(m_turns.begin() + targetTurns, m_turns.end());
        endRemoveRows();
    }
    if (ply % 2 != 0 && !m_turns.empty()) {
        int lastIdx = static_cast<int>(m_turns.size()) - 1;
        m_turns[lastIdx].blackSan.clear();
        m_turns[lastIdx].blackUci.clear();
        m_turns[lastIdx].blackFen.clear();
        QModelIndex idx = index(lastIdx);
        emit dataChanged(idx, idx, {BlackSanRole, BlackUciRole, IsBlackSelectedRole});
    }
    if (ply + 1 < static_cast<int>(m_fenHistory.size())) {
        m_fenHistory.erase(m_fenHistory.begin() + (ply + 1), m_fenHistory.end());
    }
    m_totalPly = ply;
    m_currentPly = ply;
    emit countChanged();
    emit currentPlyChanged();
}

QString MoveHistoryModel::getFenAtPly(int ply) const
{
    if (ply >= 0 && ply < static_cast<int>(m_fenHistory.size())) {
        return m_fenHistory[ply];
    }
    return QString();
}

QString MoveHistoryModel::exportPgn(const QString &whitePlayer, const QString &blackPlayer, const QString &event, const QString &result) const
{
    QString pgn;
    pgn += QString("[Event \"%1\"]\n").arg(event.isEmpty() ? "Casual Game" : event);
    pgn += QString("[Site \"%1\"]\n").arg("Qt6Chess Desktop");
    pgn += QString("[Date \"%1\"]\n").arg(QDate::currentDate().toString("yyyy.MM.dd"));
    pgn += QString("[Round \"1\"]\n");
    pgn += QString("[White \"%1\"]\n").arg(whitePlayer.isEmpty() ? "White" : whitePlayer);
    pgn += QString("[Black \"%1\"]\n").arg(blackPlayer.isEmpty() ? "Black" : blackPlayer);
    pgn += QString("[Result \"%1\"]\n\n").arg(result.isEmpty() ? "*" : result);

    for (const auto &turn : m_turns) {
        pgn += QString("%1. %2").arg(turn.turnNumber).arg(turn.whiteSan);
        if (!turn.blackSan.isEmpty()) {
            pgn += QString(" %1 ").arg(turn.blackSan);
        } else {
            pgn += " ";
        }
    }
    if (!result.isEmpty()) {
        pgn += result;
    }
    return pgn.trimmed();
}
