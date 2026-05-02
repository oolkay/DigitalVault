#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "vault_controller.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    app.setOrganizationName("DigitalVault");
    app.setApplicationName("VaultGuard");

    QQmlApplicationEngine engine;

    VaultController controller;
    engine.rootContext()->setContextProperty("vaultController", &controller);

    const QUrl url(u"qrc:/DigitalVault/main.qml"_qs);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}
