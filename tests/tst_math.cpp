#include "engine/MathEvaluator.hpp"
#include "engine/TextNormalizer.hpp"

#include <QtTest>

#include <numbers>
#include <optional>

using dex::MathEvaluator;
using dex::TextNormalizer;

namespace {
std::optional<double> evaluate(const QString &raw)
{
    return MathEvaluator::evaluateUtterance(TextNormalizer::normalize(raw));
}
} // namespace

class TestMath : public QObject
{
    Q_OBJECT

private slots:
    void spokenMath_data();
    void spokenMath();
    void precedence();
    void implicitMultiplication();
    void functions();
    void rejectsInvalid();
    void formatting();
};

void TestMath::spokenMath_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<double>("expected");

    QTest::newRow("minus 2 multiplied by l.n.e") << QStringLiteral("minus 2 multiplied by l.n.e") << -2.0;
    QTest::newRow("minus 2 into l.n.e") << QStringLiteral("minus 2 into l.n.e") << -2.0;
    QTest::newRow("10 plus 5 times 2") << QStringLiteral("10 plus 5 times 2") << 20.0;
    QTest::newRow("sqrt 16 plus 2 power 3") << QStringLiteral("sqrt 16 plus 2 power 3") << 12.0;
    QTest::newRow("what is") << QStringLiteral("what is 2 plus 2") << 4.0;
    QTest::newRow("calculate") << QStringLiteral("calculate 3.5 times 2") << 7.0;
    QTest::newRow("solve") << QStringLiteral("solve 100 divided by 4") << 25.0;
    QTest::newRow("squared") << QStringLiteral("5 squared") << 25.0;
    QTest::newRow("cubed") << QStringLiteral("2 cubed") << 8.0;
}

void TestMath::spokenMath()
{
    QFETCH(QString, input);
    QFETCH(double, expected);

    const auto result = evaluate(input);
    QVERIFY(result.has_value());
    QCOMPARE(*result, expected);
}

void TestMath::precedence()
{
    auto result = evaluate(QStringLiteral("2 plus 3 times 4"));
    QVERIFY(result.has_value());
    QCOMPARE(*result, 14.0);

    result = evaluate(QStringLiteral("2 power 3 power 2"));
    QVERIFY(result.has_value());
    QCOMPARE(*result, 512.0);

    result = evaluate(QStringLiteral("minus 2 power 2"));
    QVERIFY(result.has_value());
    QCOMPARE(*result, -4.0);

    result = evaluate(QStringLiteral("10 modulo 4"));
    // `modulo` is not a spoken keyword; it must not evaluate.
    QVERIFY(!result.has_value());

    result = evaluate(QStringLiteral("10 % 4"));
    QVERIFY(result.has_value());
    QCOMPARE(*result, 2.0);
}

void TestMath::implicitMultiplication()
{
    auto result = evaluate(QStringLiteral("2 pi"));
    QVERIFY(result.has_value());
    QVERIFY(qFuzzyCompare(*result, 2.0 * std::numbers::pi));

    result = evaluate(QStringLiteral("minus 2 ln(e)"));
    QVERIFY(result.has_value());
    QCOMPARE(*result, -2.0);

    result = evaluate(QStringLiteral("3 (4 plus 1)"));
    QVERIFY(result.has_value());
    QCOMPARE(*result, 15.0);
}

void TestMath::functions()
{
    auto result = evaluate(QStringLiteral("ln(e)"));
    QVERIFY(result.has_value());
    QVERIFY(qFuzzyCompare(*result, 1.0));

    result = evaluate(QStringLiteral("log 1000"));
    QVERIFY(result.has_value());
    QVERIFY(qFuzzyCompare(*result, 3.0));

    result = evaluate(QStringLiteral("sqrt 81"));
    QVERIFY(result.has_value());
    QCOMPARE(*result, 9.0);

    result = evaluate(QStringLiteral("sin 0"));
    QVERIFY(result.has_value());
    QVERIFY(qFuzzyCompare(*result, 0.0));
}

void TestMath::rejectsInvalid()
{
    QVERIFY(!evaluate(QStringLiteral("10 divided by 0")).has_value());
    QVERIFY(!evaluate(QStringLiteral("10 % 0")).has_value());
    QVERIFY(!evaluate(QStringLiteral("2 plus")).has_value());
    QVERIFY(!evaluate(QStringLiteral("search arch linux")).has_value());
    QVERIFY(!evaluate(QStringLiteral("open kitty")).has_value());
    QVERIFY(!evaluate(QString()).has_value());
}

void TestMath::formatting()
{
    QCOMPARE(MathEvaluator::format(12.0), QStringLiteral("12"));
    QCOMPARE(MathEvaluator::format(-2.0), QStringLiteral("-2"));
    QCOMPARE(MathEvaluator::format(20.0), QStringLiteral("20"));
    QCOMPARE(MathEvaluator::format(0.5), QStringLiteral("0.5"));
    QCOMPARE(MathEvaluator::format(-0.0), QStringLiteral("0"));
    QCOMPARE(MathEvaluator::format(0.1 + 0.2), QStringLiteral("0.3"));
}

QTEST_APPLESS_MAIN(TestMath)
#include "tst_math.moc"
