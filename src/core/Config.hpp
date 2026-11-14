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
    QString modelProfile = QStringLiteral("balanced");
    int threads = 4;
    int silenceTimeoutMs = 800;
};

struct UiConfig {
    QString anchor = QStringLiteral("top");
    int marginTop = 48;
    int width = 460;
};

struct Config {
    GeneralConfig general;
    AsrConfig asr;
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

} // namespace dex
