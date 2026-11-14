#include "engine/MathEvaluator.hpp"

#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include <QVector>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <utility>
#include <vector>

namespace dex {
namespace {

struct Value {
    bool isVector = false;
    double scalar = 0.0;
    std::vector<double> values;
};

Value makeScalar(double value)
{
    Value result;
    result.scalar = value;
    return result;
}

Value makeVector(std::vector<double> values)
{
    Value result;
    result.isVector = true;
    result.values = std::move(values);
    return result;
}

// ---------------------------------------------------------------------------
// Tokenizer
// ---------------------------------------------------------------------------

struct Token {
    enum Type { Number, Identifier, Operator, LParen, RParen, LBracket, RBracket, Comma, End };

    Type type = End;
    QString text;
    double number = 0.0;
};

QVector<Token> tokenize(const QString &input)
{
    QVector<Token> tokens;
    tokens.reserve(input.size() + 1);

    int i = 0;
    const int size = input.size();
    while (i < size) {
        const QChar c = input.at(i);

        if (c.isSpace()) {
            ++i;
            continue;
        }

        if (c.isDigit() || (c == QLatin1Char('.') && i + 1 < size && input.at(i + 1).isDigit())) {
            const int start = i;
            bool seenDot = false;
            while (i < size) {
                const QChar d = input.at(i);
                if (d.isDigit()) {
                    ++i;
                } else if (d == QLatin1Char('.') && !seenDot) {
                    seenDot = true;
                    ++i;
                } else {
                    break;
                }
            }
            Token token;
            token.type = Token::Number;
            token.text = input.mid(start, i - start);
            token.number = token.text.toDouble();
            tokens.push_back(token);
            continue;
        }

        if (c.isLetter()) {
            const int start = i;
            while (i < size && input.at(i).isLetter())
                ++i;
            Token token;
            token.type = Token::Identifier;
            token.text = input.mid(start, i - start).toLower();
            tokens.push_back(token);
            continue;
        }

        switch (c.unicode()) {
        case '(':
            tokens.push_back({Token::LParen, QStringLiteral("("), 0.0});
            ++i;
            continue;
        case ')':
            tokens.push_back({Token::RParen, QStringLiteral(")"), 0.0});
            ++i;
            continue;
        case '[':
            tokens.push_back({Token::LBracket, QStringLiteral("["), 0.0});
            ++i;
            continue;
        case ']':
            tokens.push_back({Token::RBracket, QStringLiteral("]"), 0.0});
            ++i;
            continue;
        case ',':
            tokens.push_back({Token::Comma, QStringLiteral(","), 0.0});
            ++i;
            continue;
        default:
            break;
        }

        if (QStringLiteral("+-*/^%").contains(c)) {
            tokens.push_back({Token::Operator, QString(c), 0.0});
            ++i;
            continue;
        }

        // Unknown character: encode as a sentinel the parser will reject.
        tokens.push_back({Token::Operator, QStringLiteral("?"), 0.0});
        ++i;
    }

    tokens.push_back({Token::End, QString(), 0.0});
    return tokens;
}

bool isFunctionName(const QString &name)
{
    static const QSet<QString> functions = {
        QStringLiteral("ln"),   QStringLiteral("log"),     QStringLiteral("sin"),
        QStringLiteral("cos"),  QStringLiteral("tan"),     QStringLiteral("sqrt"),
        QStringLiteral("abs"),  QStringLiteral("sigmoid"), QStringLiteral("sigma"),
        QStringLiteral("relu"), QStringLiteral("exp"),     QStringLiteral("softmax"),
    };
    return functions.contains(name);
}

// ---------------------------------------------------------------------------
// Value arithmetic
// ---------------------------------------------------------------------------

Value applyUnary(const QString &name, const Value &argument, bool &ok)
{
    if (!ok)
        return makeScalar(0.0);

    if (name == QStringLiteral("softmax")) {
        std::vector<double> values =
            argument.isVector ? argument.values : std::vector<double>{argument.scalar};
        if (values.empty()) {
            ok = false;
            return makeScalar(0.0);
        }

        // Numerically stable max-shifted exponential.
        const double maximum = *std::max_element(values.begin(), values.end());
        double sum = 0.0;
        for (double &value : values) {
            value = std::exp(value - maximum);
            sum += value;
        }
        if (!(sum > 0.0) || !std::isfinite(sum)) {
            ok = false;
            return makeScalar(0.0);
        }
        for (double &value : values)
            value /= sum;

        return argument.isVector ? makeVector(std::move(values)) : makeScalar(values.front());
    }

    const auto scalarFn = [&name](double x) -> double {
        if (name == QStringLiteral("ln"))
            return std::log(x);
        if (name == QStringLiteral("log"))
            return std::log10(x);
        if (name == QStringLiteral("sin"))
            return std::sin(x);
        if (name == QStringLiteral("cos"))
            return std::cos(x);
        if (name == QStringLiteral("tan"))
            return std::tan(x);
        if (name == QStringLiteral("sqrt"))
            return std::sqrt(x);
        if (name == QStringLiteral("abs"))
            return std::abs(x);
        if (name == QStringLiteral("sigmoid") || name == QStringLiteral("sigma"))
            return 1.0 / (1.0 + std::exp(-x));
        if (name == QStringLiteral("relu"))
            return std::max(0.0, x);
        if (name == QStringLiteral("exp"))
            return std::exp(x);
        return std::numeric_limits<double>::quiet_NaN();
    };

    if (argument.isVector) {
        std::vector<double> values = argument.values;
        for (double &value : values)
            value = scalarFn(value);
        return makeVector(std::move(values));
    }
    return makeScalar(scalarFn(argument.scalar));
}

Value applyBinary(QChar op, const Value &lhs, const Value &rhs, bool &ok)
{
    if (!ok)
        return makeScalar(0.0);

    const auto scalarOp = [op](double a, double b) -> double {
        switch (op.unicode()) {
        case '+':
            return a + b;
        case '-':
            return a - b;
        case '*':
            return a * b;
        case '/':
            return a / b;
        case '%':
            return std::fmod(a, b);
        case '^':
            return std::pow(a, b);
        default:
            return std::numeric_limits<double>::quiet_NaN();
        }
    };

    if (!lhs.isVector && !rhs.isVector) {
        if ((op == QLatin1Char('/') || op == QLatin1Char('%')) && rhs.scalar == 0.0) {
            ok = false;
            return makeScalar(0.0);
        }
        return makeScalar(scalarOp(lhs.scalar, rhs.scalar));
    }

    if (lhs.isVector && rhs.isVector) {
        if (lhs.values.size() != rhs.values.size()) {
            ok = false;
            return makeScalar(0.0);
        }
        std::vector<double> values = lhs.values;
        for (std::size_t index = 0; index < values.size(); ++index)
            values[index] = scalarOp(values[index], rhs.values[index]);
        return makeVector(std::move(values));
    }

    if (lhs.isVector) {
        if ((op == QLatin1Char('/') || op == QLatin1Char('%')) && rhs.scalar == 0.0) {
            ok = false;
            return makeScalar(0.0);
        }
        std::vector<double> values = lhs.values;
        for (double &value : values)
            value = scalarOp(value, rhs.scalar);
        return makeVector(std::move(values));
    }

    std::vector<double> values = rhs.values;
    for (double &value : values)
        value = scalarOp(lhs.scalar, value);
    return makeVector(std::move(values));
}

// ---------------------------------------------------------------------------
// Recursive-descent parser
// ---------------------------------------------------------------------------

class Parser
{
public:
    explicit Parser(const QVector<Token> &tokens)
        : m_tokens(tokens)
    {
    }

