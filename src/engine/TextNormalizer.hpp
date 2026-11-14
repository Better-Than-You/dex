#pragma once

#include <QString>

namespace dex {

/// Deterministic spoken-text canonicalisation.
///
/// Order of operations (see docs/SPEC_ENGINE.md):
///   1. trim + lowercase
///   2. spoken-math substitutions (before dot stripping, so `l.n.e` survives)
///   3. trailing punctuation removal (decimal dots are preserved)
///   4. wake-prefix removal (`hey dex`, `dex`, `please`, `can you`)
class TextNormalizer
{
public:
    static QString normalize(const QString &raw);

private:
    TextNormalizer() = default;
};

} // namespace dex
