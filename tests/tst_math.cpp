#include "engine/MathEvaluator.hpp"
#include "engine/SymbolicDifferentiator.hpp"
#include "engine/TextNormalizer.hpp"

#include <QtTest>

#include <numbers>
#include <optional>

using dex::MathEvaluator;
using dex::MathResult;
using dex::SymbolicDifferentiator;
using dex::TextNormalizer;

namespace {
std::optional<MathResult> evaluate(const QString &raw)
{
    return MathEvaluator::evaluateUtterance(TextNormalizer::normalize(raw));
}

std::optional<double> scalar(const QString &raw)
{
    const auto result = evaluate(raw);
    if (!result)
        return std::nullopt;
    return result->scalarValue();
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
    void mlFunctions();
    void softmaxVector();
    void symbolicDerivatives();
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
    QTest::newRow("minus is decimal") << QStringLiteral("minus. is zero point eight") << -0.8;
    QTest::newRow("sigma") << QStringLiteral("sigma function of ln2") << 0.6666666666666666;
}

void TestMath::spokenMath()
{
    QFETCH(QString, input);
    QFETCH(double, expected);

    const auto value = scalar(input);
    QVERIFY(value.has_value());
    QVERIFY2(qFuzzyCompare(*value + 1.0, expected + 1.0),
             qPrintable(QStringLiteral("got %1 expected %2").arg(*value).arg(expected)));
}

void TestMath::precedence()
{
    auto result = scalar(QStringLiteral("2 plus 3 times 4"));
    QVERIFY(result.has_value());
    QCOMPARE(*result, 14.0);

    result = scalar(QStringLiteral("2 power 3 power 2"));
    QVERIFY(result.has_value());
    QCOMPARE(*result, 512.0);

    result = scalar(QStringLiteral("minus 2 power 2"));
    QVERIFY(result.has_value());
    QCOMPARE(*result, -4.0);

    result = scalar(QStringLiteral("10 modulo 4"));
    // `modulo` is not a spoken keyword; it must not evaluate.
    QVERIFY(!result.has_value());

    result = scalar(QStringLiteral("10 % 4"));
    QVERIFY(result.has_value());
    QCOMPARE(*result, 2.0);
}

void TestMath::implicitMultiplication()
{
    auto result = scalar(QStringLiteral("2 pi"));
    QVERIFY(result.has_value());
    QVERIFY(qFuzzyCompare(*result, 2.0 * std::numbers::pi));

    result = scalar(QStringLiteral("minus 2 ln(e)"));
    QVERIFY(result.has_value());
    QCOMPARE(*result, -2.0);

    result = scalar(QStringLiteral("3 (4 plus 1)"));
    QVERIFY(result.has_value());
    QCOMPARE(*result, 15.0);
}

void TestMath::functions()
{
    auto result = scalar(QStringLiteral("ln(e)"));
    QVERIFY(result.has_value());
    QVERIFY(qFuzzyCompare(*result, 1.0));

    result = scalar(QStringLiteral("log 1000"));
    QVERIFY(result.has_value());
    QVERIFY(qFuzzyCompare(*result, 3.0));

    result = scalar(QStringLiteral("sqrt 81"));
    QVERIFY(result.has_value());
    QCOMPARE(*result, 9.0);

    result = scalar(QStringLiteral("sin 0"));
    QVERIFY(result.has_value());
    QVERIFY(qFuzzyCompare(*result, 0.0));
}

void TestMath::mlFunctions()
{
    // sigmoid(ln(2)) = 1 / (1 + 0.5) = 0.666667
    const auto sigmoid = MathEvaluator::evaluate(QStringLiteral("sigmoid(ln(2))"));
    QVERIFY(sigmoid.has_value());
    QCOMPARE(sigmoid->format(), QStringLiteral("0.666667"));

    const auto relu = MathEvaluator::evaluate(QStringLiteral("relu(-2)"));
    QVERIFY(relu.has_value());
    QCOMPARE(relu->format(), QStringLiteral("0"));

    const auto exp = MathEvaluator::evaluate(QStringLiteral("exp(0) + relu(3)"));
    QVERIFY(exp.has_value());
    QCOMPARE(exp->format(), QStringLiteral("4"));
}

void TestMath::softmaxVector()
{
    const auto result = MathEvaluator::evaluate(QStringLiteral("softmax([1, 0.1, -3])"));
    QVERIFY(result.has_value());
    QVERIFY(result->isVector());
    QCOMPARE(result->values.size(), std::size_t(3));
    QCOMPARE(result->format(), QStringLiteral("[0.7018, 0.2853, 0.0128]"));
}

void TestMath::symbolicDerivatives()
{
    auto derivative = SymbolicDifferentiator::differentiate(
        QStringLiteral("0.5 * (y_hat - y)^2"), QStringLiteral("y_hat"));
    QVERIFY(derivative.has_value());
    QCOMPARE(*derivative, QStringLiteral("y_hat - y"));

    derivative = SymbolicDifferentiator::differentiate(QStringLiteral("3 * x^2 + 2 * x"),
                                                        QStringLiteral("x"));
    QVERIFY(derivative.has_value());
    QCOMPARE(*derivative, QStringLiteral("2 + 6 * x"));

    derivative = SymbolicDifferentiator::differentiate(QStringLiteral("sigmoid(x)"),
                                                        QStringLiteral("x"));
    QVERIFY(derivative.has_value());
    QCOMPARE(*derivative, QStringLiteral("sigmoid(x) * (1 - sigmoid(x))"));

    derivative = SymbolicDifferentiator::differentiate(QStringLiteral("x^2"), QStringLiteral("t"));
    QVERIFY(derivative.has_value());
    QCOMPARE(*derivative, QStringLiteral("0"));
}

void TestMath::rejectsInvalid()
{
    QVERIFY(!scalar(QStringLiteral("10 divided by 0")).has_value());
    QVERIFY(!scalar(QStringLiteral("10 % 0")).has_value());
    QVERIFY(!scalar(QStringLiteral("2 plus")).has_value());
    QVERIFY(!scalar(QStringLiteral("search arch linux")).has_value());
    QVERIFY(!scalar(QStringLiteral("open kitty")).has_value());
    QVERIFY(!scalar(QString()).has_value());
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
