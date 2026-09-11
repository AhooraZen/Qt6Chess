#pragma once

#include <QObject>
#include <QSoundEffect>
#include <memory>

class SoundManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool soundEnabled READ soundEnabled WRITE setSoundEnabled NOTIFY soundEnabledChanged)

public:
    explicit SoundManager(QObject *parent = nullptr);

    bool soundEnabled() const { return m_soundEnabled; }
    void setSoundEnabled(bool enabled);

public slots:
    void playMove();
    void playCapture();
    void playCheck();
    void playCastle();
    void playEnd();

signals:
    void soundEnabledChanged();

private:
    bool m_soundEnabled = true;
    QSoundEffect m_moveSound;
    QSoundEffect m_captureSound;
    QSoundEffect m_checkSound;
    QSoundEffect m_castleSound;
    QSoundEffect m_endSound;

    void initEffect(QSoundEffect &effect, const QString &sourcePath);
};
