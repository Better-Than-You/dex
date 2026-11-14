#include "engine/SymbolicDifferentiator.hpp"

#include <QVector>

#include <cmath>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace dex {
namespace {

struct Node;
using NodePtr = std::shared_ptr<Node>;

struct Node {
    enum class Type {
        Const,
        Var,
        Add,
        Sub,
        Mul,
        Div,
        Pow,
        Ln,
        Exp,
        Sin,
        Cos,
        Sigmoid,
    };

    Type type = Type::Const;
    double value = 0.0;
    QString name;
    NodePtr left;
    NodePtr right;
};

NodePtr makeNode(Node::Type type, NodePtr left = nullptr, NodePtr right = nullptr)
{
    auto node = std::make_shared<Node>();
    node->type = type;
    node->left = std::move(left);
    node->right = std::move(right);
    return node;
}

NodePtr makeConst(double value)
{
    auto node = std::make_shared<Node>();
    node->type = Node::Type::Const;
    node->value = value;
    return node;
}

NodePtr makeVar(const QString &name)
{
    auto node = std::make_shared<Node>();
    node->type = Node::Type::Var;
    node->name = name;
    return node;
}

NodePtr makeUnary(Node::Type type, NodePtr argument)
{
    return makeNode(type, std::move(argument));
}

bool isConst(const NodePtr &node, double *value = nullptr)
{
    if (!node || node->type != Node::Type::Const)
        return false;
    if (value)
        *value = node->value;
    return true;
}

bool equals(const NodePtr &lhs, const NodePtr &rhs)
{
    if (!lhs || !rhs)
        return lhs == rhs;
    if (lhs->type != rhs->type)
        return false;

    switch (lhs->type) {
    case Node::Type::Const:
        return lhs->value == rhs->value;
    case Node::Type::Var:
        return lhs->name == rhs->name;
    default:
        return equals(lhs->left, rhs->left) && equals(lhs->right, rhs->right);
    }
}

// ---------------------------------------------------------------------------
// Algebraic simplifier
// ---------------------------------------------------------------------------

NodePtr simplifyNode(const NodePtr &node);

void collect(Node::Type type, const NodePtr &node, std::vector<NodePtr> &out)
{
    if (node && node->type == type) {
        collect(type, node->left, out);
        collect(type, node->right, out);
        return;
    }
    out.push_back(simplifyNode(node));
}

NodePtr buildChain(Node::Type type, std::vector<NodePtr> &items)
{
    NodePtr result = items.front();
    for (std::size_t index = 1; index < items.size(); ++index)
        result = makeNode(type, result, items[index]);
    return result;
}

NodePtr makeSum(const NodePtr &lhs, const NodePtr &rhs)
{
    std::vector<NodePtr> terms;
    collect(Node::Type::Add, lhs, terms);
    collect(Node::Type::Add, rhs, terms);

    double sum = 0.0;
    std::vector<NodePtr> rest;
    for (const NodePtr &term : terms) {
        double value = 0.0;
        if (isConst(term, &value)) {
            sum += value;
            continue;
        }
        rest.push_back(term);
    }

    if (rest.empty())
        return makeConst(sum);
    if (sum != 0.0)
        rest.insert(rest.begin(), makeConst(sum));
    if (rest.size() == 1)
        return rest.front();
    return buildChain(Node::Type::Add, rest);
}

NodePtr makeProduct(const NodePtr &lhs, const NodePtr &rhs)
{
    std::vector<NodePtr> factors;
    collect(Node::Type::Mul, lhs, factors);
    collect(Node::Type::Mul, rhs, factors);

    double product = 1.0;
    std::vector<NodePtr> rest;
    for (const NodePtr &factor : factors) {
        double value = 0.0;
        if (isConst(factor, &value)) {
            product *= value;
            continue;
        }
        rest.push_back(factor);
    }

    if (product == 0.0)
        return makeConst(0.0);
    if (rest.empty())
        return makeConst(product);
    if (product != 1.0)
        rest.insert(rest.begin(), makeConst(product));
    if (rest.size() == 1)
        return rest.front();
    return buildChain(Node::Type::Mul, rest);
}

NodePtr makeDifference(const NodePtr &lhs, const NodePtr &rhs)
{
    const NodePtr a = simplifyNode(lhs);
    const NodePtr b = simplifyNode(rhs);

    if (equals(a, b))
        return makeConst(0.0);
    if (isConst(b, nullptr) && b->value == 0.0)
        return a;
    if (isConst(a, nullptr) && a->value == 0.0)
        return makeProduct(makeConst(-1.0), b);

    double av = 0.0;
    double bv = 0.0;
    if (isConst(a, &av) && isConst(b, &bv))
        return makeConst(av - bv);
    return makeNode(Node::Type::Sub, a, b);
}

NodePtr makeQuotient(const NodePtr &lhs, const NodePtr &rhs)
{
    const NodePtr a = simplifyNode(lhs);
    const NodePtr b = simplifyNode(rhs);

    if (isConst(a, nullptr) && a->value == 0.0 && !(isConst(b, nullptr) && b->value == 0.0))
        return makeConst(0.0);
    if (isConst(b, nullptr) && b->value == 1.0)
        return a;
    if (isConst(a, nullptr) && isConst(b, nullptr) && b->value != 0.0)
        return makeConst(a->value / b->value);
    return makeNode(Node::Type::Div, a, b);
}

NodePtr makePower(const NodePtr &base, const NodePtr &exponent)
{
    const NodePtr a = simplifyNode(base);
    const NodePtr b = simplifyNode(exponent);

    double exponentValue = 0.0;
    if (isConst(b, &exponentValue)) {
        if (exponentValue == 0.0)
            return makeConst(1.0);
        if (exponentValue == 1.0)
            return a;

        double baseValue = 0.0;
        if (isConst(a, &baseValue))
            return makeConst(std::pow(baseValue, exponentValue));
    }

    if (isConst(a, nullptr) && a->value == 1.0)
        return makeConst(1.0);
    return makeNode(Node::Type::Pow, a, b);
}

NodePtr simplifyNode(const NodePtr &node)
{
    if (!node)
        return nullptr;

    switch (node->type) {
    case Node::Type::Const:
    case Node::Type::Var:
        return node;
    case Node::Type::Add:
        return makeSum(node->left, node->right);
    case Node::Type::Sub:
        return makeDifference(node->left, node->right);
    case Node::Type::Mul:
        return makeProduct(node->left, node->right);
    case Node::Type::Div:
        return makeQuotient(node->left, node->right);
    case Node::Type::Pow:
        return makePower(node->left, node->right);
    case Node::Type::Ln:
    case Node::Type::Exp:
    case Node::Type::Sin:
    case Node::Type::Cos:
    case Node::Type::Sigmoid:
        return makeUnary(node->type, simplifyNode(node->left));
    }
    return node;
}

// ---------------------------------------------------------------------------
// Differentiation
// ---------------------------------------------------------------------------

NodePtr derive(const NodePtr &node, const QString &variable)
{
    using Type = Node::Type;

    switch (node->type) {
    case Type::Const:
        return makeConst(0.0);
    case Type::Var:
        return makeConst(node->name == variable ? 1.0 : 0.0);
    case Type::Add:
        return makeSum(derive(node->left, variable), derive(node->right, variable));
    case Type::Sub:
        return makeDifference(derive(node->left, variable), derive(node->right, variable));
    case Type::Mul:
        return makeSum(makeProduct(derive(node->left, variable), node->right),
                       makeProduct(node->left, derive(node->right, variable)));
    case Type::Div:
        return makeQuotient(
            makeDifference(makeProduct(derive(node->left, variable), node->right),
                           makeProduct(node->left, derive(node->right, variable))),
            makePower(node->right, makeConst(2.0)));
    case Type::Pow: {
        double exponent = 0.0;
        if (isConst(node->right, &exponent)) {
            // Power rule with the chain rule: n * u^(n-1) * u'.
            return makeProduct(makeProduct(makeConst(exponent),
                                           makePower(node->left, makeConst(exponent - 1.0))),
                               derive(node->left, variable));
        }
        // General case: (u^v)' = u^v * (v' ln(u) + v u' / u).
        return makeProduct(
            makePower(node->left, node->right),
            makeSum(makeProduct(derive(node->right, variable),
                                makeUnary(Type::Ln, node->left)),
                    makeQuotient(makeProduct(node->right, derive(node->left, variable)),
                                 node->left)));
    }
    case Type::Ln:
        return makeQuotient(derive(node->left, variable), node->left);
    case Type::Exp:
        return makeProduct(makeUnary(Type::Exp, node->left), derive(node->left, variable));
    case Type::Sin:
        return makeProduct(makeUnary(Type::Cos, node->left), derive(node->left, variable));
    case Type::Cos:
        return makeProduct(makeConst(-1.0),
                           makeProduct(makeUnary(Type::Sin, node->left),
                                       derive(node->left, variable)));
    case Type::Sigmoid:
        return makeProduct(
            makeProduct(makeUnary(Type::Sigmoid, node->left),
                        makeDifference(makeConst(1.0), makeUnary(Type::Sigmoid, node->left))),
            derive(node->left, variable));
    }
    return makeConst(0.0);
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

int precedence(const NodePtr &node)
{
    switch (node->type) {
    case Node::Type::Add:
    case Node::Type::Sub:
        return 1;
    case Node::Type::Mul:
    case Node::Type::Div:
        return 2;
    case Node::Type::Pow:
        return 4;
    default:
        return 5;
    }
}

QString formatNumber(double value)
{
    if (!std::isfinite(value))
        return QStringLiteral("0");
    QString text = QString::number(value, 'g', 12);
    if (text == QStringLiteral("-0"))
        text = QStringLiteral("0");
    return text;
}

QString render(const NodePtr &node, int parentPrecedence = 0)
{
    if (!node)
        return {};

    QString text;
    int ownPrecedence = precedence(node);

    switch (node->type) {
    case Node::Type::Const:
        text = formatNumber(node->value);
        break;
    case Node::Type::Var:
        text = node->name;
        break;
    case Node::Type::Add:
        text = QStringLiteral("%1 + %2").arg(render(node->left, 2), render(node->right, 2));
        break;
    case Node::Type::Sub:
        text = QStringLiteral("%1 - %2").arg(render(node->left, 2), render(node->right, 3));
        break;
    case Node::Type::Mul:
        text = QStringLiteral("%1 * %2").arg(render(node->left, 2), render(node->right, 3));
        break;
    case Node::Type::Div:
        text = QStringLiteral("%1 / %2").arg(render(node->left, 2), render(node->right, 3));
        break;
    case Node::Type::Pow:
        text = QStringLiteral("%1 ^ %2").arg(render(node->left, 5), render(node->right, 5));
        break;
    case Node::Type::Ln:
        text = QStringLiteral("ln(%1)").arg(render(node->left));
        break;
    case Node::Type::Exp:
        text = QStringLiteral("exp(%1)").arg(render(node->left));
        break;
    case Node::Type::Sin:
        text = QStringLiteral("sin(%1)").arg(render(node->left));
        break;
    case Node::Type::Cos:
        text = QStringLiteral("cos(%1)").arg(render(node->left));
        break;
    case Node::Type::Sigmoid:
        text = QStringLiteral("sigmoid(%1)").arg(render(node->left));
        break;
    }

    if (ownPrecedence < parentPrecedence)
        return QStringLiteral("(%1)").arg(text);
    return text;
}

// ---------------------------------------------------------------------------
// Parsing
// ---------------------------------------------------------------------------

struct Token {
    enum Type { Number, Identifier, Operator, LParen, RParen, End };

    Type type = End;
    QString text;
    double number = 0.0;
};

QVector<Token> tokenize(const QString &input)
{
    QVector<Token> tokens;
    int index = 0;
    const int size = input.size();

    while (index < size) {
        const QChar c = input.at(index);
        if (c.isSpace()) {
            ++index;
            continue;
        }

        if (c.isDigit() || (c == QLatin1Char('.') && index + 1 < size
                            && input.at(index + 1).isDigit())) {
            const int start = index;
            bool seenDot = false;
            while (index < size) {
                const QChar digit = input.at(index);
                if (digit.isDigit()) {
                    ++index;
                } else if (digit == QLatin1Char('.') && !seenDot) {
                    seenDot = true;
                    ++index;
                } else {
                    break;
                }
            }
            Token token;
            token.type = Token::Number;
            token.text = input.mid(start, index - start);
            token.number = token.text.toDouble();
            tokens.push_back(token);
            continue;
        }

        if (c.isLetter()) {
            const int start = index;
            while (index < size) {
                const QChar letter = input.at(index);
                if (letter.isLetterOrNumber() || letter == QLatin1Char('_'))
                    ++index;
                else
                    break;
            }
            Token token;
            token.type = Token::Identifier;
            token.text = input.mid(start, index - start).toLower();
            tokens.push_back(token);
            continue;
        }

        if (c == QLatin1Char('(')) {
            tokens.push_back({Token::LParen, QStringLiteral("("), 0.0});
            ++index;
            continue;
        }
        if (c == QLatin1Char(')')) {
            tokens.push_back({Token::RParen, QStringLiteral(")"), 0.0});
            ++index;
            continue;
        }
        if (QStringLiteral("+-*/^").contains(c)) {
            tokens.push_back({Token::Operator, QString(c), 0.0});
            ++index;
            continue;
        }

        tokens.push_back({Token::Operator, QStringLiteral("?"), 0.0});
        ++index;
    }

    tokens.push_back({Token::End, QString(), 0.0});
    return tokens;
}

class Parser
{
public:
    explicit Parser(const QVector<Token> &tokens)
        : m_tokens(tokens)
    {
    }

    NodePtr parse()
    {
        NodePtr node = expression();
        if (!m_ok || !node || peek().type != Token::End)
            return nullptr;
        return node;
    }

private:
    const Token &peek() const { return m_tokens.at(m_position); }

    Token take()
    {
        const Token token = peek();
        if (m_position < m_tokens.size() - 1)
            ++m_position;
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

    NodePtr expression()
    {
        NodePtr node = term();
        while (m_ok) {
            if (acceptOperator(QLatin1Char('+')))
                node = makeNode(Node::Type::Add, node, term());
            else if (acceptOperator(QLatin1Char('-')))
                node = makeNode(Node::Type::Sub, node, term());
            else
                break;
        }
        return node;
    }

    NodePtr term()
    {
        NodePtr node = unary();
        while (m_ok) {
            if (acceptOperator(QLatin1Char('*'))) {
                node = makeNode(Node::Type::Mul, node, unary());
            } else if (acceptOperator(QLatin1Char('/'))) {
                node = makeNode(Node::Type::Div, node, unary());
            } else if (startsPrimary()) {
                node = makeNode(Node::Type::Mul, node, unary());
            } else {
                break;
            }
        }
        return node;
    }

    NodePtr unary()
    {
        if (acceptOperator(QLatin1Char('-')))
            return makeNode(Node::Type::Mul, makeConst(-1.0), unary());
        if (acceptOperator(QLatin1Char('+')))
            return unary();
        return power();
    }

    NodePtr power()
    {
        NodePtr base = primary();
        if (acceptOperator(QLatin1Char('^')))
            return makeNode(Node::Type::Pow, base, unary());
        return base;
    }

    NodePtr primary()
    {
        const Token token = peek();
        switch (token.type) {
        case Token::Number:
            take();
            return makeConst(token.number);
        case Token::LParen: {
            take();
            NodePtr node = expression();
            if (peek().type != Token::RParen) {
                m_ok = false;
                return nullptr;
            }
            take();
            return node;
        }
        case Token::Identifier:
            take();
            return identifier(token.text);
        default:
            m_ok = false;
            return nullptr;
        }
    }

    NodePtr identifier(const QString &name)
    {
        if (name == QStringLiteral("pi"))
            return makeConst(3.141592653589793);
        if (name == QStringLiteral("e"))
            return makeConst(2.718281828459045);

        const auto unaryType = [](const QString &function) -> std::optional<Node::Type> {
            if (function == QStringLiteral("ln") || function == QStringLiteral("log"))
                return Node::Type::Ln;
            if (function == QStringLiteral("exp"))
                return Node::Type::Exp;
            if (function == QStringLiteral("sin"))
                return Node::Type::Sin;
            if (function == QStringLiteral("cos"))
                return Node::Type::Cos;
            if (function == QStringLiteral("sigmoid") || function == QStringLiteral("sigma"))
                return Node::Type::Sigmoid;
            return std::nullopt;
        };

        if (name == QStringLiteral("sqrt")) {
            NodePtr argument = argumentFor(name);
            if (!argument)
                return nullptr;
            return makeNode(Node::Type::Pow, argument, makeConst(0.5));
        }

        if (const auto type = unaryType(name)) {
            NodePtr argument = argumentFor(name);
            if (!argument)
                return nullptr;
            return makeUnary(*type, argument);
        }

        return makeVar(name);
    }

    NodePtr argumentFor(const QString &)
    {
        if (peek().type == Token::LParen) {
            take();
            NodePtr node = expression();
            if (peek().type != Token::RParen) {
                m_ok = false;
                return nullptr;
            }
            take();
            return node;
        }
        return unary();
    }

    const QVector<Token> &m_tokens;
    int m_position = 0;
    bool m_ok = true;
};

} // namespace

std::optional<QString> SymbolicDifferentiator::differentiate(const QString &expression,
                                                             const QString &variable)
{
    const QVector<Token> tokens = tokenize(expression);
    Parser parser(tokens);
    const NodePtr root = parser.parse();
    if (!root)
        return std::nullopt;

    const NodePtr derivative = simplifyNode(derive(root, variable.trimmed().toLower()));
    return render(derivative);
}

std::optional<QString> SymbolicDifferentiator::simplify(const QString &expression)
{
    const QVector<Token> tokens = tokenize(expression);
    Parser parser(tokens);
    const NodePtr root = parser.parse();
    if (!root)
        return std::nullopt;
    return render(simplifyNode(root));
}

} // namespace dex
