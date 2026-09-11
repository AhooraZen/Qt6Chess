#pragma once

#include <QObject>
#include <QTimer>
#include <QElapsedTimer>

class ChessClock : public QObject {
    Q_OBJECT
    Q_PROPERTY(qint64 whiteTimeMs READ whiteTimeMs NOTIFY timeChanged)
    Q_PROPERTY(qint64 blackTimeMs READ blackTimeMs NOTIFY timeChanged)
    Q_PROPERTY(QString whiteTimeString READ whiteTimeString NOTIFY timeChanged)
    Q_PROPERTY(QString blackTimeString READ blackTimeString NOTIFY timeChanged)
    Q_PROPERTY(bool isRunning READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(bool isUnlimited READ isUnlimited NOTIFY settingsChanged)
    Q_PROPERTY(int activeSide READ activeSide NOTIFY activeSideChanged)

public:
    enum Side { None = 0, White = 1, Black = 2 };
    Q_ENUM(Side)

    explicit ChessClock(QObject *parent = nullptr);

    qint64 whiteTimeMs() const { return m_whiteTimeMs; }
    qint64 blackTimeMs() const { return m_blackTimeMs; }
    QString whiteTimeString() const;
    QString blackTimeString() const;
    bool isRunning() const { return m_isRunning; }
    bool isUnlimited() const { return m_isUnlimited; }
    int activeSide() const { return m_activeSide; }

public slots:
    void setTimeControl(int baseSeconds, int incrementSeconds);
    void setUnlimited();
    void start(Side side = White);
    void pause();
    void switchTurn();
    void reset();

signals:
    void timeChanged();
    void runningChanged();
    void settingsChanged();
    void activeSideChanged();
    void timeOut(int side);

private slots:
    void onTick();

private:
    int m_baseSeconds = 180; // 3 min default Blitz
    int m_incrementSeconds = 2; // 2s increment
    bool m_isUnlimited = false;
    bool m_isRunning = false;
    Side m_activeSide = None;

    qint64 m_whiteTimeMs = 180000;
    qint64 m_blackTimeMs = 180000;

    QTimer m_tickTimer;
    QElapsedTimer m_elapsedTimer;

    QString formatTime(qint64 ms) const;
};
