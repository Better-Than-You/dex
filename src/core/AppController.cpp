#include "core/AppController.hpp"

#include "engine/TextNormalizer.hpp"
#include "engine/LlamaMathTranslator.hpp"
#include "provider/ModelLayout.hpp"
#include "provider/ProviderFactory.hpp"
#include "provider/StubTranscriptProvider.hpp"

#include <QCoreApplication>
#include <QDebug>
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

AppController::AppController(Config config, RuleRouter *router, bool forceStub, QObject *parent)
    : QObject(parent)
    , m_config(std::move(config))
    , m_router(router ? *router : fallbackRouter())
    , m_engine(m_router, m_config.general.confirmByDefault)
{
    m_mathTranslator = std::make_unique<LlamaMathTranslator>(m_config.mathSlm, this);
    m_engine.setMathTranslator(m_mathTranslator.get());

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

    const ProviderSelection selection = ProviderFactory::resolve(m_config.asr, forceStub);
    m_modelProfile =
        selection.kind == ProviderKind::Stub ? QStringLiteral("stub") : QStringLiteral("parakeet");
    m_providerName =
        selection.kind == ProviderKind::Stub ? QStringLiteral("stub") : QStringLiteral("sherpa");

    QString providerError;
    m_provider = ProviderFactory::create(m_config.asr, forceStub, nullptr, &providerError);
    if (!m_provider) {
        qWarning().noquote() << QStringLiteral("ASR unavailable: %1 — falling back to stub.")
                                    .arg(providerError);
        m_providerError = providerError;
        m_provider = std::make_unique<StubTranscriptProvider>();
        m_providerName = QStringLiteral("stub");
        m_modelProfile = QStringLiteral("stub");
    }

    connect(m_provider.get(), &ITranscriptProvider::partialTextChanged,
            this, &AppController::handlePartial);
    connect(m_provider.get(), &ITranscriptProvider::utteranceFinalized,
            this, &AppController::handleFinal);
    connect(m_provider.get(), &ITranscriptProvider::levelChanged, this, [this](qreal level) {
        if (qFuzzyCompare(m_audioLevel + 1.0, level + 1.0))
            return;
        m_audioLevel = level;
        emit audioLevelChanged();
    });
    connect(m_provider.get(), &ITranscriptProvider::errorTextChanged,
            this, &AppController::handleProviderError);

    m_provider->start();

    if (!m_config.general.daemon) {
        setOverlayVisible(true);
        setState(Listening);
    }
}

AppController::~AppController()
{
    if (m_provider)
        m_provider->stop();
}

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

void AppController::handleProviderError(const QString &error)
{
    qWarning().noquote() << QStringLiteral("ASR: %1").arg(error);
    m_providerError = error;
    m_errorText = error;

    if (m_state == Confirming) {
        emit statusChanged();
        return;
    }

    if (m_overlayVisible) {
        m_statusTitle = QStringLiteral("ASR unavailable");
        m_statusDetail.clear();
        emit statusChanged();
        setState(Error);
        scheduleAutoHide();
    } else {
        emit statusChanged();
    }
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

void AppController::handleSetModel(const QString &profile)
{
    const QString canonical = ModelLayout::canonicalProfile(profile);
    if (canonical.isEmpty()) {
        m_errorText =
            QStringLiteral("Unknown ASR profile \"%1\" (Dex only supports parakeet or stub).")
                .arg(profile.trimmed());
        emit statusChanged();
        return;
    }

    if (canonical == QStringLiteral("stub") && !dynamic_cast<StubTranscriptProvider *>(m_provider.get())) {
        if (m_provider)
            m_provider->stop();
        m_provider = std::make_unique<StubTranscriptProvider>();
        m_providerName = QStringLiteral("stub");
        connect(m_provider.get(), &ITranscriptProvider::partialTextChanged,
                this, &AppController::handlePartial);
        connect(m_provider.get(), &ITranscriptProvider::utteranceFinalized,
                this, &AppController::handleFinal);
        connect(m_provider.get(), &ITranscriptProvider::levelChanged, this, [this](qreal level) {
            if (qFuzzyCompare(m_audioLevel + 1.0, level + 1.0))
                return;
            m_audioLevel = level;
            emit audioLevelChanged();
        });
        connect(m_provider.get(), &ITranscriptProvider::errorTextChanged,
                this, &AppController::handleProviderError);
        m_provider->start();
    } else if (canonical == QStringLiteral("parakeet") && dynamic_cast<StubTranscriptProvider *>(m_provider.get())) {
        QString error;
        auto newProvider = ProviderFactory::create(m_config.asr, false, nullptr, &error);
        if (!newProvider) {
            m_errorText = QStringLiteral("Cannot switch to Parakeet: %1").arg(error);
            emit statusChanged();
            return;
        }
        m_provider->stop();
        m_provider = std::move(newProvider);
        m_providerName = QStringLiteral("sherpa");
        connect(m_provider.get(), &ITranscriptProvider::partialTextChanged,
                this, &AppController::handlePartial);
        connect(m_provider.get(), &ITranscriptProvider::utteranceFinalized,
                this, &AppController::handleFinal);
        connect(m_provider.get(), &ITranscriptProvider::levelChanged, this, [this](qreal level) {
            if (qFuzzyCompare(m_audioLevel + 1.0, level + 1.0))
                return;
            m_audioLevel = level;
            emit audioLevelChanged();
        });
        connect(m_provider.get(), &ITranscriptProvider::errorTextChanged,
                this, &AppController::handleProviderError);
        m_provider->start();
    } else if (m_provider && !m_provider->setModelProfile(canonical)) {
        if (m_errorText.isEmpty()) {
            m_errorText =
                QStringLiteral("Cannot switch to the \"%1\" ASR model.").arg(canonical);
        }
        emit statusChanged();
        return;
    }

    if (m_modelProfile != canonical) {
        m_modelProfile = canonical;
        emit modelProfileChanged();
    }
    m_errorText.clear();
    m_statusTitle = QStringLiteral("Model: %1").arg(canonical);
    m_statusDetail = QStringLiteral("Active recognizer: %1").arg(canonical);
    emit statusChanged();
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
    handleFinal(text);
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
    if (m_provider)
        m_provider->stop();
    QCoreApplication::quit();
}

} // namespace dex
