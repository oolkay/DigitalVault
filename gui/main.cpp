#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <csignal>
#include <unistd.h>

#include "vault_controller.h"

static void signalHandler(int sig)
{
    if (g_vaultController && access("/dev/mapper/digitalvault", F_OK) == 0) {
        VaultController::posixEmergencyClose(
            g_vaultController->cachedVaultManagerBin(),
            g_vaultController->cachedMountPoint());
    }
    _exit(128 + sig);
}

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    app.setOrganizationName("DigitalVault");
    app.setApplicationName("VaultGuard");

    QQmlApplicationEngine engine;

    VaultController controller;
    engine.rootContext()->setContextProperty("vaultController", &controller);

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    signal(SIGHUP, signalHandler);

    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&engine]() {
        if (g_vaultController && g_vaultController->vaultRuntimeOpen())
            g_vaultController->emergencyClose();
        // Tear down QML while vaultController context property is still valid
        // (avoids "Cannot read property ... of null" during engine shutdown).
        const QList<QObject *> roots = engine.rootObjects();
        for (QObject *root : roots)
            delete root;
    });

    const QUrl url(u"qrc:/DigitalVault/main.qml"_qs);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}
