#pragma once

#include "provider/ITranscriptProvider.hpp"

namespace dex {

/// Test / headless transcript source. Text is injected over the Unix socket
/// or by tests; no microphone or ASR model is involved.
class StubTranscriptProvider : public ITranscriptProvider
{
    Q_OBJECT

public:
    explicit StubTranscriptProvider(QObject *parent = nullptr);

    void start() override;
    void stop() override;
    bool setModelProfile(const QString &profile) override;

    QString modelProfile() const { return m_profile; }

public slots:
    void feedPartial(const QString &text);
    void feedFinal(const QString &text);

private:
    QString m_profile = QStringLiteral("stub");
};

} // namespace dex
