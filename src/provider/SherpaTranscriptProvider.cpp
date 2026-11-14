#include "provider/SherpaTranscriptProvider.hpp"

#include <QByteArray>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QLoggingCategory>
#include <QMetaObject>
#include <QTimer>

#include <sherpa-onnx/c-api/c-api.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace dex {
Q_LOGGING_CATEGORY(dexAsr, "dex.asr", QtInfoMsg)

namespace {
constexpr int SampleRate = 16000;
constexpr int VadWindowSize = 512;
constexpr int TickIntervalMs = 30;
constexpr float MaxSpeechSeconds = 30.0f;
constexpr float MinSpeechSeconds = 0.25f;
constexpr int MaxActiveSamples = SampleRate * 30;
constexpr int PreRollSamples = SampleRate * 3 / 5; // 600 ms before VAD detection
constexpr int PartialSilenceMs = 350; // terminal silence for partial decodes
} // namespace

/// All sherpa-onnx objects and the decode loop live here, on a worker thread.
class SherpaWorker : public QObject
{
    Q_OBJECT

public:
    explicit SherpaWorker(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    ~SherpaWorker() override
    {
        releaseModels();
    }

    void initialize(AsrConfig config, ModelLayout layout, AudioCapture::Buffer *buffer)
    {
        releaseModels();
        m_config = std::move(config);
        m_layout = std::move(layout);
        m_buffer = buffer;
        m_reportedDrops = 0;
        m_partialClock.start();

        if (!m_buffer) {
            emit stateChanged(ITranscriptProvider::State::Error);
            return;
        }

        if (!ensureTimer())
            return;

        if (!loadModels()) {
            m_buffer = nullptr;
            return;
        }

        m_timer->start(TickIntervalMs);
        emit stateChanged(ITranscriptProvider::State::Listening);
    }

    void reload(AsrConfig config, ModelLayout layout)
    {
        if (m_timer)
            m_timer->stop();
        releaseModels();

        m_config = std::move(config);
        m_layout = std::move(layout);
        resetRecognition();

        // Drop audio captured for the previous model so it is not decoded by
        // the new one.
        drainBuffer();

        if (!ensureTimer())
            return;
        if (!loadModels())
            return;

        m_timer->start(TickIntervalMs);
        emit stateChanged(ITranscriptProvider::State::Listening);
        emit modelLoaded(m_layout.profile);
    }

    void shutdown()
    {
        if (m_timer)
            m_timer->stop();
        releaseModels();
        m_buffer = nullptr;
        resetRecognition();
        emit stateChanged(ITranscriptProvider::State::Idle);
    }

signals:
    void partialTextChanged(const QString &text);
    void utteranceFinalized(const QString &text);
    void stateChanged(dex::ITranscriptProvider::State state);
    void errorTextChanged(const QString &error);
    void modelLoaded(const QString &profile);

private:
    bool ensureTimer()
    {
        if (m_timer)
            return true;
        m_timer = new QTimer(this);
        m_timer->setTimerType(Qt::CoarseTimer);
        connect(m_timer, &QTimer::timeout, this, &SherpaWorker::tick);
        return true;
    }

    bool loadModels()
    {
        const QByteArray encoder = m_layout.encoder.toUtf8();
        const QByteArray decoder = m_layout.decoder.toUtf8();
        const QByteArray joiner = m_layout.joiner.toUtf8();
        const QByteArray tokens = m_layout.tokens.toUtf8();
        const QByteArray vad = m_layout.vadModel.toUtf8();
        const QByteArray modelType = m_layout.modelType.toUtf8();

        SherpaOnnxOfflineRecognizerConfig recognizerConfig{};
        recognizerConfig.feat_config.sample_rate = SampleRate;
        recognizerConfig.feat_config.feature_dim = 80;
        recognizerConfig.model_config.tokens = tokens.constData();
        recognizerConfig.model_config.num_threads = std::max(1, m_config.threads);
        recognizerConfig.model_config.provider = "cpu";
        recognizerConfig.model_config.debug = 0;
        recognizerConfig.decoding_method = "greedy_search";

        if (m_layout.profile == QStringLiteral("parakeet")) {
            recognizerConfig.model_config.transducer.encoder = encoder.constData();
            recognizerConfig.model_config.transducer.decoder = decoder.constData();
            recognizerConfig.model_config.transducer.joiner = joiner.constData();
            if (!modelType.isEmpty())
                recognizerConfig.model_config.model_type = modelType.constData();
        } else {
            recognizerConfig.model_config.moonshine.encoder = encoder.constData();
            recognizerConfig.model_config.moonshine.merged_decoder = decoder.constData();
        }

        m_recognizer = SherpaOnnxCreateOfflineRecognizer(&recognizerConfig);
        if (!m_recognizer) {
            emit errorTextChanged(
                QStringLiteral("Failed to load the %1 ASR model from %2.")
                    .arg(m_layout.profile, m_layout.directory));
            emit stateChanged(ITranscriptProvider::State::Error);
            return false;
        }

        SherpaOnnxVadModelConfig vadConfig{};
        vadConfig.silero_vad.model = vad.constData();
        vadConfig.silero_vad.threshold = static_cast<float>(m_config.vadThreshold);
        vadConfig.silero_vad.min_silence_duration =
            static_cast<float>(m_config.silenceTimeoutMs) / 1000.0f;
        vadConfig.silero_vad.min_speech_duration = MinSpeechSeconds;
        vadConfig.silero_vad.max_speech_duration = MaxSpeechSeconds;
        vadConfig.silero_vad.window_size = VadWindowSize;
        vadConfig.sample_rate = SampleRate;
        vadConfig.num_threads = 1;
        vadConfig.provider = "cpu";
        vadConfig.debug = 0;

        m_vad = SherpaOnnxCreateVoiceActivityDetector(&vadConfig, 60.0f);
        if (!m_vad) {
            releaseModels();
            emit errorTextChanged(
                QStringLiteral("Failed to load the Silero VAD model from %1.")
                    .arg(m_layout.vadModel));
            emit stateChanged(ITranscriptProvider::State::Error);
            return false;
        }

        qInfo().noquote() << QStringLiteral("Dex ASR: loaded %1 from %2 (%3 threads)")
                                 .arg(m_layout.profile, m_layout.directory)
                                 .arg(m_config.threads);
        return true;
    }

    void releaseModels()
    {
        if (m_vad) {
            SherpaOnnxDestroyVoiceActivityDetector(m_vad);
            m_vad = nullptr;
        }
        if (m_recognizer) {
            SherpaOnnxDestroyOfflineRecognizer(m_recognizer);
            m_recognizer = nullptr;
        }
    }

    void resetRecognition()
    {
        m_active.clear();
        m_preRoll.clear();
        m_inSpeech = false;
        m_lastPartial.clear();
    }

    void drainBuffer()
    {
        if (!m_buffer)
            return;
        float scratch[2048];
        while (m_buffer->pop(scratch, 2048) > 0) {
        }
    }

    QString decode(const float *samples, int count)
    {
        if (!m_recognizer || count <= 0)
            return {};

        const SherpaOnnxOfflineStream *stream = SherpaOnnxCreateOfflineStream(m_recognizer);
        if (!stream)
            return {};

        SherpaOnnxAcceptWaveformOffline(stream, SampleRate, samples, count);
        SherpaOnnxDecodeOfflineStream(m_recognizer, stream);
        const SherpaOnnxOfflineRecognizerResult *result =
            SherpaOnnxGetOfflineStreamResult(stream);

        QString text;
        if (result && result->text)
            text = QString::fromUtf8(result->text).trimmed();

        if (result)
            SherpaOnnxDestroyOfflineRecognizerResult(result);
        SherpaOnnxDestroyOfflineStream(stream);
        return text;
    }

    void tick()
    {
        if (!m_buffer || !m_vad || !m_recognizer)
            return;

        const std::size_t dropped = m_buffer->dropped();
        if (dropped != m_reportedDrops) {
            m_reportedDrops = dropped;
            qWarning("Dex ASR: dropped %zu audio samples (inference fell behind).",
                     dropped);
        }

        float chunk[2048];
        std::size_t count = 0;
        while ((count = m_buffer->pop(chunk, 2048)) > 0) {
            SherpaOnnxVoiceActivityDetectorAcceptWaveform(
                m_vad, chunk, static_cast<int>(count));

            const bool detected = SherpaOnnxVoiceActivityDetectorDetected(m_vad) != 0;
            if (detected) {
                if (!m_inSpeech) {
                    m_inSpeech = true;
                    // Seed with recent non-speech audio so the offline model
                    // sees the start of the utterance instead of a clipped
                    // first phoneme, which tends to trigger decoder loops.
                    m_active = m_preRoll;
                    m_preRoll.clear();
                }
                m_active.insert(m_active.end(), chunk, chunk + count);
                if (m_active.size() > static_cast<std::size_t>(MaxActiveSamples)) {
                    const std::size_t excess = m_active.size() - MaxActiveSamples;
                    m_active.erase(m_active.begin(), m_active.begin() + excess);
                }
            } else if (m_inSpeech) {
                // Keep the trailing silence that belongs to the utterance so
                // the partial transcript stays stable until the VAD closes it.
                m_active.insert(m_active.end(), chunk, chunk + count);
            } else {
                m_preRoll.insert(m_preRoll.end(), chunk, chunk + count);
                if (m_preRoll.size() > static_cast<std::size_t>(PreRollSamples)) {
                    const std::size_t excess = m_preRoll.size() - PreRollSamples;
                    m_preRoll.erase(m_preRoll.begin(), m_preRoll.begin() + excess);
                }
            }
        }

        bool producedFinal = false;
        while (!SherpaOnnxVoiceActivityDetectorEmpty(m_vad)) {
            const SherpaOnnxSpeechSegment *segment =
                SherpaOnnxVoiceActivityDetectorFront(m_vad);
            if (segment && segment->samples && segment->n > 0) {
                emit stateChanged(ITranscriptProvider::State::Finalizing);
                const QString text = decode(segment->samples, segment->n);
                if (!text.isEmpty()) {
                    m_lastPartial.clear();
                    qInfo().noquote() << QStringLiteral("Dex ASR: final \"%1\"").arg(text);
                    emit utteranceFinalized(text);
                }
            }
            if (segment)
                SherpaOnnxDestroySpeechSegment(segment);
            SherpaOnnxVoiceActivityDetectorPop(m_vad);
            producedFinal = true;
        }

        if (producedFinal) {
            resetRecognition();
            emit stateChanged(ITranscriptProvider::State::Listening);
        }

        if (!m_inSpeech || m_active.empty())
            return;

        int interval = std::max(100, m_config.partialIntervalMs);
        if (m_layout.profile == QStringLiteral("parakeet"))
            interval = std::max(interval, 900);
        if (m_partialClock.elapsed() < interval)
            return;
        m_partialClock.restart();

        // Only decode a tail of the active buffer for partials. Offline
        // models are quadratic-ish in audio length; this keeps the worker
        // responsive during long dictation while the final segment still
        // uses the complete VAD buffer.
        const std::size_t partialWindow = static_cast<std::size_t>(SampleRate)
            * (m_layout.profile == QStringLiteral("parakeet") ? 3u : 8u);
        const std::size_t offset =
            m_active.size() > partialWindow ? m_active.size() - partialWindow : 0;
        if (m_active.size() - offset < static_cast<std::size_t>(SampleRate) / 2)
            return;

        // Offline decoders use trailing context to decide that an utterance
        // ended. Without a little silence Moonshine can loop on the last word
        // while the VAD segment is still open, so pad only the partial decode
        // (the final decode already includes the VAD's trailing silence).
        m_partialScratch.assign(m_active.begin() + static_cast<std::ptrdiff_t>(offset),
                                m_active.end());
        m_partialScratch.resize(m_partialScratch.size()
                                    + static_cast<std::size_t>(SampleRate) * PartialSilenceMs
                                        / 1000,
                                0.0f);

        const QString text =
            decode(m_partialScratch.data(), static_cast<int>(m_partialScratch.size()));
        if (!text.isEmpty() && text != m_lastPartial) {
            m_lastPartial = text;
            qCDebug(dexAsr) << "partial" << text;
            emit partialTextChanged(text);
        }
    }

    AsrConfig m_config;
    ModelLayout m_layout;
    AudioCapture::Buffer *m_buffer = nullptr;
    const SherpaOnnxOfflineRecognizer *m_recognizer = nullptr;
    const SherpaOnnxVoiceActivityDetector *m_vad = nullptr;
    QTimer *m_timer = nullptr;
    QElapsedTimer m_partialClock;
    std::vector<float> m_active;
    std::vector<float> m_preRoll;
    std::vector<float> m_partialScratch;
    QString m_lastPartial;
    std::size_t m_reportedDrops = 0;
    bool m_inSpeech = false;
};

// ---------------------------------------------------------------------------

SherpaTranscriptProvider::SherpaTranscriptProvider(AsrConfig config, QObject *parent)
    : ITranscriptProvider(parent)
    , m_config(std::move(config))
{
    if (m_config.modelDir.isEmpty())
        m_config.modelDir = defaultModelDir();
    if (m_config.vadModel.isEmpty()) {
        m_config.vadModel =
            QDir(m_config.modelDir).filePath(QStringLiteral("silero_vad.onnx"));
    }

    m_worker = new SherpaWorker;
    m_worker->moveToThread(&m_thread);
    m_thread.setObjectName(QStringLiteral("dex-asr"));

    connect(m_worker, &SherpaWorker::partialTextChanged,
            this, &ITranscriptProvider::partialTextChanged);
    connect(m_worker, &SherpaWorker::utteranceFinalized,
            this, &ITranscriptProvider::utteranceFinalized);
    connect(m_worker, &SherpaWorker::stateChanged,
            this, &ITranscriptProvider::stateChanged);
    connect(m_worker, &SherpaWorker::errorTextChanged,
            this, &ITranscriptProvider::errorTextChanged);
    connect(m_worker, &SherpaWorker::modelLoaded,
            this, &SherpaTranscriptProvider::modelLoaded);

    m_thread.start();
}

SherpaTranscriptProvider::~SherpaTranscriptProvider()
{
    stop();
    delete m_worker;
    m_worker = nullptr;
}

bool SherpaTranscriptProvider::prepare(QString *error)
{
    m_layout = ModelLayout::resolve(m_config.modelProfile,
                                    m_config.modelDir,
                                    m_config.vadModel,
                                    error)
                   .value_or(ModelLayout{});
    m_prepared = m_layout.profile.isEmpty() ? false : true;
    return m_prepared;
}

void SherpaTranscriptProvider::start()
{
    if (m_started)
        return;

    if (!m_prepared) {
        QString error;
        if (!prepare(&error)) {
            emit errorTextChanged(error);
            emit stateChanged(State::Error);
            return;
        }
    }

    if (!m_thread.isRunning())
        m_thread.start();

    m_audio = std::make_unique<AudioCapture>();
    connect(m_audio.get(), &AudioCapture::levelChanged,
            this, &ITranscriptProvider::levelChanged);
    connect(m_audio.get(), &AudioCapture::errorTextChanged,
            this, &ITranscriptProvider::errorTextChanged);

    QString audioError;
    if (!m_audio->start(&audioError)) {
        m_audio.reset();
        emit errorTextChanged(audioError);
        emit stateChanged(State::Error);
        return;
    }

    qInfo().noquote() << QStringLiteral("Dex ASR: capturing 16 kHz mono from %1")
                             .arg(m_audio->deviceName());

    const AsrConfig config = m_config;
    const ModelLayout layout = m_layout;
    AudioCapture::Buffer *buffer = &m_audio->buffer();
    SherpaWorker *worker = m_worker;

    QMetaObject::invokeMethod(
        worker,
        [worker, config, layout, buffer] {
            worker->initialize(config, layout, buffer);
        },
        Qt::QueuedConnection);

    m_started = true;
}

void SherpaTranscriptProvider::stop()
{
    if (m_audio) {
        m_audio->stop();
        m_audio.reset();
    }

    if (m_thread.isRunning()) {
        SherpaWorker *worker = m_worker;
        QMetaObject::invokeMethod(worker, &SherpaWorker::shutdown,
                                  Qt::BlockingQueuedConnection);
        m_thread.quit();
        m_thread.wait();
    }
    m_started = false;
}

bool SherpaTranscriptProvider::setModelProfile(const QString &profile)
{
    const QString canonical = ModelLayout::canonicalProfile(profile);
    if (canonical.isEmpty()) {
        emit errorTextChanged(
            QStringLiteral("Unknown ASR profile '%1'.").arg(profile));
        return false;
    }

    QString error;
    const auto layout =
        ModelLayout::resolve(canonical, m_config.modelDir, m_config.vadModel, &error);
    if (!layout) {
        emit errorTextChanged(error);
        return false;
    }

    m_config.modelProfile = canonical;
    m_layout = *layout;
    m_prepared = true;

    if (m_started) {
        const AsrConfig config = m_config;
        const ModelLayout newLayout = m_layout;
        SherpaWorker *worker = m_worker;
        QMetaObject::invokeMethod(
            worker,
            [worker, config, newLayout] { worker->reload(config, newLayout); },
            Qt::QueuedConnection);
    }

    return true;
}

} // namespace dex

#include "SherpaTranscriptProvider.moc"
