#include "core/Config.hpp"
#include "engine/CommandEngine.hpp"
#include "engine/LlamaMathTranslator.hpp"
#include "engine/RuleRouter.hpp"

#include <QtTest>

#include <memory>

using dex::CommandEngine;
using dex::Config;
using dex::LlamaMathTranslator;
using dex::RuleRouter;

class TestEngine : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void mathsTakesPrecedence();
    void spokenMath_data();
    void spokenMath();
    void matchedAction();
    void confirmationGate();
    void unknownUtterance();
    void nonMathTrigger_data();
    void nonMathTrigger();
    void slmTranslatesSpeech_data();
    void slmTranslatesSpeech();

private:
    RuleRouter m_router;
    std::unique_ptr<CommandEngine> m_engine;
    std::unique_ptr<LlamaMathTranslator> m_translator;
    std::unique_ptr<CommandEngine> m_slmEngine;
};

void TestEngine::initTestCase()
{
    QString error;
    QVERIFY2(m_router.load(QStringLiteral(DEX_COMMANDS_TOML), &error), qPrintable(error));
    m_engine = std::make_unique<CommandEngine>(m_router, false);

    // The SLM is optional: it only participates when the GGUF exists. The
    // deterministic engine above never sees it, which exercises the fallback.
    dex::MathSlmConfig slm;
    slm.modelPath = dex::defaultSlmModelPath();
    slm.idleUnloadSec = 0; // keep it warm across the data rows
    m_translator = std::make_unique<LlamaMathTranslator>(slm);
    if (m_translator->available()) {
        m_slmEngine = std::make_unique<CommandEngine>(m_router, false);
        m_slmEngine->setMathTranslator(m_translator.get());
    }
}

void TestEngine::mathsTakesPrecedence()
{
    const auto outcome = m_engine->process(QStringLiteral("Hey Dex, minus 2 into l.n.e"));
    QCOMPARE(outcome.kind, CommandEngine::OutcomeKind::Math);
    QCOMPARE(outcome.expression, QStringLiteral("-2 * ln(e)"));
    QCOMPARE(outcome.result, QStringLiteral("-2"));
}

void TestEngine::spokenMath_data()
{
    QTest::addColumn<QString>("transcript");
    QTest::addColumn<QString>("expected");

    // The six real spoken transcripts from Phase 2.1.
    QTest::newRow("sigma") << QStringLiteral("sigma function of ln2")
                           << QStringLiteral("0.666667");
    QTest::newRow("softmax vector")
        << QStringLiteral("softness function for the vector one, zero point one comma minus three")
        << QStringLiteral("[0.7018, 0.2853, 0.0128]");
    QTest::newRow("derivative")
        << QStringLiteral("half into y hat minus y whole square in respect to y hat")
        << QStringLiteral("y_hat - y");
    QTest::newRow("minus decimal") << QStringLiteral("minus. is zero point eight")
                                   << QStringLiteral("-0.8");
    QTest::newRow("minus/into/l.n.e") << QStringLiteral("minus 2 into l.n.e")
                                      << QStringLiteral("-2");
    QTest::newRow("sqrt/power") << QStringLiteral("sqrt 16 plus 2 power 3")
                                << QStringLiteral("12");
}

void TestEngine::spokenMath()
{
    QFETCH(QString, transcript);
    QFETCH(QString, expected);

    const auto outcome = m_engine->process(transcript);
    QCOMPARE(outcome.kind, CommandEngine::OutcomeKind::Math);
    QVERIFY2(!outcome.usedSlm, "The deterministic engine must not call the SLM");
    QCOMPARE(outcome.result, expected);
}

void TestEngine::matchedAction()
{
    const auto outcome = m_engine->process(QStringLiteral("search hyprland layer shell"));
    QCOMPARE(outcome.kind, CommandEngine::OutcomeKind::Action);
    QCOMPARE(outcome.route.ruleName, QStringLiteral("web_search"));
    QCOMPARE(outcome.route.confirm, false);
    QCOMPARE(outcome.route.args.value(0),
             QStringLiteral("https://google.com/?q=hyprland%20layer%20shell"));
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

void TestEngine::nonMathTrigger_data()
{
    QTest::addColumn<QString>("transcript");

    // These words ("power", "what is") are maths triggers, but the utterances
    // carry nothing evaluable. They must not reach the SLM.
    QTest::newRow("turn the power off") << QStringLiteral("turn the power off");
    QTest::newRow("what is the weather") << QStringLiteral("what is the weather");
}

void TestEngine::nonMathTrigger()
{
    QFETCH(QString, transcript);

    const auto outcome = m_engine->process(transcript);
    QCOMPARE(outcome.kind, CommandEngine::OutcomeKind::None);
    QVERIFY2(!outcome.usedSlm, "A non-maths utterance must never reach the SLM");

    // The same holds when a translator is wired in: the maths gate stops it
    // before any translation is attempted.
    if (m_slmEngine) {
        const auto slmOutcome = m_slmEngine->process(transcript);
        QCOMPARE(slmOutcome.kind, CommandEngine::OutcomeKind::None);
        QVERIFY2(!slmOutcome.usedSlm, "The SLM gate must reject non-maths text");
    }
}

void TestEngine::slmTranslatesSpeech_data()
{
    QTest::addColumn<QString>("transcript");
    QTest::addColumn<QString>("expected");

    QTest::newRow("sigma") << QStringLiteral("sigma function of ln2")
                           << QStringLiteral("0.666667");
    QTest::newRow("softmax vector")
        << QStringLiteral("softness function for the vector one, zero point one comma minus three")
        << QStringLiteral("[0.7018, 0.2853, 0.0128]");
    QTest::newRow("derivative")
        << QStringLiteral("half into y hat minus y whole square in respect to y hat")
        << QStringLiteral("y_hat - y");
    QTest::newRow("minus decimal") << QStringLiteral("minus. is zero point eight")
                                   << QStringLiteral("-0.8");
}

void TestEngine::slmTranslatesSpeech()
{
    if (!m_slmEngine) {
        QSKIP("GGUF maths SLM not available; deterministic fallback already covered.");
    }

    QFETCH(QString, transcript);
    QFETCH(QString, expected);

    const auto outcome = m_slmEngine->process(transcript);
    QCOMPARE(outcome.kind, CommandEngine::OutcomeKind::Math);
    QVERIFY2(outcome.usedSlm, "The SLM engine should have translated this utterance");
    QVERIFY(!outcome.slmExpression.isEmpty());
    QVERIFY2(outcome.result == expected,
             qPrintable(QStringLiteral("SLM produced %1 (%2 %3), expected %4")
                            .arg(outcome.result, outcome.slmMode, outcome.slmExpression, expected)));
}

QTEST_APPLESS_MAIN(TestEngine)
#include "tst_engine.moc"
