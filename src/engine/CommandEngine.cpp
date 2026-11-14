#include "engine/CommandEngine.hpp"

#include "engine/MathEvaluator.hpp"
#include "engine/TextNormalizer.hpp"

#include <QRegularExpression>

namespace dex {

CommandEngine::CommandEngine(RuleRouter &router, bool confirmByDefault)
    : m_router(router)
    , m_confirmByDefault(confirmByDefault)
{
}

QString CommandEngine::displayExpression(const QString &normalized)
{
    static const QRegularExpression prefix(
        QStringLiteral(R"(^(?:calculate|what is|solve)\s+)"));
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

    if (const auto value = MathEvaluator::evaluateUtterance(outcome.normalized)) {
        outcome.kind = OutcomeKind::Math;
        outcome.expression = displayExpression(outcome.normalized);
        outcome.result = MathEvaluator::format(*value);
        return outcome;
    }

    if (const auto route = m_router.route(outcome.normalized, m_confirmByDefault)) {
        outcome.route = *route;
        outcome.kind = route->confirm ? OutcomeKind::Confirmation : OutcomeKind::Action;
        return outcome;
    }

    return outcome;
}

} // namespace dex
