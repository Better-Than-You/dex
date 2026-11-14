#pragma once

#include <QString>

#include <optional>

namespace dex {

/// A spoken derivative request: `d/d<variable> <expression>`.
struct DerivativeRequest {
    QString expression;
    QString variable;
};

/// Deterministic spoken-text canonicalisation.
///
/// Order of operations (see docs/SPEC_ENGINE.md):
///   1. trim + lowercase, drop filler words (`uh`, `um`)
///   2. spoken-math substitutions (before dot stripping, so `l.n.e` survives)
///   3. number-word mapping in maths context (`half` -> `0.5`)
///   4. trailing punctuation removal (decimal dots are preserved)
///   5. wake-prefix removal (`hey dex`, `dex`, `please`, `can you`)
class TextNormalizer
{
public:
    static QString normalize(const QString &raw);

    /// Extracts a derivative request ("... in respect to y hat") from a noisy
    /// transcript. Returns nullopt when no derivative marker is present.
    static std::optional<DerivativeRequest> derivative(const QString &raw);

private:
    TextNormalizer() = default;
};

} // namespace dex
