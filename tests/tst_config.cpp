#include "core/Config.hpp"

#include <QDir>
#include <QtTest>

#include <string_view>

using dex::Config;

class TestConfig : public QObject
{
    Q_OBJECT

private slots:
    void loadShippedConfig();
    void defaultsWhenKeysMissing();
    void parsesAsrOverrides();
    void parsesMathSlmOverrides();
    void rejectsBrokenToml();
};

void TestConfig::loadShippedConfig()
{
    QString error;
    const auto config = Config::load(QStringLiteral(DEX_CONFIG_TOML), &error);
    QVERIFY2(config.has_value(), qPrintable(error));

    QCOMPARE(config->general.daemon, true);
    QCOMPARE(config->general.confirmByDefault, false);
    QCOMPARE(config->general.autoHideMs, 2200);
    QCOMPARE(config->asr.provider, QStringLiteral("sherpa"));
    QCOMPARE(config->asr.modelDir, dex::defaultModelDir());
    QCOMPARE(config->asr.vadModel,
             QDir(config->asr.modelDir).filePath(QStringLiteral("silero_vad.onnx")));
    QCOMPARE(config->asr.threads, 4);
    QCOMPARE(config->asr.silenceTimeoutMs, 800);
    QCOMPARE(config->asr.vadThreshold, 0.5);
    QCOMPARE(config->ui.anchor, QStringLiteral("top"));
    QCOMPARE(config->ui.marginTop, 48);
    QCOMPARE(config->ui.width, 460);
    QCOMPARE(config->mathSlm.enabled, true);
    QCOMPARE(config->mathSlm.modelPath, dex::defaultSlmModelPath());
    QCOMPARE(config->mathSlm.threads, 4);
    QCOMPARE(config->mathSlm.idleUnloadSec, 60);
}

void TestConfig::defaultsWhenKeysMissing()
{
    QString error;
    const auto config = Config::fromToml(std::string_view("[general]\ndaemon = false\n"), &error);
    QVERIFY2(config.has_value(), qPrintable(error));
    QCOMPARE(config->general.daemon, false);
    QCOMPARE(config->general.confirmByDefault, false);
    QCOMPARE(config->general.autoHideMs, 2200);
    QCOMPARE(config->asr.provider, QStringLiteral("sherpa"));
    QCOMPARE(config->asr.modelDir, dex::defaultModelDir());
    QCOMPARE(config->asr.vadModel,
             QDir(config->asr.modelDir).filePath(QStringLiteral("silero_vad.onnx")));
    QCOMPARE(config->ui.width, 460);
    QCOMPARE(config->mathSlm.enabled, true);
    QCOMPARE(config->mathSlm.modelPath, dex::defaultSlmModelPath());
    QCOMPARE(config->mathSlm.threads, 4);
    QCOMPARE(config->mathSlm.idleUnloadSec, 60);
}

void TestConfig::parsesAsrOverrides()
{
    QString error;
    const auto config = Config::fromToml(std::string_view(R"(
[asr]
provider = "stub"
model_dir = "~/custom-models"
vad_model = "~/custom-models/vad.onnx"
threads = 99
silence_timeout_ms = 50
vad_threshold = 2.0
)"),
                                         &error);
    QVERIFY2(config.has_value(), qPrintable(error));
    QCOMPARE(config->asr.provider, QStringLiteral("stub"));
    QCOMPARE(config->asr.modelDir, QDir::home().filePath(QStringLiteral("custom-models")));
    QCOMPARE(config->asr.vadModel,
             QDir::home().filePath(QStringLiteral("custom-models/vad.onnx")));
    QCOMPARE(config->asr.threads, 16);
    QCOMPARE(config->asr.silenceTimeoutMs, 100);
    QCOMPARE(config->asr.vadThreshold, 0.95);
}

void TestConfig::parsesMathSlmOverrides()
{
    QString error;
    const auto config = Config::fromToml(std::string_view(R"(
[math_slm]
enabled = false
model_path = "~/custom-models/qwen.gguf"
threads = 99
idle_unload_sec = 0
)"),
                                         &error);
    QVERIFY2(config.has_value(), qPrintable(error));
    QCOMPARE(config->mathSlm.enabled, false);
    QCOMPARE(config->mathSlm.modelPath,
             QDir::home().filePath(QStringLiteral("custom-models/qwen.gguf")));
    QCOMPARE(config->mathSlm.threads, 16);
    QCOMPARE(config->mathSlm.idleUnloadSec, 0);
}

void TestConfig::rejectsBrokenToml()
{
    QString error;
    const auto config = Config::fromToml(std::string_view("[general\ndaemon = "), &error);
    QVERIFY(!config.has_value());
    QVERIFY(!error.isEmpty());
}

QTEST_APPLESS_MAIN(TestConfig)
#include "tst_config.moc"
