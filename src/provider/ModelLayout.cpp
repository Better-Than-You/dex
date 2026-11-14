#include "provider/ModelLayout.hpp"

#include <QDir>
#include <QFileInfo>

namespace dex {
namespace {

QString firstExisting(const QDir &dir, std::initializer_list<const char *> candidates)
{
    for (const char *candidate : candidates) {
        const QString path = dir.filePath(QString::fromLatin1(candidate));
        if (QFileInfo::exists(path))
            return path;
    }
    return {};
}

QString bundleDirectoryName()
{
    return QStringLiteral("sherpa-onnx-nemo-parakeet-tdt-0.6b-v2-int8");
}

} // namespace

QString ModelLayout::bundleName()
{
    return bundleDirectoryName();
}

QString ModelLayout::canonicalProfile(const QString &raw)
{
    const QString value = raw.trimmed().toLower();
    if (value == QStringLiteral("parakeet") || value == QStringLiteral("parakeet-v2")
        || value == QStringLiteral("parakeet-tdt") || value == QStringLiteral("accurate")) {
        return QStringLiteral("parakeet");
    }
    if (value == QStringLiteral("stub")) {
        return QStringLiteral("stub");
    }
    return {};
}

QStringList ModelLayout::supportedProfiles()
{
    return {QStringLiteral("parakeet")};
}

std::optional<ModelLayout> ModelLayout::resolve(const QString &modelDir,
                                                const QString &vadModel,
                                                QString *error)
{
    if (!QFileInfo(modelDir).isDir()) {
        if (error)
            *error = QStringLiteral("ASR model directory does not exist: %1").arg(modelDir);
        return std::nullopt;
    }

    const QDir base(modelDir);
    const QString bundle = bundleDirectoryName();
    const QString vadFileName = QStringLiteral("silero_vad.onnx");

    // The base directory may be `models/` (bundle nested one level down) or the
    // bundle itself (e.g. a test fixture or an explicit model_dir).
    const QStringList candidates = {
        base.filePath(bundle),
        base.absolutePath(),
    };

    for (const QString &candidate : candidates) {
        const QDir dir(candidate);

        ModelLayout layout;
        layout.profile = QStringLiteral("parakeet");
        layout.directory = dir.absolutePath();
        layout.modelType = QStringLiteral("nemo_transducer");

        layout.encoder = firstExisting(dir, {"encoder.int8.onnx", "encoder.onnx"});
        layout.decoder = firstExisting(dir, {"decoder.int8.onnx", "decoder.onnx"});
        layout.joiner = firstExisting(dir, {"joiner.int8.onnx", "joiner.onnx"});
        layout.tokens = firstExisting(dir, {"tokens.txt"});

        const bool complete = !layout.encoder.isEmpty() && !layout.decoder.isEmpty()
            && !layout.joiner.isEmpty() && !layout.tokens.isEmpty();
        if (!complete)
            continue;

        if (!vadModel.isEmpty() && QFileInfo::exists(vadModel))
            layout.vadModel = vadModel;
        else
            layout.vadModel = firstExisting(dir, {"silero_vad.onnx"});
        if (layout.vadModel.isEmpty()) {
            const QString sibling = base.filePath(vadFileName);
            if (QFileInfo::exists(sibling))
                layout.vadModel = sibling;
        }

        if (layout.vadModel.isEmpty()) {
            if (error) {
                *error = QStringLiteral("Silero VAD model not found (expected %1).")
                             .arg(base.filePath(vadFileName));
            }
            return std::nullopt;
        }

        return layout;
    }

    if (error) {
        *error = QStringLiteral("No Parakeet model bundle found below %1 "
                                "(expected %2/ with encoder.int8.onnx, decoder.int8.onnx, "
                                "joiner.int8.onnx, tokens.txt).")
                     .arg(base.absolutePath(), bundle);
    }
    return std::nullopt;
}

} // namespace dex
