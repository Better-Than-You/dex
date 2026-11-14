#include "engine/TextNormalizer.hpp"

#include <QHash>
#include <QRegularExpression>
#include <QStringList>
#include <QVector>

#include <utility>

namespace dex {
namespace {

const QVector<std::pair<QRegularExpression, QString>> &spokenMathRules()
{
    // Applied in order. Function names and `l.n.e` come first so later
    // punctuation handling cannot destroy them.
    static const QVector<std::pair<QRegularExpression, QString>> rules = {
        {QRegularExpression(QStringLiteral(R"(\b(?:uh|uhm|um|umm|erm)\b)")), QString()},
        // `minus. is 0.8` -> `-0.8` (ASR splits the sign from the number).
        {QRegularExpression(QStringLiteral(R"(\bminus\s*[.,]?\s*(?:is|equals|equal to)\b)")),
         QStringLiteral("-")},
        {QRegularExpression(QStringLiteral(R"(\bsigma\b)")), QStringLiteral("sigmoid")},
        {QRegularExpression(QStringLiteral(R"(\bsoftness\b)")), QStringLiteral("softmax")},
        {QRegularExpression(QStringLiteral(R"(\by\s+hat\b)")), QStringLiteral("y_hat")},
        {QRegularExpression(QStringLiteral(R"(\by\s+bar\b)")), QStringLiteral("y_bar")},
        {QRegularExpression(QStringLiteral(R"(\bl\.?n\.?\s*\(\s*e\s*\))")), QStringLiteral("ln(e)")},
        {QRegularExpression(QStringLiteral(R"(\bl\.?n\.?e\b)")), QStringLiteral("ln(e)")},
        {QRegularExpression(QStringLiteral(R"(\bl\.?n\.?\b)")), QStringLiteral("ln")},
        {QRegularExpression(QStringLiteral(R"(\blog\b)")), QStringLiteral("log")},
        {QRegularExpression(QStringLiteral(R"(\bexp\b)")), QStringLiteral("exp")},
        {QRegularExpression(QStringLiteral(R"(\b(?:multiplied by|times|into)\b)")), QStringLiteral("*")},
        {QRegularExpression(QStringLiteral(R"(\b(?:divided by|over|by)\b)")), QStringLiteral("/")},
        {QRegularExpression(QStringLiteral(R"(\bplus\b)")), QStringLiteral("+")},
        {QRegularExpression(QStringLiteral(R"(\b(?:minus|negative)\b)")), QStringLiteral("-")},
        {QRegularExpression(QStringLiteral(R"(\b(?:to the power of|power|raised to)\b)")), QStringLiteral("^")},
        {QRegularExpression(QStringLiteral(R"(\bsquared\b)")), QStringLiteral("^2")},
        {QRegularExpression(QStringLiteral(R"(\bcubed\b)")), QStringLiteral("^3")},
        {QRegularExpression(QStringLiteral(R"(\b(?:square root of|root)\b)")), QStringLiteral("sqrt")},
        // `y hat minus y whole square` -> `(y_hat - y)^2`
        {QRegularExpression(QStringLiteral(R"(\b([a-z_][a-z0-9_]*(?:\s*[-+]\s*[a-z_][a-z0-9_]*)+)\s+whole\s+square\b)")),
         QStringLiteral("(\\1)^2")},
        {QRegularExpression(QStringLiteral(R"(\bpi\b)")), QStringLiteral("3.141592653589793")},
        // Guarded so the `e` inside `ln(e)` is not expanded to 2.718...
        {QRegularExpression(QStringLiteral(R"((?<![\w.(])e(?![\w.)]))")),
         QStringLiteral("2.718281828459045")},
        {QRegularExpression(QStringLiteral(R"(\bfunction\s+(?:of|for)\b)")), QString()},
        // `ln2` -> `ln(2)` so bare ASR tokens become explicit calls.
        {QRegularExpression(QStringLiteral(R"(\bln\s*([0-9]+(?:\.[0-9]+)?)\b)")),
         QStringLiteral("ln(\\1)")},
    };
    return rules;
}

QString wordDigit(const QString &word)
{
    static const QHash<QString, QString> digits = {
        {QStringLiteral("zero"), QStringLiteral("0")},   {QStringLiteral("one"), QStringLiteral("1")},
        {QStringLiteral("two"), QStringLiteral("2")},    {QStringLiteral("three"), QStringLiteral("3")},
        {QStringLiteral("four"), QStringLiteral("4")},   {QStringLiteral("five"), QStringLiteral("5")},
        {QStringLiteral("six"), QStringLiteral("6")},    {QStringLiteral("seven"), QStringLiteral("7")},
        {QStringLiteral("eight"), QStringLiteral("8")},  {QStringLiteral("nine"), QStringLiteral("9")},
    };
    return digits.value(word, word);
}

QString replaceNumberWords(QString text)
{
    static const QRegularExpression decimalPattern(
        QStringLiteral(R"(\b(zero|one|two|three|four|five|six|seven|eight|nine)\s+point\s+(zero|one|two|three|four|five|six|seven|eight|nine)\b)"));

    QRegularExpressionMatch match = decimalPattern.match(text);
    while (match.hasMatch()) {
        const QString replacement =
            wordDigit(match.captured(1)) + QLatin1Char('.') + wordDigit(match.captured(2));
        text.replace(match.capturedStart(), match.capturedLength(), replacement);
        match = decimalPattern.match(text);
    }

    static const QHash<QString, QString> words = {
        {QStringLiteral("zero"), QStringLiteral("0")},      {QStringLiteral("one"), QStringLiteral("1")},
        {QStringLiteral("two"), QStringLiteral("2")},       {QStringLiteral("three"), QStringLiteral("3")},
        {QStringLiteral("four"), QStringLiteral("4")},      {QStringLiteral("five"), QStringLiteral("5")},
        {QStringLiteral("six"), QStringLiteral("6")},       {QStringLiteral("seven"), QStringLiteral("7")},
        {QStringLiteral("eight"), QStringLiteral("8")},     {QStringLiteral("nine"), QStringLiteral("9")},
        {QStringLiteral("ten"), QStringLiteral("10")},      {QStringLiteral("half"), QStringLiteral("0.5")},
        {QStringLiteral("quarter"), QStringLiteral("0.25")},
    };

    for (auto it = words.cbegin(); it != words.cend(); ++it) {
        text.replace(QRegularExpression(QStringLiteral("\\b%1\\b").arg(it.key())), it.value());
    }
    return text;
}

bool isMathContext(const QString &text)
{
    static const QRegularExpression mathWords(
        QStringLiteral(R"(\b(?:calculate|solve|evaluate|what is|derivative|differentiat\w*|respect to|wrt|minus|plus|times|into|multiplied|divided|over|squared|cubed|power|sqrt|root|ln|log|sigmoid|softmax|relu|exp|vector|sigma|softness|modulo|percent|whole square)\b)"));
    return mathWords.match(text).hasMatch();
}

/// Builds a canonical `softmax([...])` call from a spoken vector phrase such
/// as "softness function for the vector one, zero point one comma minus three".
QString buildSoftmax(const QString &text)
{
    if (!text.contains(QStringLiteral("softness")) && !text.contains(QStringLiteral("softmax")))
        return {};
    if (text.contains(QStringLiteral("softmax([")))
        return {};

    const int vectorIndex = text.indexOf(QStringLiteral("vector"));
    if (vectorIndex < 0)
        return {};

    QString tail = text.mid(vectorIndex + 6);
    tail.replace(QStringLiteral("function"), QString());
    tail.replace(QStringLiteral("comma"), QStringLiteral(","));

    static const QRegularExpression filler(QStringLiteral(R"(\b(?:uh|um|erm|the|for|of|a)\b)"));
    tail.remove(filler);
    tail.replace(QRegularExpression(QStringLiteral(R"(\bminus\b)")), QStringLiteral("-"));
    tail = replaceNumberWords(tail);

    QStringList parts;
    const QStringList rawParts = tail.split(QLatin1Char(','));
    for (const QString &rawPart : rawParts) {
        QString part = rawPart.trimmed();
        if (part.isEmpty())
            continue;
        part.replace(QRegularExpression(QStringLiteral(R"(^([+-])\s+)")), QStringLiteral("\\1"));
        static const QRegularExpression numeric(
            QStringLiteral(R"(^[+-]?(?:\d+(?:\.\d+)?|pi|e)$)"));
        if (!numeric.match(part).hasMatch())
            return {};
        parts.append(part);
    }

    if (parts.size() < 2)
        return {};
    return QStringLiteral("softmax([%1])").arg(parts.join(QStringLiteral(", ")));
}

QString canonicalizeSpacing(QString text)
{
    static const QRegularExpression whitespace(QStringLiteral(R"(\s+)"));
    text.replace(whitespace, QStringLiteral(" "));
    text = text.trimmed();

    // Remove the gap a spoken unary sign leaves behind: "- 2" -> "-2",
    // "(- 2" -> "(-2", "* - 2" -> "*-2". Binary operators keep their spaces.
    static const QRegularExpression unaryGap(
        QStringLiteral(R"((^|[+\-*/^%(\[])\s*([+-])\s+)"));
    text.replace(unaryGap, QStringLiteral("\\1\\2"));

    // Tighten parentheses and function calls.
    text.replace(QStringLiteral("( "), QStringLiteral("("));
    text.replace(QStringLiteral(" )"), QStringLiteral(")"));
    static const QRegularExpression callGap(
        QStringLiteral(R"(\b(ln|log|sin|cos|tan|sqrt|abs|sigmoid|relu|exp|softmax)\s+\()"));
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

    // Vector construction needs the raw phrase order, so handle it first.
    const QString softmax = buildSoftmax(text);
    if (!softmax.isEmpty())
        return softmax;

    for (const auto &[pattern, replacement] : spokenMathRules())
        text.replace(pattern, replacement);

    if (isMathContext(raw.toLower()))
        text = replaceNumberWords(text);

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

std::optional<DerivativeRequest> TextNormalizer::derivative(const QString &raw)
{
    QString text = raw.toLower().trimmed();
    text.remove(QRegularExpression(QStringLiteral(R"(\b(?:uh|uhm|um|umm|erm)\b)")));

    static const QRegularExpression marker(
        QStringLiteral(R"(\b(?:in|with)\s+respect\s+to\b|\bwrt\b)"));
    const QRegularExpressionMatch match = marker.match(text);
    if (!match.hasMatch())
        return std::nullopt;

    QString expressionPart = text.left(match.capturedStart()).trimmed();
    QString variablePart = text.mid(match.capturedEnd()).trimmed();

    static const QRegularExpression variableJunk(QStringLiteral(R"([.?!,]+\s*$)"));
    variablePart.remove(variableJunk);

    static const QRegularExpression derivativePrefix(
        QStringLiteral(R"(^(?:calculate\s+)?(?:the\s+)?(?:derivative\s+of|differentiate|d\s*/\s*d\s*[a-z_]*)\s*)"));
    expressionPart.remove(derivativePrefix);

    DerivativeRequest request;
    request.variable = normalize(variablePart);
    request.expression = normalize(expressionPart);

    if (request.variable.isEmpty() || request.expression.isEmpty()
        || request.variable.contains(QLatin1Char(' '))) {
        return std::nullopt;
    }
    return request;
}

} // namespace dex
