#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QTimer>
#include <QImage>
#include <iostream>
#include "GameController.h"
#include "TerminalChess.h"

int main(int argc, char *argv[])
{
    bool forceCli = false;
    bool forceGui = false;
    QString screenshotPath;
    int aiElo = 1500;
    int playerColor = 1; // 1 = White, 2 = Black, 3 = PvP

    for (int i = 1; i < argc; ++i) {
        QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == QStringLiteral("--cli") || arg == QStringLiteral("-c")) {
            forceCli = true;
        } else if (arg == QStringLiteral("--gui") || arg == QStringLiteral("-g")) {
            forceGui = true;
        } else if (arg == QStringLiteral("--screenshot") && i + 1 < argc) {
            screenshotPath = QString::fromLocal8Bit(argv[++i]);
        } else if (arg == QStringLiteral("--elo") && i + 1 < argc) {
            aiElo = QString::fromLocal8Bit(argv[++i]).toInt();
        } else if (arg == QStringLiteral("--black")) {
            playerColor = 2;
        } else if (arg == QStringLiteral("--pvp")) {
            playerColor = 3;
        }
    }

    bool isHeadless = qEnvironmentVariableIsEmpty("DISPLAY") && qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY");

    // If headless and no screenshot requested and GUI not forced, run interactive Terminal Chess
    if ((isHeadless && screenshotPath.isEmpty() && !forceGui) || forceCli) {
        QCoreApplication app(argc, argv);
        return TerminalChess::run(aiElo, playerColor);
    }

    // Otherwise launch Qt6 QML GUI
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Qt6Chess"));
    app.setOrganizationName(QStringLiteral("Qt6Chess"));

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QQmlApplicationEngine engine;

    GameController gameController;
    engine.rootContext()->setContextProperty(QStringLiteral("gameController"), &gameController);
    engine.rootContext()->setContextProperty(QStringLiteral("appController"), &gameController);

    const QUrl url(QStringLiteral("qrc:/qt/qml/Qt6Chess/UI/qml/Main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);

    engine.load(url);

    // If screenshot requested (e.g. under xvfb), capture window and exit
    if (!screenshotPath.isEmpty()) {
        QTimer::singleShot(1500, [&]() {
            if (!engine.rootObjects().isEmpty()) {
                QQuickWindow *win = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
                if (win) {
                    QImage image = win->grabWindow();
                    if (image.save(screenshotPath)) {
                        std::cout << "Successfully saved GUI screenshot to: " << screenshotPath.toStdString() << std::endl;
                    } else {
                        std::cerr << "Failed to save screenshot to: " << screenshotPath.toStdString() << std::endl;
                    }
                }
            }
            app.quit();
        });
    }

    return app.exec();
}
