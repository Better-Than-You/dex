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
    void resolveUnknownProfile();
    void canonicalProfiles();
    void moonshineLayout();
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
    QCOMPARE(selection.profile, QStringLiteral("moonshine"));

    config.modelProfile = QStringLiteral("parakeet");
    QCOMPARE(ProviderFactory::resolve(config, false).profile, QStringLiteral("parakeet"));
    QCOMPARE(static_cast<int>(ProviderFactory::resolve(config, false).kind),
             static_cast<int>(ProviderKind::Sherpa));
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
    QCOMPARE(ProviderFactory::resolve(config, true).profile, QStringLiteral("stub"));
}

void TestProvider::resolveUnknownProfile()
{
    AsrConfig config;
    config.modelProfile = QStringLiteral("gpt-4");
    const ProviderSelection selection = ProviderFactory::resolve(config, false);
    QCOMPARE(static_cast<int>(selection.kind), static_cast<int>(ProviderKind::Stub));
    QVERIFY(selection.reason.contains(QStringLiteral("gpt-4")));
}

void TestProvider::canonicalProfiles()
{
    QCOMPARE(ModelLayout::canonicalProfile(QStringLiteral("Moonshine")),
             QStringLiteral("moonshine"));
    QCOMPARE(ModelLayout::canonicalProfile(QStringLiteral("balanced")),
             QStringLiteral("moonshine"));
    QCOMPARE(ModelLayout::canonicalProfile(QStringLiteral("parakeet-v2")),
             QStringLiteral("parakeet"));
    QVERIFY(ModelLayout::canonicalProfile(QStringLiteral("whisper")).isEmpty());
    QCOMPARE(ModelLayout::supportedProfiles(),
             QStringList({QStringLiteral("moonshine"), QStringLiteral("parakeet")}));
}

void TestProvider::moonshineLayout()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString bundle = makeModelDir(root, QStringLiteral(
        "sherpa-onnx-moonshine-base-en-quantized-2026-02-27"));
    QDir dir(bundle);
    touch(dir, QStringLiteral("encoder_model.ort"));
    touch(dir, QStringLiteral("decoder_model_merged.ort"));
    touch(dir, QStringLiteral("tokens.txt"));
    touch(dir, QStringLiteral("silero_vad.onnx"));

    QString error;
    const auto layout = ModelLayout::resolve(QStringLiteral("moonshine"), root.path(), {},
                                             &error);
    QVERIFY2(layout.has_value(), qPrintable(error));
    QCOMPARE(layout->profile, QStringLiteral("moonshine"));
    QCOMPARE(QFileInfo(layout->encoder).fileName(), QStringLiteral("encoder_model.ort"));
    QCOMPARE(QFileInfo(layout->decoder).fileName(),
             QStringLiteral("decoder_model_merged.ort"));
    QVERIFY(layout->joiner.isEmpty());
    QCOMPARE(QFileInfo(layout->vadModel).fileName(), QStringLiteral("silero_vad.onnx"));
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
    const auto layout = ModelLayout::resolve(QStringLiteral("parakeet"), root.path(), {},
                                             &error);
    QVERIFY2(layout.has_value(), qPrintable(error));
    QCOMPARE(layout->modelType, QStringLiteral("nemo_transducer"));
    QCOMPARE(QFileInfo(layout->joiner).fileName(), QStringLiteral("joiner.int8.onnx"));
}

void TestProvider::missingVadRejected()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QDir rootDir(root.path());
    touch(rootDir, QStringLiteral("encoder_model.ort"));
    touch(rootDir, QStringLiteral("decoder_model_merged.ort"));
    touch(rootDir, QStringLiteral("tokens.txt"));

    QString error;
    QVERIFY(!ModelLayout::resolve(QStringLiteral("moonshine"), root.path(), {}, &error));
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

    QVERIFY(!ModelLayout::resolve(QStringLiteral("parakeet"), root.path(), {}, nullptr));
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
