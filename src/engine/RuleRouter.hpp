#pragma once

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>
#include <string_view>

namespace dex {

struct CommandRule {
    QString name;
    QStringList patterns;
    QString type = QStringLiteral("exec"); ///< "url" or "exec"
    QString exec;
    QStringList args;
    std::optional<bool> confirm;
    bool shell = false;
};

struct RouteResult {
    int ruleIndex = -1;
    QString ruleName;
    QString type;
    QString program;
    QStringList args;
    bool confirm = false;
    QString label;
    QString detail;
};

/// Matches normalized utterances against `commands.toml` rules top-to-bottom.
class RuleRouter
{
public:
    bool load(const QString &path, QString *error = nullptr);
    bool loadFromToml(std::string_view tomlText, QString *error = nullptr);

    /// `confirmByDefault` is applied when a rule does not specify `confirm`.
    std::optional<RouteResult> route(const QString &text, bool confirmByDefault) const;

    const QVector<CommandRule> &rules() const { return m_rules; }

    /// Resolves `$BROWSER` (falling back to `xdg-open`) and other tokens.
    static QString resolveProgram(const QString &exec);

    /// Replaces `%{slot}` tokens. When `encode` is true each value is
    /// percent-encoded (required for URL query slots).
    static QString substitute(const QString &tmpl, const QHash<QString, QString> &captures, bool encode);

private:
    QVector<CommandRule> m_rules;
};

} // namespace dex
