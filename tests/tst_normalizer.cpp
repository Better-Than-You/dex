#include "engine/TextNormalizer.hpp"

#include <QtTest>

using dex::TextNormalizer;

class TestNormalizer : public QObject
{
    Q_OBJECT

private slots:
    void wakePrefix();
    void spokenMath_data();
    void spokenMath();
    void decimalsAndPunctuation();
    void unarySpacing();
};

void TestNormalizer::wakePrefix()
{
    QCOMPARE(TextNormalizer::normalize(QStringLiteral("Hey Dex, search for Arch Linux Wiki.")),
             QStringLiteral("search for arch linux wiki"));
    QCOMPARE(TextNormalizer::normalize(QStringLiteral("Please open kitty")),
             QStringLiteral("open kitty"));
    QCOMPARE(TextNormalizer::normalize(QStringLiteral("  DEX   launch spotify  ")),
             QStringLiteral("launch spotify"));
}

void TestNormalizer::spokenMath_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expected");

    QTest::newRow("minus/into/l.n.e") << QStringLiteral("minus 2 into l.n.e")
                                      << QStringLiteral("-2 * ln(e)");
    QTest::newRow("multiplied by") << QStringLiteral("minus 2 multiplied by l.n.e")
                                   << QStringLiteral("-2 * ln(e)");
    QTest::newRow("plus/times") << QStringLiteral("10 plus 5 times 2")
                                << QStringLiteral("10 + 5 * 2");
    QTest::newRow("power") << QStringLiteral("2 power 3") << QStringLiteral("2 ^ 3");
    QTest::newRow("squared") << QStringLiteral("5 squared") << QStringLiteral("5 ^ 2");
    QTest::newRow("cubed") << QStringLiteral("2 cubed") << QStringLiteral("2 ^ 3");
    QTest::newRow("square root of") << QStringLiteral("square root of 16 plus 2 power 3")
                                    << QStringLiteral("sqrt 16 + 2 ^ 3");
    QTest::newRow("root") << QStringLiteral("root 81") << QStringLiteral("sqrt 81");
    QTest::newRow("divide") << QStringLiteral("10 divided by 2") << QStringLiteral("10 / 2");
    QTest::newRow("over") << QStringLiteral("10 over 2") << QStringLiteral("10 / 2");
}

void TestNormalizer::spokenMath()
{
    QFETCH(QString, input);
    QFETCH(QString, expected);
    QCOMPARE(TextNormalizer::normalize(input), expected);
}

void TestNormalizer::decimalsAndPunctuation()
{
    QCOMPARE(TextNormalizer::normalize(QStringLiteral("3.14 plus 2.5")),
             QStringLiteral("3.14 + 2.5"));
    QCOMPARE(TextNormalizer::normalize(QStringLiteral("what is 2 plus 2?")),
             QStringLiteral("what is 2 + 2"));
    QCOMPARE(TextNormalizer::normalize(QStringLiteral("calculate 3.5 times 2!")),
             QStringLiteral("calculate 3.5 * 2"));
}

void TestNormalizer::unarySpacing()
{
    QCOMPARE(TextNormalizer::normalize(QStringLiteral("negative 4 plus 1")),
             QStringLiteral("-4 + 1"));
    QCOMPARE(TextNormalizer::normalize(QStringLiteral("minus 2 ln(e)")),
             QStringLiteral("-2 ln(e)"));
}

QTEST_APPLESS_MAIN(TestNormalizer)
#include "tst_normalizer.moc"
