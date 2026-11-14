#include "engine/MathEvaluator.hpp"

#include <QRegularExpression>
#include <QSet>
#include <QVector>

#include <cmath>
#include <numbers>

namespace dex {
namespace {

struct Token {
    enum Type { Number, Identifier, Operator, LParen, RParen, End };

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

        if (c == QLatin1Char('(')) {
            tokens.push_back({Token::LParen, QStringLiteral("("), 0.0});
            ++i;
            continue;
        }
        if (c == QLatin1Char(')')) {
            tokens.push_back({Token::RParen, QStringLiteral(")"), 0.0});
            ++i;
            continue;
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

bool isFunction(const QString &name)
{
    static const QSet<QString> functions = {
        QStringLiteral("ln"), QStringLiteral("log"), QStringLiteral("sin"),
        QStringLiteral("cos"), QStringLiteral("tan"), QStringLiteral("sqrt"),
        QStringLiteral("abs"),
    };
    return functions.contains(name);
}

class Parser
{
public:
    explicit Parser(const QVector<Token> &tokens)
        : m_tokens(tokens)
    {
    }

    std::optional<double> parse()
    {
        const double value = expression();
        if (!m_ok || peek().type != Token::End || !std::isfinite(value))
            return std::nullopt;
        return value;
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
        return type == Token::Number || type == Token::Identifier || type == Token::LParen;
    }

    double expression()
    {
        double value = term();
        while (m_ok) {
            if (acceptOperator(QLatin1Char('+'))) {
                value += term();
            } else if (acceptOperator(QLatin1Char('-'))) {
                value -= term();
            } else {
                break;
            }
        }
        return value;
    }

    double term()
    {
        double value = unary();
        while (m_ok) {
            if (acceptOperator(QLatin1Char('*'))) {
                value *= unary();
            } else if (acceptOperator(QLatin1Char('/'))) {
                const double divisor = unary();
                if (divisor == 0.0) {
                    m_ok = false;
                    return 0.0;
                }
                value /= divisor;
            } else if (acceptOperator(QLatin1Char('%'))) {
                const double divisor = unary();
                if (divisor == 0.0) {
                    m_ok = false;
                    return 0.0;
                }
                value = std::fmod(value, divisor);
            } else if (startsPrimary()) {
                // Implicit multiplication: `2pi`, `2 ln(e)`, `3(4+1)`.
                value *= unary();
            } else {
                break;
            }
        }
        return value;
    }

    double unary()
    {
        if (acceptOperator(QLatin1Char('-')))
            return -unary();
        if (acceptOperator(QLatin1Char('+')))
            return unary();
        return power();
    }

    double power()
    {
        const double base = primary();
        if (acceptOperator(QLatin1Char('^')))
            return std::pow(base, unary());
        return base;
    }

    double primary()
    {
        const Token token = peek();
        switch (token.type) {
        case Token::Number:
            take();
            return token.number;
        case Token::LParen: {
            take();
            const double value = expression();
            if (peek().type != Token::RParen) {
                m_ok = false;
                return 0.0;
            }
            take();
            return value;
        }
        case Token::Identifier:
            take();
            return identifier(token.text);
        default:
            m_ok = false;
            return 0.0;
        }
    }

    double identifier(const QString &name)
    {
        if (name == QStringLiteral("pi"))
            return std::numbers::pi;
        if (name == QStringLiteral("e"))
            return std::numbers::e;

        if (!isFunction(name)) {
            m_ok = false;
            return 0.0;
        }

        double argument = 0.0;
        if (peek().type == Token::LParen) {
            take();
            argument = expression();
            if (peek().type != Token::RParen) {
                m_ok = false;
                return 0.0;
            }
            take();
        } else {
            // Bare application: `sqrt 16`, `ln e`.
            argument = unary();
        }

        if (!m_ok)
            return 0.0;

        if (name == QStringLiteral("ln"))
            return std::log(argument);
        if (name == QStringLiteral("log"))
            return std::log10(argument);
        if (name == QStringLiteral("sin"))
            return std::sin(argument);
        if (name == QStringLiteral("cos"))
            return std::cos(argument);
        if (name == QStringLiteral("tan"))
            return std::tan(argument);
        if (name == QStringLiteral("sqrt"))
            return std::sqrt(argument);
        if (name == QStringLiteral("abs"))
            return std::abs(argument);

        m_ok = false;
        return 0.0;
    }

    const QVector<Token> &m_tokens;
    int m_pos = 0;
    bool m_ok = true;
};

bool isAllowedWord(const QString &word)
{
    static const QSet<QString> allowed = {
        QStringLiteral("ln"), QStringLiteral("log"), QStringLiteral("sin"),
        QStringLiteral("cos"), QStringLiteral("tan"), QStringLiteral("sqrt"),
        QStringLiteral("abs"), QStringLiteral("pi"), QStringLiteral("e"),
    };
    return allowed.contains(word);
}

QString stripTriggerPrefix(QString text)
{
    static const QRegularExpression prefix(
        QStringLiteral(R"(^(?:calculate|what is|solve)\s+)"));
    text = text.trimmed();
    const auto match = prefix.match(text);
    if (match.hasMatch())
        text = text.mid(match.capturedLength());
    return text.trimmed();
}

} // namespace

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
        QStringLiteral(R"([^0-9a-z\s.()+\-*/^%])"));
    if (text.contains(junk))
        return false;

    // Require something evaluable: a digit, a constant or a function call.
    static const QRegularExpression evaluable(
        QStringLiteral(R"([0-9]|\bpi\b|\be\b|\b(?:ln|log|sin|cos|tan|sqrt|abs)\b)"));
    return text.contains(evaluable);
}

std::optional<double> MathEvaluator::evaluate(const QString &expression)
{
    const QString text = stripTriggerPrefix(expression.trimmed());
    if (text.isEmpty())
        return std::nullopt;

    const QVector<Token> tokens = tokenize(text);
    Parser parser(tokens);
    return parser.parse();
}

std::optional<double> MathEvaluator::evaluateUtterance(const QString &normalized)
{
    const QString text = stripTriggerPrefix(normalized);
    if (!looksLikeMath(text))
        return std::nullopt;
    return evaluate(text);
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

    QString text = QString::number(value, 'g', 12);
    if (text == QStringLiteral("-0"))
        text = QStringLiteral("0");
    return text;
}

} // namespace dex
