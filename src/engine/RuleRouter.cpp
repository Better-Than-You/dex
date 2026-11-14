#include "engine/RuleRouter.hpp"

#include <QFile>
#include <QProcess>
#include <QRegularExpression>
#include <QUrl>

#include <toml++/toml.hpp>

#include <string>
#include <string_view>

namespace dex {
namespace {

template <typename Node>
QString stringValue(const Node &node)
{
    if (auto value = node.template value<std::string>())
        return QString::fromStdString(*value);
    return {};
}

QString labelForRule(const QString &name)
{
    static const QHash<QString, QString> labels = {
        {QStringLiteral("web_search"), QStringLiteral("Search")},
        {QStringLiteral("youtube_search"), QStringLiteral("YouTube")},
        {QStringLiteral("launch_app"), QStringLiteral("Launch")},
        {QStringLiteral("hypr_workspace"), QStringLiteral("Workspace")},
    };
    return labels.value(name, name);
}

bool parseRule(const toml::table &table, CommandRule &rule, QString *error)
{
    rule.name = stringValue(table["name"]);
    if (rule.name.isEmpty()) {
        if (error)
            *error = QStringLiteral("Command rule is missing a `name`.");
        return false;
    }

    if (const auto *patterns = table["patterns"].as_array()) {
        for (const auto &pattern : *patterns)
            rule.patterns.append(stringValue(pattern));
    }
    if (rule.patterns.isEmpty()) {
        if (error)
            *error = QStringLiteral("Rule `%1` has no patterns.").arg(rule.name);
        return false;
    }

    if (auto type = stringValue(table["type"]); !type.isEmpty())
        rule.type = type;
    if (rule.type != QStringLiteral("url") && rule.type != QStringLiteral("exec")) {
        if (error) {
            *error = QStringLiteral("Rule `%1` has unsupported type `%2` (expected url|exec).")
                         .arg(rule.name, rule.type);
        }
        return false;
    }

    rule.exec = stringValue(table["exec"]);
    if (rule.exec.isEmpty()) {
        if (error)
            *error = QStringLiteral("Rule `%1` has no `exec`.").arg(rule.name);
        return false;
    }

    if (const auto *args = table["args"].as_array()) {
        for (const auto &arg : *args)
            rule.args.append(stringValue(arg));
    }

    if (auto confirm = table["confirm"].value<bool>())
        rule.confirm = *confirm;
    rule.shell = table["shell"].value_or(false);

    for (const QString &pattern : rule.patterns) {
        const QRegularExpression expression(pattern);
        if (!expression.isValid()) {
            if (error) {
                *error = QStringLiteral("Rule `%1` has an invalid pattern `%2`: %3")
                             .arg(rule.name, pattern, expression.errorString());
            }
            return false;
        }
    }

    return true;
}

} // namespace

bool RuleRouter::load(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error)
            *error = QStringLiteral("Cannot open %1: %2").arg(path, file.errorString());
        return false;
    }
    const QByteArray data = file.readAll();
    return loadFromToml(std::string_view(data.constData(), static_cast<size_t>(data.size())), error);
}

bool RuleRouter::loadFromToml(std::string_view tomlText, QString *error)
{
    m_rules.clear();

    toml::table document;
    try {
        document = toml::parse(tomlText);
    } catch (const toml::parse_error &parseError) {
        if (error) {
            *error = QStringLiteral("Failed to parse commands: %1")
                         .arg(QString::fromStdString(std::string(parseError.description())));
        }
        return false;
    }

    const auto *commands = document["commands"].as_array();
    if (!commands) {
        if (error)
            *error = QStringLiteral("commands.toml contains no [[commands]] entries.");
        return false;
    }

    QVector<CommandRule> parsed;
    parsed.reserve(static_cast<int>(commands->size()));
    for (const auto &node : *commands) {
        const auto *table = node.as_table();
        if (!table) {
            if (error)
                *error = QStringLiteral("Every `commands` entry must be a table.");
            return false;
        }
        CommandRule rule;
        if (!parseRule(*table, rule, error))
            return false;
        parsed.append(rule);
    }

    m_rules = std::move(parsed);
    return true;
}

std::optional<RouteResult> RuleRouter::route(const QString &text, bool confirmByDefault) const
{
    for (int index = 0; index < m_rules.size(); ++index) {
        const CommandRule &rule = m_rules.at(index);

        for (const QString &pattern : rule.patterns) {
            const QRegularExpression expression(pattern);
            const QRegularExpressionMatch match = expression.match(text);
            if (!match.hasMatch())
                continue;

            QHash<QString, QString> captures;
            QStringList detailParts;
            for (const QString &name : expression.namedCaptureGroups()) {
                if (name.isEmpty())
                    continue;
                const QString value = match.captured(name);
                captures.insert(name, value);
                detailParts.append(value);
            }

            const bool encode = rule.type == QStringLiteral("url");

            RouteResult result;
            result.ruleIndex = index;
            result.ruleName = rule.name;
            result.type = rule.type;
            result.label = labelForRule(rule.name);
            result.detail = detailParts.join(QStringLiteral(" · "));
            result.confirm = rule.confirm.value_or(confirmByDefault);

            const QString execTemplate = substitute(rule.exec, captures, encode);
            QStringList arguments;
            for (const QString &arg : rule.args)
                arguments.append(substitute(arg, captures, encode));

            if (rule.shell) {
                // Explicitly opted-in shell execution.
                result.program = QStringLiteral("/bin/sh");
                result.args = {QStringLiteral("-c"),
                               execTemplate + (arguments.isEmpty()
                                                   ? QString()
                                                   : QStringLiteral(" ") + arguments.join(QLatin1Char(' ')))};
            } else {
                // `$BROWSER` may contain arguments (e.g. "firefox --new-window").
                const QString resolved = resolveProgram(execTemplate);
                QStringList programParts = QProcess::splitCommand(resolved);
                if (programParts.isEmpty()) {
                    result.program = resolved;
                } else {
                    result.program = programParts.takeFirst();
                    if (!programParts.isEmpty())
                        arguments = programParts + arguments;
                }
                result.args = arguments;
            }

            return result;
        }
    }

    return std::nullopt;
}

QString RuleRouter::resolveProgram(const QString &exec)
{
    const QString trimmed = exec.trimmed();
    if (trimmed == QStringLiteral("$BROWSER") || trimmed == QStringLiteral("${BROWSER}")) {
        const QString browser = qEnvironmentVariable("BROWSER").trimmed();
        return browser.isEmpty() ? QStringLiteral("xdg-open") : browser;
    }
    return trimmed;
}

QString RuleRouter::substitute(const QString &tmpl, const QHash<QString, QString> &captures, bool encode)
{
    QString output = tmpl;
    for (auto it = captures.cbegin(); it != captures.cend(); ++it) {
        const QString value = encode
            ? QString::fromUtf8(QUrl::toPercentEncoding(it.value()))
            : it.value();
        output.replace(QStringLiteral("%{") + it.key() + QStringLiteral("}"), value);
    }
    return output;
}

} // namespace dex
