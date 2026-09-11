#include "ChessClock.h"

ChessClock::ChessClock(QObject *parent)
    : QObject(parent)
{
    m_tickTimer.setInterval(50); // 20 updates per second for smooth countdown
    connect(&m_tickTimer, &QTimer::timeout, this, &ChessClock::onTick);
}

void ChessClock::setTimeControl(int baseSeconds, int incrementSeconds)
{
    m_baseSeconds = baseSeconds;
    m_incrementSeconds = incrementSeconds;
    m_isUnlimited = false;
    reset();
    emit settingsChanged();
}

void ChessClock::setUnlimited()
{
    m_isUnlimited = true;
    m_isRunning = false;
    m_tickTimer.stop();
    emit settingsChanged();
    emit runningChanged();
    emit timeChanged();
}

void ChessClock::reset()
{
    pause();
    m_whiteTimeMs = static_cast<qint64>(m_baseSeconds) * 1000;
    m_blackTimeMs = static_cast<qint64>(m_baseSeconds) * 1000;
    m_activeSide = None;
    emit timeChanged();
    emit activeSideChanged();
}

void ChessClock::start(Side side)
{
    if (m_isUnlimited) return;

    m_activeSide = side;
    m_isRunning = true;
    m_elapsedTimer.start();
    m_tickTimer.start();

    emit activeSideChanged();
    emit runningChanged();
}

void ChessClock::pause()
{
    if (m_isRunning) {
        onTick(); // Flush elapsed delta
        m_isRunning = false;
        m_tickTimer.stop();
        emit runningChanged();
    }
}

void ChessClock::switchTurn()
{
    if (m_isUnlimited) return;

    if (m_isRunning) {
        onTick(); // Flush time spent

        // Apply increment to side that just finished move
        if (m_activeSide == White) {
            m_whiteTimeMs += static_cast<qint64>(m_incrementSeconds) * 1000;
            m_activeSide = Black;
        } else if (m_activeSide == Black) {
            m_blackTimeMs += static_cast<qint64>(m_incrementSeconds) * 1000;
            m_activeSide = White;
        }

        m_elapsedTimer.restart();
        emit timeChanged();
        emit activeSideChanged();
    } else {
        if (m_activeSide == White) m_activeSide = Black;
        else if (m_activeSide == Black) m_activeSide = White;
        emit activeSideChanged();
    }
}

void ChessClock::onTick()
{
    if (!m_isRunning || m_isUnlimited) return;

    qint64 elapsed = m_elapsedTimer.restart();
    if (m_activeSide == White) {
        m_whiteTimeMs = std::max<qint64>(0, m_whiteTimeMs - elapsed);
        if (m_whiteTimeMs == 0) {
            pause();
            emit timeOut(White);
        }
    } else if (m_activeSide == Black) {
        m_blackTimeMs = std::max<qint64>(0, m_blackTimeMs - elapsed);
        if (m_blackTimeMs == 0) {
            pause();
            emit timeOut(Black);
        }
    }
    emit timeChanged();
}

QString ChessClock::formatTime(qint64 ms) const
{
    if (m_isUnlimited) return QStringLiteral("--:--");

    qint64 totalSeconds = ms / 1000;
    int minutes = static_cast<int>(totalSeconds / 60);
    int seconds = static_cast<int>(totalSeconds % 60);

    if (minutes < 1 && totalSeconds < 20) {
        int tenths = static_cast<int>((ms % 1000) / 100);
        return QString("%1:%2.%3")
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'))
            .arg(tenths);
    }

    return QString("%1:%2")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
}

QString ChessClock::whiteTimeString() const
{
    return formatTime(m_whiteTimeMs);
}

QString ChessClock::blackTimeString() const
{
    return formatTime(m_blackTimeMs);
}
