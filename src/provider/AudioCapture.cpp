#include "provider/AudioCapture.hpp"

#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSource>
#include <QIODevice>
#include <QMediaDevices>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace dex {
namespace {
constexpr int LevelIntervalMs = 40;
constexpr qreal LevelGain = 12.0;

QString audioErrorLabel(QAudio::Error error)
{
    switch (error) {
    case QAudio::NoError:
        return QStringLiteral("no error");
    case QAudio::OpenError:
        return QStringLiteral("device open error");
    case QAudio::IOError:
        return QStringLiteral("I/O error");
    case QAudio::UnderrunError:
        return QStringLiteral("underrun");
    case QAudio::FatalError:
        return QStringLiteral("fatal error");
    }
    return QStringLiteral("unknown error");
}
} // namespace

AudioCapture::AudioCapture(QObject *parent)
    : QObject(parent)
{
    m_levelClock.start();
}

AudioCapture::~AudioCapture()
{
    stop();
}

bool AudioCapture::start(QString *error)
{
    if (m_running)
        return true;

    const QAudioDevice device = QMediaDevices::defaultAudioInput();
    if (device.isNull()) {
        if (error)
            *error = QStringLiteral("No audio input device is available.");
        return false;
    }

    QAudioFormat format;
    format.setSampleRate(SampleRate);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Float);

    if (!device.isFormatSupported(format)) {
        format.setSampleFormat(QAudioFormat::Int16);
        if (!device.isFormatSupported(format)) {
            if (error) {
                *error = QStringLiteral("Audio device '%1' does not support 16 kHz mono input.")
                             .arg(device.description());
            }
            return false;
        }
    }

    m_floatInput = format.sampleFormat() == QAudioFormat::Float;
    m_buffer.reset();
    m_level = 0.0;
    m_stopping = false;

    m_source = std::make_unique<QAudioSource>(device, format);
    // Keep Qt's internal latency around 100 ms so VAD reacts quickly.
    m_source->setBufferSize(SampleRate * format.bytesPerSample() / 10);

    m_io = m_source->start();
    if (!m_io) {
        if (error) {
            *error = QStringLiteral("Cannot open audio input '%1': %2")
                         .arg(device.description(), audioErrorLabel(m_source->error()));
        }
        m_source.reset();
        return false;
    }

    m_deviceName = device.description();
    connect(m_io, &QIODevice::readyRead, this, &AudioCapture::readMore);
    connect(m_source.get(), &QAudioSource::stateChanged, this, [this](QAudio::State state) {
        if (m_stopping || state != QAudio::StoppedState)
            return;
        if (m_source->error() == QAudio::NoError)
            return;
        m_running = false;
        emit errorTextChanged(
            QStringLiteral("Audio input stopped: %1").arg(m_deviceName));
    });

    m_running = true;
    return true;
}

void AudioCapture::stop()
{
    m_stopping = true;
    m_running = false;

    if (m_source) {
        if (m_io) {
            disconnect(m_io, nullptr, this, nullptr);
            m_io = nullptr;
        }
        m_source->stop();
        m_source.reset();
    }
    m_stopping = false;
}

void AudioCapture::readMore()
{
    if (!m_io)
        return;

    const QByteArray data = m_io->readAll();
    if (data.isEmpty())
        return;

    const int bytesPerSample = m_floatInput ? static_cast<int>(sizeof(float))
                                            : static_cast<int>(sizeof(qint16));
    const int frames = data.size() / bytesPerSample;
    if (frames <= 0)
        return;

    double sumSquares = 0.0;
    if (m_floatInput) {
        const auto *samples = reinterpret_cast<const float *>(data.constData());
        m_buffer.push(samples, static_cast<std::size_t>(frames));
        for (int i = 0; i < frames; ++i) {
            const float value = std::clamp(samples[i], -1.0f, 1.0f);
            sumSquares += static_cast<double>(value) * value;
        }
    } else {
        const auto *samples = reinterpret_cast<const qint16 *>(data.constData());
        std::vector<float> converted(static_cast<std::size_t>(frames));
        for (int i = 0; i < frames; ++i) {
            const float value = static_cast<float>(samples[i]) / 32768.0f;
            converted[static_cast<std::size_t>(i)] = value;
            sumSquares += static_cast<double>(value) * value;
        }
        m_buffer.push(converted.data(), converted.size());
    }

    const float rms = static_cast<float>(std::sqrt(sumSquares / frames));
    publishLevel(rms);
}

void AudioCapture::publishLevel(float rms)
{
    // Map typical speech RMS (~0.02–0.15) onto a usable visual range and
    // smooth the result so the QML pulse does not flicker.
    const qreal instant =
        std::sqrt(std::clamp(static_cast<qreal>(rms) * LevelGain, 0.0, 1.0));
    m_level = 0.55 * m_level + 0.45 * instant;

    if (m_levelClock.elapsed() < LevelIntervalMs)
        return;
    m_levelClock.restart();
    emit levelChanged(m_level);
}

} // namespace dex
