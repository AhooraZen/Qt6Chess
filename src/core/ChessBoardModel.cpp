#include "ChessBoardModel.h"
#include <algorithm>

ChessBoardModel::ChessBoardModel(QObject *parent)
    : QAbstractListModel(parent)
{
    m_squares.resize(64);
    for (int i = 0; i < 64; ++i) {
        int file = i % 8;
        int rank = i / 8;
        m_squares[i].squareIndex = i;
        m_squares[i].squareName = QString("%1%2").arg(QChar('a' + file)).arg(rank + 1);
        m_squares[i].isLight = ((file + rank) % 2 != 0);
    }
}

int ChessBoardModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return static_cast<int>(m_squares.size());
}

QVariant ChessBoardModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_squares.size()))
        return QVariant();

    const auto &sq = m_squares[index.row()];
    switch (role) {
    case SquareIndexRole:
        return sq.squareIndex;
    case SquareNameRole:
        return sq.squareName;
    case PieceTypeRole:
        return sq.pieceType;
    case PieceColorRole:
        return sq.pieceColor;
    case PieceCodeRole:
        return sq.pieceCode;
    case IsLightSquareRole:
        return sq.isLight;
    case IsSelectedRole:
        return sq.isSelected;
    case IsLegalTargetRole:
        return sq.isLegalTarget;
    case IsLastMoveRole:
        return sq.isLastMove;
    case IsInCheckRole:
        return sq.isInCheck;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> ChessBoardModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[SquareIndexRole] = "squareIndex";
    roles[SquareNameRole] = "squareName";
    roles[PieceTypeRole] = "pieceType";
    roles[PieceColorRole] = "pieceColor";
    roles[PieceCodeRole] = "pieceCode";
    roles[IsLightSquareRole] = "isLightSquare";
    roles[IsSelectedRole] = "isSelected";
    roles[IsLegalTargetRole] = "isLegalTarget";
    roles[IsLastMoveRole] = "isLastMove";
    roles[IsInCheckRole] = "isInCheck";
    return roles;
}

QVariantMap ChessBoardModel::getSquareData(int sqIndex) const
{
    QVariantMap map;
    if (sqIndex < 0 || sqIndex >= static_cast<int>(m_squares.size()))
        return map;

    const auto &sq = m_squares[sqIndex];
    map["squareIndex"] = sq.squareIndex;
    map["squareName"] = sq.squareName;
    map["pieceType"] = sq.pieceType;
    map["pieceColor"] = sq.pieceColor;
    map["pieceCode"] = sq.pieceCode;
    map["isLightSquare"] = sq.isLight;
    map["isSelected"] = sq.isSelected;
    map["isLegalTarget"] = sq.isLegalTarget;
    map["isLastMove"] = sq.isLastMove;
    map["isInCheck"] = sq.isInCheck;
    return map;
}

void ChessBoardModel::syncWithBoard(const chess::Board &board,
                                   int selectedSquare,
                                   const std::vector<int> &legalTargets,
                                   int lastFrom,
                                   int lastTo,
                                   int kingInCheckSquare)
{
    for (int i = 0; i < 64; ++i) {
        SquareData oldData = m_squares[i];
        SquareData newData = oldData;

        chess::Piece p = board.at(chess::Square(i));
        if (p != chess::Piece::NONE) {
            char c = (p.color() == chess::Color::WHITE) ? 'w' : 'b';
            char t = 'P';
            if (p.type() == chess::PieceType::PAWN) t = 'P';
            else if (p.type() == chess::PieceType::KNIGHT) t = 'N';
            else if (p.type() == chess::PieceType::BISHOP) t = 'B';
            else if (p.type() == chess::PieceType::ROOK) t = 'R';
            else if (p.type() == chess::PieceType::QUEEN) t = 'Q';
            else if (p.type() == chess::PieceType::KING) t = 'K';
            newData.pieceColor = QString(c);
            newData.pieceType = QString(t).toLower();
            newData.pieceCode = QString("%1%2").arg(c).arg(t);
        } else {
            newData.pieceColor = "";
            newData.pieceType = "";
            newData.pieceCode = "";
        }

        newData.isSelected = (i == selectedSquare);
        newData.isLegalTarget = (std::find(legalTargets.begin(), legalTargets.end(), i) != legalTargets.end());
        newData.isLastMove = (i == lastFrom || i == lastTo);
        newData.isInCheck = (i == kingInCheckSquare);

        if (oldData.pieceCode != newData.pieceCode ||
            oldData.isSelected != newData.isSelected ||
            oldData.isLegalTarget != newData.isLegalTarget ||
            oldData.isLastMove != newData.isLastMove ||
            oldData.isInCheck != newData.isInCheck) {
            m_squares[i] = newData;
            QModelIndex idx = index(i);
            emit dataChanged(idx, idx);
        }
    }
}
