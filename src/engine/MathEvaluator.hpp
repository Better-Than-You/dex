#pragma once

#include <QString>

#include <optional>

namespace dex {

/// Deterministic recursive-descent arithmetic evaluator.
///
/// Supports `+ - * / ^ %`, parentheses, unary minus, implicit multiplication
/// (`2pi`, `-2ln(e)`), and the functions `ln`, `log`, `sin`, `cos`, `tan`,
/// `sqrt` plus the constants `pi` and `e`.
class MathEvaluator
{
public:
    /// Evaluates a bare mathematical expression.
    /// Returns std::nullopt on syntax errors, division/modulo by zero and
    /// non-finite results.
    static std::optional<double> evaluate(const QString &expression);

    /// Applies the trigger rules from docs/SPEC_ENGINE.md to an already
    /// normalized utterance. Returns std::nullopt when the utterance is not
    /// a maths expression.
    static std::optional<double> evaluateUtterance(const QString &normalized);

    /// True when every token is a number, operator, known function/constant.
    static bool looksLikeMath(const QString &expression);

    /// Formats a result for the clipboard / UI, e.g. `12`, `-2`, `0.5`.
    static QString format(double value);

private:
    MathEvaluator() = default;
};

} // namespace dex
