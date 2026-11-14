#pragma once

#include <QObject>
#include <QString>

namespace dex {

/// Abstract ASR contract. `StubTranscriptProvider` (text injection) and
/// `SherpaTranscriptProvider` (live audio) both implement it, so everything
/// downstream (IPC, command engine, QML) is independent of the audio backend.
class ITranscriptProvider : public QObject
{
    Q_OBJECT

public:
    enum class State { Idle, Listening, Finalizing, Error };
    Q_ENUM(State)

    explicit ITranscriptProvider(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    ~ITranscriptProvider() override = default;

    virtual void start() = 0;
    virtual void stop() = 0;

    /// Hot-switches the ASR model profile. Returns false when the backend
    /// cannot change models while running.
    virtual bool setModelProfile(const QString &profile)
    {
        Q_UNUSED(profile)
        return false;
    }

signals:
    void partialTextChanged(const QString &text);
    void utteranceFinalized(const QString &text);
    void stateChanged(dex::ITranscriptProvider::State state);
    void levelChanged(qreal level);
    void errorTextChanged(const QString &error);
};

} // namespace dex
