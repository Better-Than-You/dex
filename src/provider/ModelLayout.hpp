#pragma once

#include <QString>
#include <QStringList>

#include <optional>

namespace dex {

/// Resolved on-disk layout for one sherpa-onnx ASR profile.
///
/// Dex stores every model bundle below a single base directory
/// (`~/.local/share/dex/models`) so switching profiles only changes which
/// files the recognizer is pointed at.
struct ModelLayout {
    QString profile;    ///< Canonical profile name: "moonshine" or "parakeet".
    QString directory;  ///< Directory that contains the model files.
    QString encoder;
    QString decoder;
    QString joiner;     ///< Empty for Moonshine.
    QString tokens;
    QString vadModel;
    QString modelType;  ///< Passed to sherpa-onnx; empty means auto-detect.

    /// Canonicalises a profile name. Returns an empty string for unknown
    /// profiles. Legacy aliases such as "balanced" map to "moonshine".
    static QString canonicalProfile(const QString &raw);

    /// Canonical profile names in preference order.
    static QStringList supportedProfiles();

    /// Locates and validates the files for `profile` below `modelDir`.
    /// `modelDir` may be the base directory or the profile directory itself.
    static std::optional<ModelLayout> resolve(const QString &profile,
                                              const QString &modelDir,
                                              const QString &vadModel,
                                              QString *error = nullptr);
};

} // namespace dex