    std::optional<MathResult> parse()
    {
        const Value value = expression();
        if (!m_ok || peek().type != Token::End)
            return std::nullopt;

        if (value.isVector) {
            if (value.values.empty())
                return std::nullopt;
            for (const double item : value.values) {
                if (!std::isfinite(item))
                    return std::nullopt;
            }
            return MathResult::fromVector(value.values);
        }

        if (!std::isfinite(value.scalar))
            return std::nullopt;
        return MathResult::fromScalar(value.scalar);
    }

private:
    const Token &peek() const { return m_tokens.at(m_pos); }

    Token take()
    {
        const Token token = peek();
        if (m_pos < m_tokens.size() - 1)
            ++m_pos;
        return token;
    }

    bool acceptOperator(QChar op)
    {
        if (peek().type == Token::Operator && peek().text == QString(op)) {
            take();
            return true;
        }
        return false;
    }

    bool startsPrimary() const
    {
        const Token::Type type = peek().type;
        return type == Token::Number || type == Token::Identifier || type == Token::LParen
            || type == Token::LBracket;
    }

    Value expression()
    {
        Value value = term();
        while (m_ok) {
            if (acceptOperator(QLatin1Char('+'))) {
                value = applyBinary(QLatin1Char('+'), value, term(), m_ok);
            } else if (acceptOperator(QLatin1Char('-'))) {
                value = applyBinary(QLatin1Char('-'), value, term(), m_ok);
            } else {
                break;
            }
        }
        return value;
    }

