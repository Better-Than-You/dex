#pragma once

#include <QString>

#include <optional>
#include <string_view>

namespace dex {

struct GeneralConfig {
    bool daemon = true;
    bool confirmByDefault = false;
    int autoHideMs = 2200;
};

struct AsrConfig {
    /// "sherpa" (default, Parakeet single-shot) or "stub". `--stub` overrides
    /// this at runtime.
    QString provider = QStringLiteral("sherpa");
    /// Base directory that holds the Parakeet bundle and `silero_vad.onnx`.
    QString modelDir;
    /// Optional explicit VAD model path. Empty -> `<modelDir>/silero_vad.onnx`.
    QString vadModel;
    int threads = 4;
    int silenceTimeoutMs = 800;
    double vadThreshold = 0.5;
};

/// Phase 2.1: on-demand llama.cpp translator for noisy spoken maths/calculus.
/// The model is never loaded at startup; it is paged in on the first maths
/// query and released again after `idleUnloadSec` of inactivity.
struct MathSlmConfig {
    bool enabled = true;
    QString modelPath;
    int threads = 4;
    int idleUnloadSec = 60;
};

struct UiConfig {
    QString anchor = QStringLiteral("top");
    int marginTop = 48;
    int width = 460;
};

struct Config {
    GeneralConfig general;
    AsrConfig asr;
    MathSlmConfig mathSlm;
    UiConfig ui;

    /// Loads `config.toml`. Missing keys keep their defaults. Returns the
    /// default-constructed config (with an error message) when the file
    /// cannot be read or parsed.
    static std::optional<Config> load(const QString &path, QString *error = nullptr);

    /// Parses TOML from memory. Useful for tests and for embedding defaults.
    static std::optional<Config> fromToml(std::string_view tomlText, QString *error = nullptr);
};

/// `$XDG_CONFIG_HOME/dex/config.toml` or `~/.config/dex/config.toml`.
QString defaultConfigPath();

/// `$XDG_CONFIG_HOME/dex/commands.toml` or `~/.config/dex/commands.toml`.
QString defaultCommandsPath();

/// `$XDG_DATA_HOME/dex/models` or `~/.local/share/dex/models`.
QString defaultModelDir();

/// `$XDG_DATA_HOME/dex/models/Qwen3.5-0.8B-Q4_K_M.gguf`.
QString defaultSlmModelPath();

} // namespace dex

