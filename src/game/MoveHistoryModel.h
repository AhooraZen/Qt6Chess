#pragma once

#include <QAbstractListModel>
#include <vector>

struct MoveTurn {
    int turnNumber = 1;
    QString whiteSan;
    QString blackSan;
    QString whiteUci;
    QString blackUci;
    QString whiteFen;
    QString blackFen;
};

class MoveHistoryModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int currentPly READ currentPly NOTIFY currentPlyChanged)
    Q_PROPERTY(int totalPly READ totalPly NOTIFY countChanged)

public:
    enum MoveRoles {
        TurnNumberRole = Qt::UserRole + 1,
        WhiteSanRole,
        BlackSanRole,
        WhiteUciRole,
        BlackUciRole,
        IsWhiteSelectedRole,
        IsBlackSelectedRole
    };
    Q_ENUM(MoveRoles)

    explicit MoveHistoryModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int currentPly() const { return m_currentPly; }
    int totalPly() const { return m_totalPly; }

    void addMove(bool isWhite, const QString &san, const QString &uci, const QString &fen);
    void clear();
    void setCurrentPly(int ply);

    Q_INVOKABLE QString exportPgn(const QString &whitePlayer, const QString &blackPlayer, const QString &event, const QString &result) const;
    Q_INVOKABLE QString getFenAtPly(int ply) const;

signals:
    void currentPlyChanged();
    void countChanged();

private:
    std::vector<MoveTurn> m_turns;
    int m_currentPly = 0;
    int m_totalPly = 0;
    std::vector<QString> m_fenHistory; // 0 = startpos
};
