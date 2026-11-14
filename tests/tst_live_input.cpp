#include "provider/ITranscriptProvider.hpp"
#include "provider/ModelLayout.hpp"
#include "provider/ProviderFactory.hpp"
#include "provider/SherpaTranscriptProvider.hpp"

#include <QAudioDevice>
#include <QElapsedTimer>
#include <QMediaDevices>
#include <QtTest>

#include <cstdio>
#include <memory>

#include <unistd.h>

using namespace dex;

namespace {

constexpr int DefaultSpeechTimeoutMs = 25000;

/// Writes a line straight to stdout and flushes immediately. QtTest buffers
/// its own log until the test finishes, but live transcripts are only useful
/// if they appear while the user is still speaking.
void announce(const QString &line)
{
    const QByteArray utf8 = line.toUtf8();
    std::fwrite(utf8.constData(), 1, static_cast<std::size_t>(utf8.size()), stdout);
    std::fputc('\n', stdout);
    std::fflush(stdout);
}

int envInt(const char *name, int fallback)
{
    bool ok = false;
    const int value = qEnvironmentVariableIntValue(name, &ok);
    return ok && value > 0 ? value : fallback;
}

} // namespace

/// Real microphone test: it drives the production `AudioCapture` ->
/// Silero VAD -> sherpa-onnx path against the system default input device and
/// prints partials/finals while it runs.
///
/// It has nothing to assert until someone speaks, so it skips instead of
/// failing when no utterance arrives in time. That keeps `ctest` usable on a
/// quiet desktop while still catching broken models, a dead input device, or
/// a stalled decode loop. Non-interactive runs skip immediately unless
/// DEX_TEST_SPEECH_TIMEOUT_MS is set, so the suite never stalls on CI.
///
/// Environment overrides:
///   DEX_TEST_PROFILE            moonshine (default) or parakeet
///   DEX_TEST_MODEL_DIR          model base directory
///   DEX_TEST_SPEECH_TIMEOUT_MS  how long to wait for speech (default 25000)
///   DEX_TEST_UTTERANCES         utterances to collect (default 1)
///   DEX_TEST_EXPECT             substring the transcript must contain
class TestLiveInput : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void defaultInputTranscribesSpeech();

private:
    AsrConfig m_config;
};

void TestLiveInput::initTestCase()
{
    m_config.modelProfile =
        qEnvironmentVariable("DEX_TEST_PROFILE", QStringLiteral("moonshine"));

    const QString overrideDir = qEnvironmentVariable("DEX_TEST_MODEL_DIR");
    if (!overrideDir.isEmpty())
        m_config.modelDir = overrideDir;

    if (QMediaDevices::defaultAudioInput().isNull())
        QSKIP("No default audio input device is available.");

    // Resolve the bundle up front so a missing download skips the suite
    // instead of spending seconds starting a provider that cannot load.
    const QString baseDir =
        m_config.modelDir.isEmpty() ? defaultModelDir() : m_config.modelDir;
    QString error;
    const auto layout =
        ModelLayout::resolve(m_config.modelProfile, baseDir, m_config.vadModel, &error);
    if (!layout) {
        const QString message = QStringLiteral(
            "ASR models unavailable (%1). Run scripts/download-models.sh or set "
            "DEX_TEST_MODEL_DIR.")
            .arg(error);
        QSKIP(qPrintable(message));
    }

    announce(QStringLiteral("Live ASR test: input='%1' model='%2'")
                 .arg(QMediaDevices::defaultAudioInput().description(), layout->profile));
}

void TestLiveInput::defaultInputTranscribesSpeech()
{
    // Waiting for speech under `ctest` (or any redirect) would just stall the
    // suite for the whole timeout, so only wait when a human is watching a
    // terminal, or when the timeout was requested explicitly.
    const bool interactive = ::isatty(STDOUT_FILENO) != 0;
    const bool timeoutRequested = qEnvironmentVariableIsSet("DEX_TEST_SPEECH_TIMEOUT_MS");
    if (!interactive && !timeoutRequested) {
        QSKIP("Not attached to a terminal; set DEX_TEST_SPEECH_TIMEOUT_MS to run "
              "the live test under ctest.");
    }
    const int timeoutMs = timeoutRequested
        ? envInt("DEX_TEST_SPEECH_TIMEOUT_MS", DefaultSpeechTimeoutMs)
        : DefaultSpeechTimeoutMs;

    QString error;
    std::unique_ptr<ITranscriptProvider> provider =
        ProviderFactory::create(m_config, false, nullptr, &error);
    QVERIFY2(provider != nullptr, qPrintable(error));
    QVERIFY2(dynamic_cast<SherpaTranscriptProvider *>(provider.get()) != nullptr,
             "The factory selected the stub backend; check DEX_TEST_PROFILE.");

    QStringList finals;
    QStringList errors;
    bool listening = false;

    connect(provider.get(), &ITranscriptProvider::partialTextChanged, this,
            [](const QString &text) {
                announce(QStringLiteral("  [partial] %1").arg(text));
            });
    connect(provider.get(), &ITranscriptProvider::utteranceFinalized, this,
            [&finals](const QString &text) {
                finals.append(text);
                announce(QStringLiteral("  [final]   %1").arg(text));
            });
    connect(provider.get(), &ITranscriptProvider::errorTextChanged, this,
            [&errors](const QString &text) { errors.append(text); });
    connect(provider.get(), &ITranscriptProvider::stateChanged, this,
            [&listening](ITranscriptProvider::State state) {
                if (state == ITranscriptProvider::State::Listening)
                    listening = true;
            });

    const int wanted = envInt("DEX_TEST_UTTERANCES", 1);

    provider->start();
    announce(QStringLiteral("Listening - speak %1 utterance(s) now (%2 s timeout)...")
                 .arg(wanted)
                 .arg(timeoutMs / 1000));

    QElapsedTimer clock;
    clock.start();
    while (finals.size() < wanted && errors.isEmpty() && clock.elapsed() < timeoutMs)
        QTest::qWait(50);

    provider->stop();

    QVERIFY2(errors.isEmpty(), qPrintable(errors.join(QStringLiteral("; "))));
    QVERIFY2(listening, "The provider never reached the Listening state.");

    if (finals.isEmpty()) {
        const QString message = QStringLiteral(
            "No speech captured in %1 s from '%2'. Speak while the test runs, "
            "or raise DEX_TEST_SPEECH_TIMEOUT_MS.")
            .arg(timeoutMs / 1000)
            .arg(QMediaDevices::defaultAudioInput().description());
        QSKIP(qPrintable(message));
    }

    const QString transcript = finals.join(QStringLiteral(" "));
    announce(QStringLiteral("Captured %1 utterance(s): %2")
                 .arg(finals.size())
                 .arg(transcript));

    const QString expected = qEnvironmentVariable("DEX_TEST_EXPECT");
    if (!expected.isEmpty()) {
        QVERIFY2(transcript.contains(expected, Qt::CaseInsensitive),
                 qPrintable(QStringLiteral("Expected '%1' in '%2'")
                                .arg(expected, transcript)));
    }
}

QTEST_GUILESS_MAIN(TestLiveInput)
#include "tst_live_input.moc"
