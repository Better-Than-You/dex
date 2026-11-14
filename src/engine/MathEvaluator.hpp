#pragma once

#include <QString>

#include <optional>
#include <vector>

namespace dex {

/// Result of a deterministic evaluation: either a scalar or a vector
/// (`softmax([...])`) value.
struct MathResult {
    enum class Kind { Scalar, Vector };

    Kind kind = Kind::Scalar;
    double scalar = 0.0;
    std::vector<double> values;

    static MathResult fromScalar(double value);
    static MathResult fromVector(std::vector<double> values);

    bool isVector() const { return kind == Kind::Vector; }

    /// Scalar value when the result is a scalar.
    std::optional<double> scalarValue() const;

    /// Clipboard / UI representation. Scalars use up to 6 significant
    /// digits; vectors use 4 decimal places with trailing zeros trimmed,
    /// e.g. `[0.7018, 0.2853, 0.0128]`.
    QString format() const;
};

/// Deterministic recursive-descent arithmetic evaluator.
///
/// Supports `+ - * / ^ %`, parentheses, unary minus, implicit multiplication
/// (`2pi`, `-2ln(e)`), scalar functions `ln`, `log`, `sin`, `cos`, `tan`,
/// `sqrt`, `abs`, the ML helpers `sigmoid`, `relu`, `exp`, and the vector
/// helper `softmax([...])`, plus the constants `pi` and `e`.
class MathEvaluator
{
public:
    /// Evaluates a bare mathematical expression.
    /// Returns std::nullopt on syntax errors, division/modulo by zero and
    /// non-finite results.
    static std::optional<MathResult> evaluate(const QString &expression);

    /// Applies the trigger rules from docs/SPEC_ENGINE.md to an already
    /// normalized utterance. Returns std::nullopt when the utterance is not
    /// a maths expression.
    static std::optional<MathResult> evaluateUtterance(const QString &normalized);

    /// True when every token is a number, operator, known function/constant.
    static bool looksLikeMath(const QString &expression);

    /// True when an utterance should be offered to the maths/calculus path
    /// (used to decide whether the SLM translator is worth invoking).
    static bool isMathTrigger(const QString &text);

    /// True when the text carries something evaluable: a digit, a known
    /// function/constant, a vector keyword or a number word. Combined with
    /// `isMathTrigger` so phrases such as "turn the power off" do not wake the
    /// SLM.
    static bool hasMathContent(const QString &text);

    /// Formats a scalar for the clipboard / UI, e.g. `12`, `-2`, `0.5`.
    static QString format(double value);

    static QString format(const MathResult &result);

private:
    MathEvaluator() = default;
};

} // namespace dex
