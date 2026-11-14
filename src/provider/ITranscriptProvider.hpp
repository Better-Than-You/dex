#pragma once

#include <QObject>
#include <QString>

namespace dex {

/// Abstract ASR contract. Phase 1 ships only StubTranscriptProvider; Phase 2
/// will add an AudioCapture + WhisperWorker implementation without touching
/// anything downstream of this interface.
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

signals:
    void partialTextChanged(const QString &text);
    void utteranceFinalized(const QString &text);
    void stateChanged(dex::ITranscriptProvider::State state);
};

} // namespace dex