    Value term()
    {
        Value value = unary();
        while (m_ok) {
            if (acceptOperator(QLatin1Char('*'))) {
                value = applyBinary(QLatin1Char('*'), value, unary(), m_ok);
            } else if (acceptOperator(QLatin1Char('/'))) {
                value = applyBinary(QLatin1Char('/'), value, unary(), m_ok);
            } else if (acceptOperator(QLatin1Char('%'))) {
                value = applyBinary(QLatin1Char('%'), value, unary(), m_ok);
            } else if (startsPrimary()) {
                // Implicit multiplication: `2pi`, `2 ln(e)`, `3(4+1)`.
                value = applyBinary(QLatin1Char('*'), value, unary(), m_ok);
            } else {
                break;
            }
        }
        return value;
    }

    Value unary()
    {
        if (acceptOperator(QLatin1Char('-')))
            return applyBinary(QLatin1Char('*'), makeScalar(-1.0), unary(), m_ok);
        if (acceptOperator(QLatin1Char('+')))
            return unary();
        return power();
    }

    Value power()
    {
        const Value base = primary();
        if (acceptOperator(QLatin1Char('^')))
            return applyBinary(QLatin1Char('^'), base, unary(), m_ok);
        return base;
    }

    Value primary()
    {
        const Token token = peek();
        switch (token.type) {
        case Token::Number:
            take();
            return makeScalar(token.number);
        case Token::LParen: {
            take();
            const Value value = expression();
            if (peek().type != Token::RParen) {
                m_ok = false;
                return makeScalar(0.0);
            }
            take();
            return value;
        }
        case Token::LBracket:
            take();
            return list();
        case Token::Identifier:
            take();
            return identifier(token.text);
        default:
            m_ok = false;
            return makeScalar(0.0);
        }
    }

    Value list()
    {
        std::vector<double> values;
        if (peek().type == Token::RBracket) {
            take();
            return makeVector(std::move(values));
        }

        while (m_ok) {
            const Value item = expression();
            if (item.isVector)
                values.insert(values.end(), item.values.begin(), item.values.end());
            else
                values.push_back(item.scalar);

            if (peek().type == Token::Comma) {
                take();
                continue;
            }
            break;
        }

        if (peek().type != Token::RBracket) {
            m_ok = false;
            return makeScalar(0.0);
        }
        take();
        return makeVector(std::move(values));
    }

    Value identifier(const QString &name)
    {
        if (name == QStringLiteral("pi"))
            return makeScalar(std::numbers::pi);
        if (name == QStringLiteral("e"))
            return makeScalar(std::numbers::e);

        if (!isFunctionName(name)) {
            m_ok = false;
            return makeScalar(0.0);
        }

        Value argument;
        if (peek().type == Token::LParen) {
            take();
            argument = expression();
            if (!m_ok)
                return makeScalar(0.0);
            if (peek().type != Token::RParen) {
                m_ok = false;
                return makeScalar(0.0);
            }
            take();
        } else {
            // Bare application: `sqrt 16`, `ln e`, `sigmoid ln(2)`.
            argument = unary();
        }

        if (!m_ok)
            return makeScalar(0.0);
        return applyUnary(name, argument, m_ok);
    }

