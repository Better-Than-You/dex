#include "core/Config.hpp"

#include <QDir>
#include <QFile>

#include <toml++/toml.hpp>

#include <algorithm>
#include <string>
#include <string_view>

namespace dex {
namespace {

template <typename T>
T valueOr(const toml::table &table, std::string_view key, T fallback)
{
    if (const toml::node *node = table.get(key)) {
        if (auto value = node->value<T>())
            return *value;
    }
    return fallback;
}

template <typename T>
T clamped(T value, T low, T high)
{
    return std::clamp(value, low, high);
}

QString stringOr(const toml::table &table, std::string_view key, const QString &fallback)
{
    if (const toml::node *node = table.get(key)) {
        if (auto value = node->value<std::string>())
            return QString::fromStdString(*value);
    }
    return fallback;
}

QString expandPath(const QString &path)
{
    if (path == QStringLiteral("~"))
        return QDir::homePath();
    if (path.startsWith(QStringLiteral("~/")))
        return QDir::home().filePath(path.mid(2));
    return path;
}

void readGeneral(const toml::table &table, GeneralConfig &general)
{
    if (const auto *node = table["general"].as_table()) {
        general.daemon = valueOr(*node, "daemon", general.daemon);
        general.confirmByDefault = valueOr(*node, "confirm_by_default", general.confirmByDefault);
        general.autoHideMs = valueOr(*node, "auto_hide_ms", general.autoHideMs);
    }
}

void readAsr(const toml::table &table, AsrConfig &asr)
{
    if (const auto *node = table["asr"].as_table()) {
        const QString provider = stringOr(*node, "provider", asr.provider);
        if (!provider.isEmpty())
            asr.provider = provider;

        const QString modelDir = stringOr(*node, "model_dir", asr.modelDir);
        if (!modelDir.isEmpty())
            asr.modelDir = expandPath(modelDir);

        const QString vadModel = stringOr(*node, "vad_model", asr.vadModel);
        if (!vadModel.isEmpty())
            asr.vadModel = expandPath(vadModel);

        asr.threads = clamped(valueOr(*node, "threads", asr.threads), 1, 16);
        asr.silenceTimeoutMs =
            clamped(valueOr(*node, "silence_timeout_ms", asr.silenceTimeoutMs), 100, 5000);
        asr.vadThreshold =
            clamped(valueOr(*node, "vad_threshold", asr.vadThreshold), 0.05, 0.95);
    }

    if (asr.modelDir.isEmpty())
        asr.modelDir = defaultModelDir();
    if (asr.vadModel.isEmpty())
        asr.vadModel = QDir(asr.modelDir).filePath(QStringLiteral("silero_vad.onnx"));
}

void readMathSlm(const toml::table &table, MathSlmConfig &slm)
{
    if (const auto *node = table["math_slm"].as_table()) {
        slm.enabled = valueOr(*node, "enabled", slm.enabled);

        const QString modelPath = stringOr(*node, "model_path", slm.modelPath);
        if (!modelPath.isEmpty())
            slm.modelPath = expandPath(modelPath);

        slm.threads = clamped(valueOr(*node, "threads", slm.threads), 1, 16);
        slm.idleUnloadSec =
            clamped(valueOr(*node, "idle_unload_sec", slm.idleUnloadSec), 0, 3600);
    }

    if (slm.modelPath.isEmpty())
        slm.modelPath = defaultSlmModelPath();
}

void readUi(const toml::table &table, UiConfig &ui)
{
    if (const auto *node = table["ui"].as_table()) {
        const QString anchor = stringOr(*node, "anchor", ui.anchor);
        if (!anchor.isEmpty())
            ui.anchor = anchor;
        ui.marginTop = valueOr(*node, "margin_top", ui.marginTop);
        ui.width = valueOr(*node, "width", ui.width);
    }
}

Config readConfig(const toml::table &table)
{
    Config config;
    readGeneral(table, config.general);
    readAsr(table, config.asr);
    readMathSlm(table, config.mathSlm);
    readUi(table, config.ui);
    return config;
}

QString configHome()
{
    const QString xdg = qEnvironmentVariable("XDG_CONFIG_HOME");
    if (!xdg.isEmpty())
        return xdg;
    return QDir::home().filePath(QStringLiteral(".config"));
}

} // namespace

std::optional<Config> Config::fromToml(std::string_view tomlText, QString *error)
{
    try {
        const std::string text(tomlText);
        const toml::table table = toml::parse(text);
        return readConfig(table);
    } catch (const toml::parse_error &parseError) {
        if (error) {
            *error = QStringLiteral("Failed to parse config: %1")
                         .arg(QString::fromStdString(std::string(parseError.description())));
        }
        return std::nullopt;
    }
}

std::optional<Config> Config::load(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error)
            *error = QStringLiteral("Cannot open %1: %2").arg(path, file.errorString());
        return std::nullopt;
    }

    const QByteArray data = file.readAll();
    return fromToml(std::string_view(data.constData(), static_cast<size_t>(data.size())), error);
}

QString defaultConfigPath()
{
    return QDir(configHome()).filePath(QStringLiteral("dex/config.toml"));
}

QString defaultCommandsPath()
{
    return QDir(configHome()).filePath(QStringLiteral("dex/commands.toml"));
}

QString defaultModelDir()
{
    const QString xdg = qEnvironmentVariable("XDG_DATA_HOME");
    if (!xdg.isEmpty())
        return QDir(xdg).filePath(QStringLiteral("dex/models"));
    return QDir::home().filePath(QStringLiteral(".local/share/dex/models"));
}

QString defaultSlmModelPath()
{
    return QDir(defaultModelDir()).filePath(QStringLiteral("Qwen3.5-0.8B-Q4_K_M.gguf"));
}

} // namespace dex
