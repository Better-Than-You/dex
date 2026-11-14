#include "core/AppController.hpp"

#include "engine/TextNormalizer.hpp"

#include <QCoreApplication>
#include <QJSEngine>
#include <QQmlEngine>

#include <utility>

namespace dex {
namespace {
AppController *g_instance = nullptr;

RuleRouter &fallbackRouter()
{
    static RuleRouter router;
    return router;
}
} // namespace

AppController::AppController(Config config, RuleRouter *router, QObject *parent)
    : QObject(parent)
    , m_config(std::move(config))
    , m_router(router ? *router : fallbackRouter())
    , m_engine(m_router, m_config.general.confirmByDefault)
{
    m_autoHideTimer.setSingleShot(true);
    m_autoHideTimer.setInterval(m_config.general.autoHideMs);
    connect(&m_autoHideTimer, &QTimer::timeout, this, [this] {
        if (m_state == Confirming)
            return;
        setOverlayVisible(false);
        setState(Hidden);
        if (!m_config.general.daemon)
            QCoreApplication::quit();
    });

    connect(&m_provider, &ITranscriptProvider::partialTextChanged,
            this, &AppController::handlePartial);
    connect(&m_provider, &ITranscriptProvider::utteranceFinalized,
            this, &AppController::handleFinal);

    m_provider.start();

    if (!m_config.general.daemon) {
        setOverlayVisible(true);
        setState(Listening);
    }
}

AppController::~AppController() = default;

AppController *AppController::create(QQmlEngine *, QJSEngine *)
{
    if (!g_instance)
        return nullptr;
    QQmlEngine::setObjectOwnership(g_instance, QQmlEngine::CppOwnership);
    return g_instance;
}

void AppController::installInstance(AppController *controller)
{
    g_instance = controller;
}

void AppController::setState(State state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged();
}

void AppController::setOverlayVisible(bool visible)
{
    if (m_overlayVisible == visible)
        return;
    m_overlayVisible = visible;
    emit overlayVisibleChanged();
}

void AppController::scheduleAutoHide()
{
    if (m_config.general.autoHideMs <= 0)
        return;
    m_autoHideTimer.start(m_config.general.autoHideMs);
}

void AppController::clearAutoHide()
{
    m_autoHideTimer.stop();
}

void AppController::resetTransientState()
{
    m_expressionText.clear();
    emit expressionTextChanged();
    m_resultText.clear();
    emit resultTextChanged();
    if (m_resultCopied) {
        m_resultCopied = false;
        emit resultCopiedChanged();
    }
    m_errorText.clear();
    m_statusTitle.clear();
    m_statusDetail.clear();
    m_confirmLabel.clear();
    emit statusChanged();
}

void AppController::handleShow()
{
    showOverlay();
}

void AppController::handleHide()
{
    hideOverlay();
}

void AppController::handleToggle()
{
    toggleOverlay();
}

void AppController::handlePartial(const QString &text)
{
    if (m_state == Confirming)
        return;
    setOverlayVisible(true);
    if (m_state == Hidden || m_state == Error)
        setState(Listening);
    m_partialText = text;
    emit partialTextChanged();
}

void AppController::handleFinal(const QString &text)
{
    processUtterance(text);
}

void AppController::handleConfirm()
{
    confirmAction();
}

void AppController::handleCancel()
{
    cancelAction();
}

void AppController::handleQuit()
{
    quit();
}

void AppController::showOverlay()
{
    setOverlayVisible(true);
    if (m_state == Hidden)
        setState(Listening);
}

void AppController::hideOverlay()
{
    clearAutoHide();
    m_pending.reset();
    m_partialText.clear();
    emit partialTextChanged();
    setOverlayVisible(false);
    setState(Hidden);
    if (!m_config.general.daemon)
        QCoreApplication::quit();
}

void AppController::toggleOverlay()
{
    if (m_overlayVisible)
        hideOverlay();
    else
        showOverlay();
}

void AppController::submitUtterance(const QString &text)
{
    m_provider.feedFinal(text);
}

void AppController::processUtterance(const QString &text)
{
    clearAutoHide();
    resetTransientState();
    m_pending.reset();

    m_finalText = text;
    emit finalTextChanged();
    m_partialText.clear();
    emit partialTextChanged();

    setOverlayVisible(true);

    const CommandEngine::Outcome outcome = m_engine.process(text);
    switch (outcome.kind) {
    case CommandEngine::OutcomeKind::Math: {
        m_expressionText = outcome.expression;
        emit expressionTextChanged();
        m_resultText = outcome.result;
        emit resultTextChanged();

        QString clipboardError;
        m_resultCopied = ActionDispatcher::copyToClipboard(outcome.result, &clipboardError);
        emit resultCopiedChanged();
        if (!m_resultCopied)
            m_errorText = clipboardError;

        m_statusTitle = QStringLiteral("Result");
        m_statusDetail = outcome.expression;
        emit statusChanged();
        setState(MathResult);
        scheduleAutoHide();
        break;
    }
    case CommandEngine::OutcomeKind::Action:
        dispatch(outcome.route);
        break;
    case CommandEngine::OutcomeKind::Confirmation: {
        m_pending = outcome.route;
        m_statusTitle = outcome.route.label;
        m_statusDetail = outcome.route.detail;
        m_confirmLabel = QStringLiteral("Run %1").arg(outcome.route.label);
        emit statusChanged();
        setState(Confirming);
        // Never auto-hide while waiting for the confirmation gate.
        break;
    }
    case CommandEngine::OutcomeKind::None:
        m_errorText = QStringLiteral("No matching command for “%1”.").arg(outcome.normalized);
        m_statusTitle = QStringLiteral("Not understood");
        m_statusDetail.clear();
        emit statusChanged();
        setState(Error);
        scheduleAutoHide();
        break;
    }
}

void AppController::dispatch(const RouteResult &route)
{
    m_statusTitle = route.label;
    m_statusDetail = route.detail.isEmpty() ? route.program : route.detail;
    m_confirmLabel = QStringLiteral("Run %1").arg(route.label);
    emit statusChanged();

    QString error;
    if (ActionDispatcher::launch(route.program, route.args, &error)) {
        m_errorText.clear();
        setState(CommandExecuted);
    } else {
        m_errorText = error;
        setState(Error);
    }
    scheduleAutoHide();
}

void AppController::confirmAction()
{
    if (m_state != Confirming || !m_pending)
        return;
    const RouteResult route = *m_pending;
    m_pending.reset();
    dispatch(route);
}

void AppController::cancelAction()
{
    m_pending.reset();
    clearAutoHide();
    setOverlayVisible(false);
    setState(Hidden);
    if (!m_config.general.daemon)
        QCoreApplication::quit();
}

void AppController::quit()
{
    clearAutoHide();
    m_provider.stop();
    QCoreApplication::quit();
}

} // namespace dex
