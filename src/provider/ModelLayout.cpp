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

QStringList profileMarkers(const QString &profile)
{
    if (profile == QStringLiteral("moonshine")) {
        return {
            QStringLiteral("sherpa-onnx-moonshine-base-en-quantized-2026-02-27"),
            QStringLiteral("sherpa-onnx-moonshine"),
        };
    }
    if (profile == QStringLiteral("parakeet")) {
        return {
            QStringLiteral("sherpa-onnx-nemo-parakeet-tdt-0.6b-v2-int8"),
            QStringLiteral("sherpa-onnx-nemo-parakeet"),
        };
    }
    return {};
}

/// Candidate directories: the base directory itself plus any subdirectory
/// whose name matches the profile markers (exact match first).
QList<QDir> candidateDirs(const QString &modelDir, const QString &profile)
{
    const QDir base(modelDir);
    QList<QDir> dirs;
    dirs.append(base);

    const QStringList markers = profileMarkers(profile);
    const QStringList subdirs =
        base.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

    for (const QString &marker : markers) {
        for (const QString &subdir : subdirs) {
            if (subdir == marker)
                dirs.prepend(base.filePath(subdir));
        }
        for (const QString &subdir : subdirs) {
            if (subdir.startsWith(marker))
                dirs.append(base.filePath(subdir));
        }
    }
    return dirs;
}

} // namespace

QString ModelLayout::canonicalProfile(const QString &raw)
{
    const QString value = raw.trimmed().toLower();
    if (value == QStringLiteral("moonshine") || value == QStringLiteral("moonshine-base")
        || value == QStringLiteral("moonshine-streaming-medium")
        || value == QStringLiteral("moonshine-v2") || value == QStringLiteral("balanced")) {
        return QStringLiteral("moonshine");
    }
    if (value == QStringLiteral("parakeet") || value == QStringLiteral("parakeet-v2")
        || value == QStringLiteral("parakeet-tdt") || value == QStringLiteral("accurate")) {
        return QStringLiteral("parakeet");
    }
    if (value == QStringLiteral("light"))
        return QStringLiteral("moonshine");
    return {};
}

QStringList ModelLayout::supportedProfiles()
{
    return {QStringLiteral("moonshine"), QStringLiteral("parakeet")};
}

std::optional<ModelLayout> ModelLayout::resolve(const QString &profile,
                                                const QString &modelDir,
                                                const QString &vadModel,
                                                QString *error)
{
    const QString canonical = canonicalProfile(profile);
    if (canonical.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Unknown ASR model profile '%1' (expected one of: %2).")
                         .arg(profile, supportedProfiles().join(QStringLiteral(", ")));
        }
        return std::nullopt;
    }

    if (!QFileInfo(modelDir).isDir()) {
        if (error)
            *error = QStringLiteral("ASR model directory does not exist: %1").arg(modelDir);
        return std::nullopt;
    }

    for (const QDir &dir : candidateDirs(modelDir, canonical)) {
        ModelLayout layout;
        layout.profile = canonical;
        layout.directory = dir.absolutePath();
        layout.tokens = firstExisting(dir, {"tokens.txt"});

        if (canonical == QStringLiteral("moonshine")) {
            layout.encoder = firstExisting(dir, {"encoder_model.ort", "encoder_model.onnx"});
            layout.decoder = firstExisting(
                dir, {"decoder_model_merged.ort", "decoder_model_merged.onnx"});
        } else {
            layout.encoder = firstExisting(dir, {"encoder.int8.onnx", "encoder.onnx"});
            layout.decoder = firstExisting(dir, {"decoder.int8.onnx", "decoder.onnx"});
            layout.joiner = firstExisting(dir, {"joiner.int8.onnx", "joiner.onnx"});
            layout.modelType = QStringLiteral("nemo_transducer");
        }

        const bool complete = !layout.tokens.isEmpty() && !layout.encoder.isEmpty()
            && !layout.decoder.isEmpty()
            && (canonical == QStringLiteral("moonshine") || !layout.joiner.isEmpty());
        if (!complete)
            continue;

        if (!vadModel.isEmpty() && QFileInfo::exists(vadModel))
            layout.vadModel = vadModel;
        else
            layout.vadModel = firstExisting(dir, {"silero_vad.onnx"});
        if (layout.vadModel.isEmpty()) {
            const QString sibling =
                QDir(modelDir).filePath(QStringLiteral("silero_vad.onnx"));
            if (QFileInfo::exists(sibling))
                layout.vadModel = sibling;
        }

        if (layout.vadModel.isEmpty()) {
            if (error) {
                *error = QStringLiteral("Silero VAD model not found (expected %1).")
                             .arg(QDir(modelDir).filePath(QStringLiteral("silero_vad.onnx")));
            }
            return std::nullopt;
        }

        return layout;
    }

    if (error) {
        *error = QStringLiteral("No %1 model bundle found below %2.")
                     .arg(canonical, modelDir);
    }
    return std::nullopt;
}

} // namespace dex
