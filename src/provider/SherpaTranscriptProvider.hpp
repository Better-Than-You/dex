#pragma once

#include "core/Config.hpp"
#include "provider/AudioCapture.hpp"
#include "provider/ITranscriptProvider.hpp"
#include "provider/ModelLayout.hpp"

#include <QString>
#include <QThread>

#include <memory>

namespace dex {

class SherpaWorker;

/// Live ASR provider: 16 kHz capture -> Silero VAD -> NVIDIA Parakeet TDT 0.6B
/// v2 (INT8), decoded single-shot once per complete VAD utterance.
///
/// There is no simulated streaming: `partialTextChanged` is never emitted
/// during speech, and `utteranceFinalized` fires exactly once when Silero VAD
/// observes `asr.silence_timeout_ms` of trailing silence. The UI shows
/// "Listening…" with the live RMS pulse until then.
///
/// `StubTranscriptProvider` remains the alternative backend for headless
/// testing (`--stub`, `asr.provider = "stub"`).
class SherpaTranscriptProvider : public ITranscriptProvider
{
    Q_OBJECT

public:
    explicit SherpaTranscriptProvider(AsrConfig config, QObject *parent = nullptr);
    ~SherpaTranscriptProvider() override;

    void start() override;
    void stop() override;
    bool setModelProfile(const QString &profile) override;

    /// Resolves and validates the model files without loading them. Called
    /// by ProviderFactory before the provider is handed to the controller.
    bool prepare(QString *error = nullptr);

    ModelLayout layout() const { return m_layout; }
    const AsrConfig &config() const { return m_config; }

signals:
    void modelLoaded(const QString &profile);

private:
    AsrConfig m_config;
    ModelLayout m_layout;
    std::unique_ptr<AudioCapture> m_audio;
    SherpaWorker *m_worker = nullptr;
    QThread m_thread;
    bool m_prepared = false;
    bool m_started = false;
};

} // namespace dex
