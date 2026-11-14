#pragma once

#include "core/Config.hpp"
#include "engine/ActionDispatcher.hpp"
#include "engine/CommandEngine.hpp"
#include "engine/RuleRouter.hpp"
#include "provider/ITranscriptProvider.hpp"

#include <QObject>
#include <QString>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

#include <memory>
#include <optional>

class QJSEngine;
class QQmlEngine;

namespace dex {

class LlamaMathTranslator;

/// Presentation-facing state machine. QML binds to its properties; every
/// decision (normalisation, maths, routing) happens in C++.
class AppController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString partialText READ partialText NOTIFY partialTextChanged)
    Q_PROPERTY(QString finalText READ finalText NOTIFY finalTextChanged)
    Q_PROPERTY(QString expressionText READ expressionText NOTIFY expressionTextChanged)
    Q_PROPERTY(QString resultText READ resultText NOTIFY resultTextChanged)
    Q_PROPERTY(bool resultCopied READ resultCopied NOTIFY resultCopiedChanged)
    Q_PROPERTY(QString statusTitle READ statusTitle NOTIFY statusChanged)
    Q_PROPERTY(QString statusDetail READ statusDetail NOTIFY statusChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY statusChanged)
    Q_PROPERTY(QString confirmLabel READ confirmLabel NOTIFY statusChanged)
    Q_PROPERTY(bool overlayVisible READ overlayVisible NOTIFY overlayVisibleChanged)
    Q_PROPERTY(bool daemonMode READ daemonMode CONSTANT)
    Q_PROPERTY(int windowWidth READ windowWidth CONSTANT)
    Q_PROPERTY(int marginTop READ marginTop CONSTANT)
    Q_PROPERTY(int autoHideMs READ autoHideMs CONSTANT)
    Q_PROPERTY(qreal audioLevel READ audioLevel NOTIFY audioLevelChanged)
    Q_PROPERTY(QString modelProfile READ modelProfile NOTIFY modelProfileChanged)
    Q_PROPERTY(QString providerName READ providerName CONSTANT)

public:
    enum State {
        Hidden = 0,
        Listening,
        Processing,
        MathResult,
        CommandExecuted,
        Confirming,
        Error,
    };
    Q_ENUM(State)

    AppController(Config config, RuleRouter *router, bool forceStub = false,
                  QObject *parent = nullptr);
    ~AppController() override;

    /// QML_SINGLETON factory. The instance is created by main().
    static AppController *create(QQmlEngine *engine, QJSEngine *scriptEngine);
    static void installInstance(AppController *controller);

    State state() const { return m_state; }
    QString partialText() const { return m_partialText; }
    QString finalText() const { return m_finalText; }
    QString expressionText() const { return m_expressionText; }
    QString resultText() const { return m_resultText; }
    bool resultCopied() const { return m_resultCopied; }
    QString statusTitle() const { return m_statusTitle; }
    QString statusDetail() const { return m_statusDetail; }
    QString errorText() const { return m_errorText; }
    QString confirmLabel() const { return m_confirmLabel; }
    bool overlayVisible() const { return m_overlayVisible; }
    bool daemonMode() const { return m_config.general.daemon; }
    int windowWidth() const { return m_config.ui.width; }
    int marginTop() const { return m_config.ui.marginTop; }
    int autoHideMs() const { return m_config.general.autoHideMs; }
    qreal audioLevel() const { return m_audioLevel; }
    QString modelProfile() const { return m_modelProfile; }
    QString providerName() const { return m_providerName; }

public slots:
    /// IPC plumbing (also used by tests).
    void handleShow();
    void handleHide();
    void handleToggle();
    void handlePartial(const QString &text);
    void handleFinal(const QString &text);
    void handleConfirm();
    void handleCancel();
    void handleQuit();
    void handleSetModel(const QString &profile);

    Q_INVOKABLE void showOverlay();
    Q_INVOKABLE void hideOverlay();
    Q_INVOKABLE void toggleOverlay();
    Q_INVOKABLE void submitUtterance(const QString &text);
    Q_INVOKABLE void confirmAction();
    Q_INVOKABLE void cancelAction();
    Q_INVOKABLE void quit();

signals:
    void stateChanged();
    void partialTextChanged();
    void finalTextChanged();
    void expressionTextChanged();
    void resultTextChanged();
    void resultCopiedChanged();
    void statusChanged();
    void overlayVisibleChanged();
    void audioLevelChanged();
    void modelProfileChanged();

private:
    void setState(State state);
    void processUtterance(const QString &text);
    void dispatch(const RouteResult &route);
    void resetTransientState();
    void scheduleAutoHide();
    void clearAutoHide();
    void setOverlayVisible(bool visible);
    void handleProviderError(const QString &error);

    Config m_config;
    RuleRouter &m_router;
    CommandEngine m_engine;
    std::unique_ptr<LlamaMathTranslator> m_mathTranslator;
    std::unique_ptr<ITranscriptProvider> m_provider;
    QTimer m_autoHideTimer;

    State m_state = Hidden;
    QString m_partialText;
    QString m_finalText;
    QString m_expressionText;
    QString m_resultText;
    QString m_statusTitle;
    QString m_statusDetail;
    QString m_errorText;
    QString m_confirmLabel;
    QString m_modelProfile;
    QString m_providerName;
    QString m_providerError;
    qreal m_audioLevel = 0.0;
    bool m_resultCopied = false;
    bool m_overlayVisible = false;
    std::optional<RouteResult> m_pending;
};

} // namespace dex
