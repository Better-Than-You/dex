#include "engine/TextNormalizer.hpp"

#include <QRegularExpression>
#include <QVector>

#include <utility>

namespace dex {
namespace {

const QVector<std::pair<QRegularExpression, QString>> &spokenMathRules()
{
    // Applied in order. Function names and `l.n.e` come first so later
    // punctuation handling cannot destroy them.
    static const QVector<std::pair<QRegularExpression, QString>> rules = {
        {QRegularExpression(QStringLiteral(R"(\bl\.?n\.?\s*\(\s*e\s*\))")), QStringLiteral("ln(e)")},
        {QRegularExpression(QStringLiteral(R"(\bl\.?n\.?e\b)")), QStringLiteral("ln(e)")},
        {QRegularExpression(QStringLiteral(R"(\bl\.?n\.?\b)")), QStringLiteral("ln")},
        {QRegularExpression(QStringLiteral(R"(\blog\b)")), QStringLiteral("log")},
        {QRegularExpression(QStringLiteral(R"(\b(?:multiplied by|times|into)\b)")), QStringLiteral("*")},
        {QRegularExpression(QStringLiteral(R"(\b(?:divided by|over|by)\b)")), QStringLiteral("/")},
        {QRegularExpression(QStringLiteral(R"(\bplus\b)")), QStringLiteral("+")},
        {QRegularExpression(QStringLiteral(R"(\b(?:minus|negative)\b)")), QStringLiteral("-")},
        {QRegularExpression(QStringLiteral(R"(\b(?:to the power of|power|raised to)\b)")), QStringLiteral("^")},
        {QRegularExpression(QStringLiteral(R"(\bsquared\b)")), QStringLiteral("^2")},
        {QRegularExpression(QStringLiteral(R"(\bcubed\b)")), QStringLiteral("^3")},
        {QRegularExpression(QStringLiteral(R"(\b(?:square root of|root)\b)")), QStringLiteral("sqrt")},
        {QRegularExpression(QStringLiteral(R"(\bpi\b)")), QStringLiteral("3.141592653589793")},
        // Guarded so the `e` inside `ln(e)` is not expanded to 2.718...
        {QRegularExpression(QStringLiteral(R"((?<![\w.(])e(?![\w.)]))")),
         QStringLiteral("2.718281828459045")},
    };
    return rules;
}

QString canonicalizeSpacing(QString text)
{
    static const QRegularExpression whitespace(QStringLiteral(R"(\s+)"));
    text.replace(whitespace, QStringLiteral(" "));
    text = text.trimmed();

    // Remove the gap a spoken unary sign leaves behind: "- 2" -> "-2",
    // "(- 2" -> "(-2", "* - 2" -> "*-2". Binary operators keep their spaces.
    static const QRegularExpression unaryGap(
        QStringLiteral(R"((^|[+\-*/^%(])\s*([+-])\s+)"));
    text.replace(unaryGap, QStringLiteral("\\1\\2"));

    // Tighten parentheses and function calls.
    text.replace(QStringLiteral("( "), QStringLiteral("("));
    text.replace(QStringLiteral(" )"), QStringLiteral(")"));
    static const QRegularExpression callGap(
        QStringLiteral(R"(\b(ln|log|sin|cos|tan|sqrt)\s+\()"));
    text.replace(callGap, QStringLiteral("\\1("));

    // `5 ^2` -> `5 ^ 2` for readable maths cards.
    static const QRegularExpression powerGap(QStringLiteral(R"(\s*\^\s*)"));
    text.replace(powerGap, QStringLiteral(" ^ "));

    return text.trimmed();
}

} // namespace

QString TextNormalizer::normalize(const QString &raw)
{
    QString text = raw.toLower().trimmed();

    for (const auto &[pattern, replacement] : spokenMathRules())
        text.replace(pattern, replacement);

    // Trailing punctuation only; decimal points between digits are untouched
    // because they are never the last character of a number.
    static const QRegularExpression trailingPunctuation(QStringLiteral(R"([.?!,]+\s*$)"));
    text.remove(trailingPunctuation);

    // Wake prefixes are stripped late so `hey dex, ...` (with comma) works.
    static const QRegularExpression wakePrefix(
        QStringLiteral(R"(^(?:hey dex|dex|please|can you)[\s,]+)"));
    text.remove(wakePrefix);

    return canonicalizeSpacing(text);
}

} // namespace dex
