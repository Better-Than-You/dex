#pragma once

#include <QString>
#include <QStringList>

#include <optional>

namespace dex {

/// Resolved on-disk layout for Dex's one and only ASR bundle: NVIDIA Parakeet
/// TDT 0.6B v2 (INT8), a single-shot transducer decoded once per complete VAD
/// utterance.
///
/// Dex stores every model file below a single base directory
/// (`~/.local/share/dex/models`). There is no profile switching: the bundle
/// directory name, the four transducer files and `silero_vad.onnx` are all
/// that `resolve()` looks for.
struct ModelLayout {
    QString profile;    ///< Always `"parakeet"`.
    QString directory;  ///< Directory that contains the model files.
    QString encoder;
    QString decoder;
    QString joiner;
    QString tokens;
    QString vadModel;
    QString modelType;  ///< Always `"nemo_transducer"`.

    /// The canonical bundle directory name.
    static QString bundleName();

    /// Canonicalises a profile name. Returns an empty string for anything but
    /// `"parakeet"` (and its aliases) or `"stub"`; `"moonshine"` is rejected.
    static QString canonicalProfile(const QString &raw);

    /// Canonical profile names in preference order: just `parakeet`.
    static QStringList supportedProfiles();

    /// Locates and validates the Parakeet files below `modelDir`.
    /// `modelDir` may be the base directory or the bundle directory itself.
    static std::optional<ModelLayout> resolve(const QString &modelDir,
                                              const QString &vadModel,
                                              QString *error = nullptr);
};

} // namespace dex
