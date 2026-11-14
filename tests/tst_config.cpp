#include "core/Config.hpp"

#include <QtTest>

#include <string_view>

using dex::Config;

class TestConfig : public QObject
{
    Q_OBJECT

private slots:
    void loadShippedConfig();
    void defaultsWhenKeysMissing();
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
    QCOMPARE(config->asr.modelProfile, QStringLiteral("balanced"));
    QCOMPARE(config->asr.threads, 4);
    QCOMPARE(config->asr.silenceTimeoutMs, 800);
    QCOMPARE(config->ui.anchor, QStringLiteral("top"));
    QCOMPARE(config->ui.marginTop, 48);
    QCOMPARE(config->ui.width, 460);
}

void TestConfig::defaultsWhenKeysMissing()
{
    QString error;
    const auto config = Config::fromToml(std::string_view("[general]\ndaemon = false\n"), &error);
    QVERIFY2(config.has_value(), qPrintable(error));
    QCOMPARE(config->general.daemon, false);
    QCOMPARE(config->general.confirmByDefault, false);
    QCOMPARE(config->general.autoHideMs, 2200);
    QCOMPARE(config->asr.modelProfile, QStringLiteral("balanced"));
    QCOMPARE(config->ui.width, 460);
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
