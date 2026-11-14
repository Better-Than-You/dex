#include "core/AppController.hpp"
#include "core/Config.hpp"
#include "core/IpcServer.hpp"
#include "engine/RuleRouter.hpp"

#include <QCommandLineParser>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QSocketNotifier>

#include <LayerShellQt/Window>

#include <csignal>
#include <cstdio>

#include <fcntl.h>
#include <unistd.h>

using namespace dex;

namespace {

int g_signalPipe[2] = {-1, -1};

void onSigusr1(int)
{
    const char byte = 1;
    [[maybe_unused]] const ssize_t written = ::write(g_signalPipe[1], &byte, 1);
}

/// SIGUSR1 -> TOGGLE without doing non-async-signal-safe work in the handler.
bool installSignalWatcher(AppController *controller)
{
    if (::pipe(g_signalPipe) != 0)
        return false;

    for (const int fd : g_signalPipe) {
        const int flags = ::fcntl(fd, F_GETFL, 0);
        ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }

    auto *notifier = new QSocketNotifier(g_signalPipe[0], QSocketNotifier::Read, controller);
    QObject::connect(notifier, &QSocketNotifier::activated, controller, [controller] {
        char buffer = 0;
        while (::read(g_signalPipe[0], &buffer, sizeof(buffer)) > 0) {
        }
        controller->toggleOverlay();
    });

    std::signal(SIGUSR1, onSigusr1);
    return true;
}

QString resolveFile(const QString &explicitPath, const QString &defaultPath, const QString &fileName)
{
    if (!explicitPath.isEmpty())
        return explicitPath;
    if (QFileInfo::exists(defaultPath))
        return defaultPath;
#ifdef DEX_SOURCE_CONFIG_DIR
    // Development convenience: run straight from the source tree.
    const QString bundled = QStringLiteral(DEX_SOURCE_CONFIG_DIR "/") + fileName;
    if (QFileInfo::exists(bundled))
        return bundled;
#else
    Q_UNUSED(fileName)
#endif
    return defaultPath;
}

void configureLayerShell(QQuickWindow *window, AppController *controller, const Config &config)
{
    if (!window)
        return;
    if (!QGuiApplication::platformName().startsWith(QStringLiteral("wayland"))) {
        qInfo("Not running on Wayland; layer-shell configuration skipped.");
        return;
    }

    auto *layer = LayerShellQt::Window::get(window);
    layer->setLayer(LayerShellQt::Window::LayerOverlay);
    layer->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorTop));
    layer->setExclusiveZone(-1);
    layer->setMargins(QMargins(0, config.ui.marginTop, 0, 0));
    layer->setScope(QStringLiteral("dex"));
    layer->setWantsToBeOnActiveScreen(true);
    layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);

    QObject::connect(controller, &AppController::stateChanged, layer, [controller, layer, window] {
        const bool wantsKeyboard = controller->state() == AppController::Confirming;
        layer->setKeyboardInteractivity(wantsKeyboard
                                            ? LayerShellQt::Window::KeyboardInteractivityOnDemand
                                            : LayerShellQt::Window::KeyboardInteractivityNone);
        if (wantsKeyboard)
            window->requestActivate();
    });
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Dex"));
    QCoreApplication::setApplicationName(QStringLiteral("dex"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Offline, deterministic voice command launcher for Hyprland."));
    parser.addHelpOption();
    parser.addVersionOption();

    const QCommandLineOption configOption(
        {QStringLiteral("c"), QStringLiteral("config")},
        QStringLiteral("Path to config.toml."), QStringLiteral("path"));
    const QCommandLineOption commandsOption(
        QStringLiteral("commands"),
        QStringLiteral("Path to commands.toml."), QStringLiteral("path"));
    const QCommandLineOption socketOption(
        QStringLiteral("socket"),
        QStringLiteral("Unix socket path (default: $XDG_RUNTIME_DIR/dex.sock)."),
        QStringLiteral("path"));
    const QCommandLineOption oneShotOption(
        {QStringLiteral("1"), QStringLiteral("one-shot")},
        QStringLiteral("Show immediately, handle one utterance, then exit."));
    const QCommandLineOption noIpcOption(
        QStringLiteral("no-ipc"), QStringLiteral("Do not start the control socket."));

    parser.addOptions({configOption, commandsOption, socketOption, oneShotOption, noIpcOption});
    parser.process(app);

    const QString configPath = resolveFile(parser.value(configOption),
                                          defaultConfigPath(),
                                          QStringLiteral("config.toml"));
    const QString commandsPath = resolveFile(parser.value(commandsOption),
                                            defaultCommandsPath(),
                                            QStringLiteral("commands.toml"));

    Config config;
    QString configError;
    if (const auto loaded = Config::load(configPath, &configError)) {
        config = *loaded;
        qInfo().noquote() << QStringLiteral("Loaded config from %1").arg(configPath);
    } else {
        qWarning().noquote() << configError << QStringLiteral("— using built-in defaults.");
    }

    if (parser.isSet(oneShotOption))
        config.general.daemon = false;

    RuleRouter router;
    QString commandsError;
    if (router.load(commandsPath, &commandsError))
        qInfo().noquote() << QStringLiteral("Loaded %1 command rules from %2")
                                 .arg(router.rules().size())
                                 .arg(commandsPath);
    else
        qWarning().noquote() << commandsError;

    AppController controller(config, &router);
    AppController::installInstance(&controller);
    installSignalWatcher(&controller);

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::warnings, [](const QList<QQmlError> &warnings) {
        for (const QQmlError &warning : warnings)
            fprintf(stderr, "[qml] %s\n", qPrintable(warning.toString()));
    });
    engine.loadFromModule("Dex", "Main");
    if (engine.rootObjects().isEmpty())
        return 1;

    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    configureLayerShell(window, &controller, config);

    IpcServer ipc;
    if (!parser.isSet(noIpcOption)) {
        const QString socketPath = parser.isSet(socketOption)
            ? parser.value(socketOption)
            : IpcServer::defaultSocketPath();

        QString ipcError;
        if (ipc.listen(socketPath, &ipcError)) {
            qInfo().noquote() << QStringLiteral("Listening on %1").arg(socketPath);
        } else {
            qWarning().noquote() << ipcError;
        }

        QObject::connect(&ipc, &IpcServer::showRequested, &controller, &AppController::handleShow);
        QObject::connect(&ipc, &IpcServer::hideRequested, &controller, &AppController::handleHide);
        QObject::connect(&ipc, &IpcServer::toggleRequested, &controller, &AppController::handleToggle);
        QObject::connect(&ipc, &IpcServer::partialReceived, &controller, &AppController::handlePartial);
        QObject::connect(&ipc, &IpcServer::finalReceived, &controller, &AppController::handleFinal);
        QObject::connect(&ipc, &IpcServer::confirmRequested, &controller, &AppController::handleConfirm);
        QObject::connect(&ipc, &IpcServer::cancelRequested, &controller, &AppController::handleCancel);
        QObject::connect(&ipc, &IpcServer::quitRequested, &controller, &AppController::handleQuit);
    }

    return app.exec();
}
