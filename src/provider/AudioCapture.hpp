#pragma once

#include "provider/SpscRingBuffer.hpp"

#include <QElapsedTimer>
#include <QObject>
#include <QString>

#include <memory>

QT_BEGIN_NAMESPACE
class QAudioSource;
class QIODevice;
QT_END_NAMESPACE

namespace dex {

/// Captures 16 kHz mono audio from the default input device and pushes it
/// into a lock-free ring buffer consumed by the ASR worker thread.
///
/// The class itself lives on the GUI thread: Qt Multimedia delivers
/// `readyRead` there, which keeps the single-producer contract intact.
class AudioCapture : public QObject
{
    Q_OBJECT

public:
    /// ~4 s of headroom at 16 kHz. The consumer drains every few tens of
    /// milliseconds, so overflow only happens if inference stalls badly.
    using Buffer = SpscRingBuffer<float, 1u << 16>;

    static constexpr int SampleRate = 16000;

    explicit AudioCapture(QObject *parent = nullptr);
    ~AudioCapture() override;

    bool start(QString *error = nullptr);
    void stop();

    bool isRunning() const { return m_running; }
    QString deviceName() const { return m_deviceName; }

    Buffer &buffer() { return m_buffer; }

signals:
    /// Smoothed 0.0–1.0 input level, emitted at ~25 Hz for the QML pulse.
    void levelChanged(qreal level);
    void errorTextChanged(const QString &error);

private:
    void readMore();
    void publishLevel(float rms);

    Buffer m_buffer;
    std::unique_ptr<QAudioSource> m_source;
    QIODevice *m_io = nullptr; // owned by QAudioSource
    QElapsedTimer m_levelClock;
    QString m_deviceName;
    qreal m_level = 0.0;
    bool m_floatInput = true;
    bool m_running = false;
    bool m_stopping = false;
};

} // namespace dex
