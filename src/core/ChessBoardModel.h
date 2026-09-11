#pragma once

#include <QAbstractListModel>
#include <vector>
#include <string>
#include "chess.hpp"

class ChessBoardModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum ChessRoles {
        SquareIndexRole = Qt::UserRole + 1,
        SquareNameRole,
        PieceTypeRole,
        PieceColorRole,
        PieceCodeRole,
        IsLightSquareRole,
        IsSelectedRole,
        IsLegalTargetRole,
        IsLastMoveRole,
        IsInCheckRole
    };
    Q_ENUM(ChessRoles)

    struct SquareData {
        int squareIndex = 0;
        QString squareName;
        QString pieceType;
        QString pieceColor;
        QString pieceCode;
        bool isLight = false;
        bool isSelected = false;
        bool isLegalTarget = false;
        bool isLastMove = false;
        bool isInCheck = false;
    };

    explicit ChessBoardModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE QVariantMap getSquareData(int sqIndex) const;

    void syncWithBoard(const chess::Board &board,
                       int selectedSquare,
                       const std::vector<int> &legalTargets,
                       int lastFrom,
                       int lastTo,
                       int kingInCheckSquare);

private:
    std::vector<SquareData> m_squares;
};
