#pragma once

#include <QString>

#include <optional>

namespace dex {

/// Deterministic symbolic differentiator for algebraic expressions.
///
/// Supports constants, variables (including identifiers such as `y_hat`),
/// `+ - * / ^`, parentheses, and the functions `ln`, `exp`, `sin`, `cos`,
/// `sigmoid` and `sqrt`. The derivative is built from the sum, product,
/// quotient, power and chain rules and then simplified (`0 + x -> x`,
/// `1 * x -> x`, `0 * x -> 0`, `x ^ 1 -> x`, `x ^ 0 -> 1`, constant folding)
/// so `d/dy_hat (0.5 * (y_hat - y) ^ 2)` becomes `y_hat - y`.
class SymbolicDifferentiator
{
public:
    /// Returns the simplified derivative, or nullopt when `expression` cannot
    /// be parsed. Occurrences of variables other than `variable` are treated
    /// as constants.
    static std::optional<QString> differentiate(const QString &expression,
                                                const QString &variable);

    /// Parses and simplifies an expression without differentiating it.
    static std::optional<QString> simplify(const QString &expression);

private:
    SymbolicDifferentiator() = default;
};

} // namespace dex
