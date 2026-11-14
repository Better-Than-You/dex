#include "engine/CommandEngine.hpp"
#include "engine/RuleRouter.hpp"

#include <QtTest>

#include <memory>

using dex::CommandEngine;
using dex::RuleRouter;

class TestEngine : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void mathsTakesPrecedence();
    void matchedAction();
    void confirmationGate();
    void unknownUtterance();

private:
    RuleRouter m_router;
    std::unique_ptr<CommandEngine> m_engine;
};

void TestEngine::initTestCase()
{
    QString error;
    QVERIFY2(m_router.load(QStringLiteral(DEX_COMMANDS_TOML), &error), qPrintable(error));
    m_engine = std::make_unique<CommandEngine>(m_router, false);
}

void TestEngine::mathsTakesPrecedence()
{
    const auto outcome = m_engine->process(QStringLiteral("Hey Dex, minus 2 into l.n.e"));
    QCOMPARE(outcome.kind, CommandEngine::OutcomeKind::Math);
    QCOMPARE(outcome.expression, QStringLiteral("-2 * ln(e)"));
    QCOMPARE(outcome.result, QStringLiteral("-2"));
}

void TestEngine::matchedAction()
{
    const auto outcome = m_engine->process(QStringLiteral("search hyprland layer shell"));
    QCOMPARE(outcome.kind, CommandEngine::OutcomeKind::Action);
    QCOMPARE(outcome.route.ruleName, QStringLiteral("web_search"));
    QCOMPARE(outcome.route.confirm, false);
    QCOMPARE(outcome.route.args.value(0),
             QStringLiteral("https://duckduckgo.com/?q=hyprland%20layer%20shell"));
}

void TestEngine::confirmationGate()
{
    CommandEngine confirmEngine(m_router, true);
    const auto outcome = confirmEngine.process(QStringLiteral("search rust"));
    QCOMPARE(outcome.kind, CommandEngine::OutcomeKind::Confirmation);
    QVERIFY(outcome.route.confirm);
}

void TestEngine::unknownUtterance()
{
    const auto outcome = m_engine->process(QStringLiteral("dance for me"));
    QCOMPARE(outcome.kind, CommandEngine::OutcomeKind::None);
    QCOMPARE(outcome.normalized, QStringLiteral("dance for me"));
}

QTEST_APPLESS_MAIN(TestEngine)
#include "tst_engine.moc"
