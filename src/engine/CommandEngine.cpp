#include "engine/CommandEngine.hpp"

#include "engine/LlamaMathTranslator.hpp"
#include "engine/MathEvaluator.hpp"
#include "engine/SymbolicDifferentiator.hpp"
#include "engine/TextNormalizer.hpp"

#include <QDebug>
#include <QRegularExpression>

namespace dex {
namespace {

QString derivativeDisplay(const QString &variable, const QString &expression)
{
    return QStringLiteral("d/d%1 (%2)").arg(variable, expression);
}

} // namespace

CommandEngine::CommandEngine(RuleRouter &router, bool confirmByDefault)
    : m_router(router)
    , m_confirmByDefault(confirmByDefault)
{
}

QString CommandEngine::displayExpression(const QString &normalized)
{
    static const QRegularExpression prefix(
        QStringLiteral(R"(^(?:calculate|what is|solve|evaluate)\s+)"));
    const QString text = normalized.trimmed();
    const auto match = prefix.match(text);
    if (match.hasMatch())
        return text.mid(match.capturedLength()).trimmed();
    return text;
}

CommandEngine::Outcome CommandEngine::process(const QString &rawText) const
{
    Outcome outcome;
    outcome.normalized = TextNormalizer::normalize(rawText);
    if (outcome.normalized.isEmpty())
        return outcome;

    // --- 1. Deterministic desktop fast path (0 ms, no model) ----------------
    if (const auto route = m_router.route(outcome.normalized, m_confirmByDefault)) {
        outcome.route = *route;
        outcome.kind = route->confirm ? OutcomeKind::Confirmation : OutcomeKind::Action;
        return outcome;
    }

    const auto derivativeRequest = TextNormalizer::derivative(rawText);
    const bool wantsMath = derivativeRequest.has_value()
        || (MathEvaluator::isMathTrigger(rawText) && MathEvaluator::hasMathContent(rawText));
    if (!wantsMath)
        return outcome;

    // --- 2. Optional SLM translation --------------------------------------
    if (m_translator && m_translator->available()) {
        QString error;
        if (const auto command = m_translator->translate(rawText, &error)) {
            // A derivative marker ("... in respect to x") is detected
            // deterministically before the SLM runs. Small models sometimes
            // tag such an utterance as "eval" because most few-shot examples
            // are evaluations, so the marker wins and supplies the variable.
            const bool slmEval = command->mode == QStringLiteral("eval");
            const bool slmDiff = command->mode == QStringLiteral("diff");
            const bool asDerivative = derivativeRequest.has_value() || slmDiff;
            const QString mode = asDerivative ? QStringLiteral("diff") : command->mode;
            const QString variable =
                derivativeRequest ? derivativeRequest->variable : command->var;

            outcome.slmMode = mode;
            outcome.slmExpression = command->expr;

            if (slmEval && !asDerivative) {
                if (const auto value = MathEvaluator::evaluate(command->expr)) {
                    outcome.kind = OutcomeKind::Math;
                    outcome.usedSlm = true;
                    outcome.expression = command->expr;
                    outcome.result = value->format();
                    return outcome;
                }
            } else if (asDerivative) {
                if (const auto derivative =
                        SymbolicDifferentiator::differentiate(command->expr, variable)) {
                    outcome.kind = OutcomeKind::Math;
                    outcome.usedSlm = true;
                    outcome.expression = derivativeDisplay(variable, command->expr);
                    outcome.result = *derivative;
                    return outcome;
                }
            }

            qWarning().noquote()
                << QStringLiteral("Maths SLM returned an unusable command (%1: %2); "
                                  "falling back to the deterministic pipeline.")
                       .arg(command->mode, command->expr);
        } else {
            qWarning().noquote()
                << QStringLiteral("Maths SLM unavailable: %1").arg(error);
        }
    }

    // --- 3. Deterministic fallback ----------------------------------------
    if (derivativeRequest) {
        if (const auto derivative = SymbolicDifferentiator::differentiate(
                derivativeRequest->expression, derivativeRequest->variable)) {
            outcome.kind = OutcomeKind::Math;
            outcome.expression =
                derivativeDisplay(derivativeRequest->variable, derivativeRequest->expression);
            outcome.result = *derivative;
            return outcome;
        }
    }

    if (const auto value = MathEvaluator::evaluateUtterance(outcome.normalized)) {
        outcome.kind = OutcomeKind::Math;
        outcome.expression = displayExpression(outcome.normalized);
        outcome.result = value->format();
        return outcome;
    }

    return outcome;
}

} // namespace dex
