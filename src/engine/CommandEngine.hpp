#pragma once

#include "engine/RuleRouter.hpp"

#include <QString>

namespace dex {

class LlamaMathTranslator;

/// Deterministic pipeline with an optional maths/calculus SLM front-end.
///
/// Order (see docs/SPEC_ENGINE.md):
///   1. `commands.toml` rules run first: desktop commands never pay for the
///      SLM and dispatch on the deterministic fast path.
///   2. Maths/calculus utterances may be normalised by `LlamaMathTranslator`
///      (when enabled and the GGUF exists) into `{"mode":"eval|diff",...}`.
///   3. If the SLM is unavailable or fails, the deterministic
///      `TextNormalizer` + `MathEvaluator` / `SymbolicDifferentiator`
///      pipeline handles the utterance.
///
/// `process()` is side-effect free; the caller (AppController) decides whether
/// to copy the maths result, dispatch a command or ask for confirmation.
class CommandEngine
{
public:
    enum class OutcomeKind { None, Math, Action, Confirmation };

    struct Outcome {
        OutcomeKind kind = OutcomeKind::None;
        QString normalized;
        QString expression;
        QString result;
        RouteResult route;

        /// Diagnostics for the live test and logging.
        bool usedSlm = false;
        QString slmMode;
        QString slmExpression;
    };

    CommandEngine(RuleRouter &router, bool confirmByDefault);

    void setConfirmByDefault(bool value) { m_confirmByDefault = value; }

    /// Optional maths SLM. Not owned; may be nullptr (pure deterministic).
    void setMathTranslator(LlamaMathTranslator *translator) { m_translator = translator; }

    Outcome process(const QString &rawText) const;

    /// Strips `calculate`, `what is`, `solve` for display purposes.
    static QString displayExpression(const QString &normalized);

private:
    RuleRouter &m_router;
    LlamaMathTranslator *m_translator = nullptr;
    bool m_confirmByDefault = false;
};

} // namespace dex
