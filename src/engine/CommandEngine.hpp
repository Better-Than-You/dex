#pragma once

#include "engine/RuleRouter.hpp"

#include <QString>

namespace dex {

/// Deterministic pipeline: normalize -> maths -> rule routing.
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
    };

    CommandEngine(RuleRouter &router, bool confirmByDefault);

    void setConfirmByDefault(bool value) { m_confirmByDefault = value; }

    Outcome process(const QString &rawText) const;

    /// Strips `calculate`, `what is`, `solve` for display purposes.
    static QString displayExpression(const QString &normalized);

private:
    RuleRouter &m_router;
    bool m_confirmByDefault = false;
};

} // namespace dex
