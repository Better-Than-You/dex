#include "engine/RuleRouter.hpp"

#include <QtTest>

using dex::RuleRouter;

class TestRouter : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void webSearch();
    void youtubeSearch();
    void launchApp();
    void workspace();
    void confirmationDefault();
    void unknownCommand();
    void browserResolution();
    void percentEncoding();

private:
    RuleRouter m_router;
};

void TestRouter::initTestCase()
{
    QString error;
    QVERIFY2(m_router.load(QStringLiteral(DEX_COMMANDS_TOML), &error), qPrintable(error));
    QCOMPARE(m_router.rules().size(), 4);
}

void TestRouter::webSearch()
{
    const auto route = m_router.route(QStringLiteral("search C++20 concepts"), false);
    QVERIFY(route.has_value());
    QCOMPARE(route->ruleName, QStringLiteral("web_search"));
    QCOMPARE(route->type, QStringLiteral("url"));
    QCOMPARE(route->args.value(0),
             QStringLiteral("https://google.com/?q=C%2B%2B20%20concepts"));
    QCOMPARE(route->label, QStringLiteral("Search"));
    QCOMPARE(route->detail, QStringLiteral("C++20 concepts"));
    QVERIFY(!route->confirm);
}

void TestRouter::youtubeSearch()
{
    const auto route = m_router.route(QStringLiteral("youtube lofi beats"), false);
    QVERIFY(route.has_value());
    QCOMPARE(route->ruleName, QStringLiteral("youtube_search"));
    QCOMPARE(route->type, QStringLiteral("exec"));
    QCOMPARE(route->program, QStringLiteral("brave-origin"));
    // exec rules pass the URL as one argv element; only url rules encode slots.
    QCOMPARE(route->args, (QStringList{QStringLiteral("--profile-directory=Default"),
                                       QStringLiteral("https://www.youtube.com/results?search_query=lofi beats")}));

    const auto alternate = m_router.route(QStringLiteral("play lofi beats on youtube"), false);
    QVERIFY(alternate.has_value());
    QCOMPARE(alternate->ruleName, QStringLiteral("youtube_search"));
}

void TestRouter::launchApp()
{
    const auto route = m_router.route(QStringLiteral("launch kitty"), false);
    QVERIFY(route.has_value());
    QCOMPARE(route->ruleName, QStringLiteral("launch_app"));
    QCOMPARE(route->program, QStringLiteral("kitty"));
    QVERIFY(route->args.isEmpty());
}

void TestRouter::workspace()
{
    const auto route = m_router.route(QStringLiteral("workspace 4"), true);
    QVERIFY(route.has_value());
    QCOMPARE(route->ruleName, QStringLiteral("hypr_workspace"));
    QCOMPARE(route->program, QStringLiteral("hyprctl"));
    QCOMPARE(route->args, (QStringList{QStringLiteral("dispatch"),
                                       QStringLiteral("workspace"),
                                       QStringLiteral("4")}));
    // Explicit `confirm = false` wins over confirm_by_default = true.
    QVERIFY(!route->confirm);

    const auto switchTo = m_router.route(QStringLiteral("switch to workspace 10"), false);
    QVERIFY(switchTo.has_value());
    QCOMPARE(switchTo->args.value(2), QStringLiteral("10"));
}

void TestRouter::confirmationDefault()
{
    const auto route = m_router.route(QStringLiteral("search rust"), true);
    QVERIFY(route.has_value());
    QVERIFY(route->confirm);

    const auto noConfirm = m_router.route(QStringLiteral("search rust"), false);
    QVERIFY(noConfirm.has_value());
    QVERIFY(!noConfirm->confirm);
}

void TestRouter::unknownCommand()
{
    QVERIFY(!m_router.route(QStringLiteral("dance for me"), false).has_value());
    QVERIFY(!m_router.route(QString(), false).has_value());
}

void TestRouter::browserResolution()
{
    const QByteArray previous = qgetenv("BROWSER");
    qputenv("BROWSER", "firefox --new-window");

    const auto route = m_router.route(QStringLiteral("search hyprland"), false);
    QVERIFY(route.has_value());
    QCOMPARE(route->program, QStringLiteral("firefox"));
    QCOMPARE(route->args.value(0), QStringLiteral("--new-window"));
    QCOMPARE(route->args.value(1),
             QStringLiteral("https://google.com/?q=hyprland"));

    if (previous.isEmpty())
        qunsetenv("BROWSER");
    else
        qputenv("BROWSER", previous);
}

void TestRouter::percentEncoding()
{
    QCOMPARE(RuleRouter::substitute(QStringLiteral("https://x/?q=%{query}"),
                                    {{QStringLiteral("query"), QStringLiteral("a b&c=d")}},
                                    true),
             QStringLiteral("https://x/?q=a%20b%26c%3Dd"));
    QCOMPARE(RuleRouter::substitute(QStringLiteral("%{app}"),
                                    {{QStringLiteral("app"), QStringLiteral("kitty")}},
                                    false),
             QStringLiteral("kitty"));
}

QTEST_APPLESS_MAIN(TestRouter)
#include "tst_router.moc"
