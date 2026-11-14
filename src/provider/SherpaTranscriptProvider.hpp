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

/// Phase 2 ASR provider: 16 kHz capture -> Silero VAD -> sherpa-onnx offline
/// recognizer running on a dedicated thread.
///
/// The model is offline (non-streaming), so partial transcripts are produced
/// by periodically re-decoding the active VAD buffer, the same "simulated
/// streaming" technique used by sherpa-onnx's own examples. A completed VAD
/// segment triggers a final decode and `utteranceFinalized`.
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
