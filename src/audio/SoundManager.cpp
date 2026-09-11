#include "SoundManager.h"
#include <QUrl>

SoundManager::SoundManager(QObject *parent)
    : QObject(parent)
{
    initEffect(m_moveSound, QStringLiteral("qrc:/qt/qml/Qt6Chess/UI/resources/sounds/move.wav"));
    initEffect(m_captureSound, QStringLiteral("qrc:/qt/qml/Qt6Chess/UI/resources/sounds/capture.wav"));
    initEffect(m_checkSound, QStringLiteral("qrc:/qt/qml/Qt6Chess/UI/resources/sounds/check.wav"));
    initEffect(m_castleSound, QStringLiteral("qrc:/qt/qml/Qt6Chess/UI/resources/sounds/castle.wav"));
    initEffect(m_endSound, QStringLiteral("qrc:/qt/qml/Qt6Chess/UI/resources/sounds/end.wav"));
}

void SoundManager::initEffect(QSoundEffect &effect, const QString &sourcePath)
{
    effect.setSource(QUrl(sourcePath));
    effect.setVolume(0.85f);
}

void SoundManager::setSoundEnabled(bool enabled)
{
    if (m_soundEnabled != enabled) {
        m_soundEnabled = enabled;
        emit soundEnabledChanged();
    }
}

void SoundManager::playMove()
{
    if (m_soundEnabled && m_moveSound.isLoaded()) {
        m_moveSound.play();
    }
}

void SoundManager::playCapture()
{
    if (m_soundEnabled && m_captureSound.isLoaded()) {
        m_captureSound.play();
    }
}

void SoundManager::playCheck()
{
    if (m_soundEnabled && m_checkSound.isLoaded()) {
        m_checkSound.play();
    }
}

void SoundManager::playCastle()
{
    if (m_soundEnabled && m_castleSound.isLoaded()) {
        m_castleSound.play();
    }
}

void SoundManager::playEnd()
{
    if (m_soundEnabled && m_endSound.isLoaded()) {
        m_endSound.play();
    }
}
