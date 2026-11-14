#include "provider/ModelLayout.hpp"
#include "provider/ProviderFactory.hpp"
#include "provider/SherpaTranscriptProvider.hpp"
#include "provider/StubTranscriptProvider.hpp"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace dex;

namespace {

void touch(const QDir &dir, const QString &name)
{
    QFile file(dir.filePath(name));
    QVERIFY2(file.open(QIODevice::WriteOnly), qPrintable(file.errorString()));
    file.write("stub");
}

QString makeModelDir(QTemporaryDir &root, const QString &bundle)
{
    QDir dir(root.path());
    dir.mkpath(bundle);
    return dir.filePath(bundle);
}

} // namespace

class TestProvider : public QObject
{
    Q_OBJECT

private slots:
    void resolveDefaults();
    void resolveStubOverrides();
    void resolveUnknownProvider();
    void canonicalProfiles();
    void parakeetLayout();
    void missingVadRejected();
    void missingJoinerRejected();
    void factoryBuildsStub();
    void factoryFailsWithoutModels();
};

void TestProvider::resolveDefaults()
{
    AsrConfig config;
    const ProviderSelection selection = ProviderFactory::resolve(config, false);
    QCOMPARE(static_cast<int>(selection.kind), static_cast<int>(ProviderKind::Sherpa));
    QVERIFY(selection.reason.contains(QStringLiteral("Parakeet")));
}

void TestProvider::resolveStubOverrides()
{
    AsrConfig config;
    config.provider = QStringLiteral("stub");
    QCOMPARE(static_cast<int>(ProviderFactory::resolve(config, false).kind),
             static_cast<int>(ProviderKind::Stub));
    QCOMPARE(static_cast<int>(ProviderFactory::resolve(config, true).kind),
             static_cast<int>(ProviderKind::Stub));

    config.provider = QStringLiteral("sherpa");
    QCOMPARE(static_cast<int>(ProviderFactory::resolve(config, true).kind),
             static_cast<int>(ProviderKind::Stub));
    QCOMPARE(ProviderFactory::resolve(config, true).reason, QStringLiteral("--stub"));
}

void TestProvider::resolveUnknownProvider()
{
    AsrConfig config;
    config.provider = QStringLiteral("unknown-provider");
    const ProviderSelection selection = ProviderFactory::resolve(config, false);
    QCOMPARE(static_cast<int>(selection.kind), static_cast<int>(ProviderKind::Stub));
    QVERIFY(selection.reason.contains(QStringLiteral("unknown-provider")));
}

void TestProvider::canonicalProfiles()
{
    QCOMPARE(ModelLayout::canonicalProfile(QStringLiteral("parakeet")),
             QStringLiteral("parakeet"));
    QCOMPARE(ModelLayout::canonicalProfile(QStringLiteral("parakeet-v2")),
             QStringLiteral("parakeet"));
    QCOMPARE(ModelLayout::canonicalProfile(QStringLiteral("accurate")),
             QStringLiteral("parakeet"));
    QCOMPARE(ModelLayout::canonicalProfile(QStringLiteral("stub")),
             QStringLiteral("stub"));

    // Moonshine and unsupported names are rejected
    QVERIFY(ModelLayout::canonicalProfile(QStringLiteral("Moonshine")).isEmpty());
    QVERIFY(ModelLayout::canonicalProfile(QStringLiteral("whisper")).isEmpty());
    QCOMPARE(ModelLayout::supportedProfiles(), QStringList({QStringLiteral("parakeet")}));
}

void TestProvider::parakeetLayout()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString bundle = makeModelDir(
        root, QStringLiteral("sherpa-onnx-nemo-parakeet-tdt-0.6b-v2-int8"));
    QDir dir(bundle);
    touch(dir, QStringLiteral("encoder.int8.onnx"));
    touch(dir, QStringLiteral("decoder.int8.onnx"));
    touch(dir, QStringLiteral("joiner.int8.onnx"));
    touch(dir, QStringLiteral("tokens.txt"));
    touch(dir, QStringLiteral("silero_vad.onnx"));

    QString error;
    const auto layout = ModelLayout::resolve(root.path(), {}, &error);
    QVERIFY2(layout.has_value(), qPrintable(error));
    QCOMPARE(layout->profile, QStringLiteral("parakeet"));
    QCOMPARE(layout->modelType, QStringLiteral("nemo_transducer"));
    QCOMPARE(QFileInfo(layout->encoder).fileName(), QStringLiteral("encoder.int8.onnx"));
    QCOMPARE(QFileInfo(layout->decoder).fileName(), QStringLiteral("decoder.int8.onnx"));
    QCOMPARE(QFileInfo(layout->joiner).fileName(), QStringLiteral("joiner.int8.onnx"));
    QCOMPARE(QFileInfo(layout->tokens).fileName(), QStringLiteral("tokens.txt"));
    QCOMPARE(QFileInfo(layout->vadModel).fileName(), QStringLiteral("silero_vad.onnx"));
}

void TestProvider::missingVadRejected()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString bundle = makeModelDir(
        root, QStringLiteral("sherpa-onnx-nemo-parakeet-tdt-0.6b-v2-int8"));
    QDir dir(bundle);
    touch(dir, QStringLiteral("encoder.int8.onnx"));
    touch(dir, QStringLiteral("decoder.int8.onnx"));
    touch(dir, QStringLiteral("joiner.int8.onnx"));
    touch(dir, QStringLiteral("tokens.txt"));

    QString error;
    QVERIFY(!ModelLayout::resolve(root.path(), {}, &error));
    QVERIFY(error.contains(QStringLiteral("VAD")));
}

void TestProvider::missingJoinerRejected()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString bundle = makeModelDir(
        root, QStringLiteral("sherpa-onnx-nemo-parakeet-tdt-0.6b-v2-int8"));
    QDir dir(bundle);
    touch(dir, QStringLiteral("encoder.int8.onnx"));
    touch(dir, QStringLiteral("decoder.int8.onnx"));
    touch(dir, QStringLiteral("tokens.txt"));
    touch(dir, QStringLiteral("silero_vad.onnx"));

    QVERIFY(!ModelLayout::resolve(root.path(), {}, nullptr));
}

void TestProvider::factoryBuildsStub()
{
    AsrConfig config;
    QString error;
    auto provider = ProviderFactory::create(config, true, nullptr, &error);
    QVERIFY2(provider != nullptr, qPrintable(error));
    QVERIFY(dynamic_cast<StubTranscriptProvider *>(provider.get()) != nullptr);
}

void TestProvider::factoryFailsWithoutModels()
{
    AsrConfig config;
    config.modelDir = QStringLiteral("/nonexistent/dex-models");
    config.vadModel.clear();
    QString error;
    auto provider = ProviderFactory::create(config, false, nullptr, &error);
    QVERIFY(provider == nullptr);
    QVERIFY(!error.isEmpty());
}

QTEST_GUILESS_MAIN(TestProvider)
#include "tst_provider.moc"