    const QVector<Token> &m_tokens;
    int m_pos = 0;
    bool m_ok = true;
};

bool isAllowedWord(const QString &word)
{
    static const QSet<QString> allowed = {
        QStringLiteral("ln"),   QStringLiteral("log"),     QStringLiteral("sin"),
        QStringLiteral("cos"),  QStringLiteral("tan"),     QStringLiteral("sqrt"),
        QStringLiteral("abs"),  QStringLiteral("sigmoid"), QStringLiteral("sigma"),
        QStringLiteral("relu"), QStringLiteral("exp"),     QStringLiteral("softmax"),
        QStringLiteral("pi"),   QStringLiteral("e"),
    };
    return allowed.contains(word);
}

QString stripTriggerPrefix(QString text)
{
    static const QRegularExpression prefix(
        QStringLiteral(R"(^(?:calculate|what is|solve|evaluate)\s+)"));
    text = text.trimmed();
    const auto match = prefix.match(text);
    if (match.hasMatch())
        text = text.mid(match.capturedLength());
    return text.trimmed();
}

QString formatVectorComponent(double value)
{
    if (!std::isfinite(value))
        return QStringLiteral("0");

    // Truncate (not round) to 4 decimal places so values just below a
    // rounding boundary stay stable, then trim trailing zeros.
    const double truncated = std::trunc(value * 10000.0) / 10000.0;
    QString text = QString::number(truncated, 'f', 4);
    while (text.contains(QLatin1Char('.')) && text.endsWith(QLatin1Char('0')))
        text.chop(1);
    if (text.endsWith(QLatin1Char('.')))
        text.chop(1);
    if (text == QStringLiteral("-0"))
        text = QStringLiteral("0");
    return text;
}

} // namespace

MathResult MathResult::fromScalar(double value)
{
    MathResult result;
    result.kind = Kind::Scalar;
    result.scalar = value;
    return result;
}

MathResult MathResult::fromVector(std::vector<double> values)
{
    MathResult result;
    result.kind = Kind::Vector;
    result.values = std::move(values);
    return result;
}

std::optional<double> MathResult::scalarValue() const
{
    if (kind != Kind::Scalar)
        return std::nullopt;
    return scalar;
}

QString MathResult::format() const
{
    if (kind == Kind::Scalar)
        return MathEvaluator::format(scalar);

    QStringList parts;
    parts.reserve(static_cast<int>(values.size()));
    for (const double value : values)
        parts.append(formatVectorComponent(value));
    return QStringLiteral("[%1]").arg(parts.join(QStringLiteral(", ")));
}

bool MathEvaluator::looksLikeMath(const QString &expression)
{
    const QString text = expression.trimmed();
    if (text.isEmpty())
        return false;

    static const QRegularExpression wordPattern(QStringLiteral(R"([a-z]+)"));
    auto words = wordPattern.globalMatch(text);
    while (words.hasNext()) {
        if (!isAllowedWord(words.next().captured(0)))
            return false;
    }

    static const QRegularExpression junk(
        QStringLiteral(R"([^0-9a-z\s.()\[\],+\-*/^%])"));
    if (text.contains(junk))
        return false;

    // Require something evaluable: a digit, a constant or a function call.
    static const QRegularExpression evaluable(
        QStringLiteral(R"([0-9]|\bpi\b|\be\b|\b(?:ln|log|sin|cos|tan|sqrt|abs|sigmoid|sigma|relu|exp|softmax)\b)"));
    return text.contains(evaluable);
}

std::optional<MathResult> MathEvaluator::evaluate(const QString &expression)
{
    const QString text = stripTriggerPrefix(expression.trimmed());
    if (text.isEmpty())
        return std::nullopt;

    const QVector<Token> tokens = tokenize(text);
    Parser parser(tokens);
    return parser.parse();
}

std::optional<MathResult> MathEvaluator::evaluateUtterance(const QString &normalized)
{
    const QString text = stripTriggerPrefix(normalized);
    if (!looksLikeMath(text))
        return std::nullopt;
    return evaluate(text);
}

bool MathEvaluator::isMathTrigger(const QString &text)
{
    static const QRegularExpression trigger(
        QStringLiteral(R"(\b(?:calculate|solve|evaluate|what is|derivative|differentiat\w*|respect to|wrt|minus|plus|times|into|multiplied|divided|over|squared|cubed|power|sqrt|root|ln|log|sigmoid|softmax|relu|exp|vector|sigma|softness|modulo|percent)\b)"));
    return trigger.match(text.toLower()).hasMatch();
}

bool MathEvaluator::hasMathContent(const QString &text)
{
    static const QRegularExpression content(
        QStringLiteral(R"([0-9]|\b(?:pi|e|ln|log|sin|cos|tan|sqrt|abs|sigmoid|sigma|relu|exp|softmax|vector|squared|cubed|half|quarter|zero|one|two|three|four|five|six|seven|eight|nine|ten)\b)"));
    return content.match(text.toLower()).hasMatch();
}

QString MathEvaluator::format(double value)
{
    if (!std::isfinite(value))
        return {};

    if (std::abs(value) < 1e-12)
        value = 0.0;

    const double rounded = std::round(value);
    if (std::abs(value - rounded) <= 1e-10 * std::max(1.0, std::abs(value)))
        value = rounded;

    QString text = QString::number(value, 'g', 6);
    if (text == QStringLiteral("-0"))
        text = QStringLiteral("0");
    return text;
}

QString MathEvaluator::format(const MathResult &result)
{
    return result.format();
}

} // namespace dex
