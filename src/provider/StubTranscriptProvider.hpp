#pragma once

#include "provider/ITranscriptProvider.hpp"

namespace dex {

/// Phase 1 transcript source. Text is injected over the Unix socket or by
/// tests; no microphone or whisper.cpp involvement.
class StubTranscriptProvider : public ITranscriptProvider
{
    Q_OBJECT

public:
    explicit StubTranscriptProvider(QObject *parent = nullptr);

    void start() override;
    void stop() override;

public slots:
    void feedPartial(const QString &text);
    void feedFinal(const QString &text);
};

} // namespace dex
