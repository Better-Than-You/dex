#include "core/IpcServer.hpp"

#include <QLocalSocket>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class TestIpc : public QObject
{
    Q_OBJECT

private slots:
    void controlCommands();
    void transcriptCommands();
    void modelCommands();
    void bufferedLines();

private:
    QTemporaryDir m_dir;
};

void TestIpc::controlCommands()
{
    dex::IpcServer server;
    QString error;
    QVERIFY2(server.listen(m_dir.filePath(QStringLiteral("control.sock")), &error),
             qPrintable(error));

    QSignalSpy show(&server, &dex::IpcServer::showRequested);
    QSignalSpy hide(&server, &dex::IpcServer::hideRequested);
    QSignalSpy toggle(&server, &dex::IpcServer::toggleRequested);
    QSignalSpy confirm(&server, &dex::IpcServer::confirmRequested);
    QSignalSpy cancel(&server, &dex::IpcServer::cancelRequested);
    QSignalSpy quit(&server, &dex::IpcServer::quitRequested);

    QLocalSocket socket;
    socket.connectToServer(server.socketPath());
    QVERIFY(socket.waitForConnected(1000));
    socket.write("SHOW\nTOGGLE\nHIDE\nCONFIRM\nCANCEL\nQUIT\n");
    socket.flush();

    QTRY_COMPARE(show.count(), 1);
    QTRY_COMPARE(toggle.count(), 1);
    QTRY_COMPARE(hide.count(), 1);
    QTRY_COMPARE(confirm.count(), 1);
    QTRY_COMPARE(cancel.count(), 1);
    QTRY_COMPARE(quit.count(), 1);
}

void TestIpc::transcriptCommands()
{
    dex::IpcServer server;
    QString error;
    QVERIFY2(server.listen(m_dir.filePath(QStringLiteral("transcript.sock")), &error),
             qPrintable(error));

    QSignalSpy partial(&server, &dex::IpcServer::partialReceived);
    QSignalSpy final(&server, &dex::IpcServer::finalReceived);

    QLocalSocket socket;
    socket.connectToServer(server.socketPath());
    QVERIFY(socket.waitForConnected(1000));
    socket.write("PARTIAL:minus 2 into\nFINAL:minus 2 into l.n.e\n");
    socket.flush();

    QTRY_COMPARE(partial.count(), 1);
    QTRY_COMPARE(final.count(), 1);
    QCOMPARE(partial.at(0).at(0).toString(), QStringLiteral("minus 2 into"));
    QCOMPARE(final.at(0).at(0).toString(), QStringLiteral("minus 2 into l.n.e"));
}

void TestIpc::modelCommands()
{
    dex::IpcServer server;
    QString error;
    QVERIFY2(server.listen(m_dir.filePath(QStringLiteral("model.sock")), &error),
             qPrintable(error));

    QSignalSpy setModel(&server, &dex::IpcServer::setModelRequested);

    QLocalSocket socket;
    socket.connectToServer(server.socketPath());
    QVERIFY(socket.waitForConnected(1000));
    socket.write("SET_MODEL:parakeet\nset_model: stub \n");
    socket.flush();

    QTRY_COMPARE(setModel.count(), 2);
    QCOMPARE(setModel.at(0).at(0).toString(), QStringLiteral("parakeet"));
    QCOMPARE(setModel.at(1).at(0).toString(), QStringLiteral("stub"));
}

void TestIpc::bufferedLines()
{
    dex::IpcServer server;
    QString error;
    QVERIFY2(server.listen(m_dir.filePath(QStringLiteral("buffered.sock")), &error),
             qPrintable(error));

    QSignalSpy final(&server, &dex::IpcServer::finalReceived);

    QLocalSocket socket;
    socket.connectToServer(server.socketPath());
    QVERIFY(socket.waitForConnected(1000));

    socket.write("FINAL:search hypr");
    socket.flush();
    QTest::qWait(50);
    QCOMPARE(final.count(), 0);

    socket.write("land layer shell\n");
    socket.flush();

    QTRY_COMPARE(final.count(), 1);
    QCOMPARE(final.at(0).at(0).toString(), QStringLiteral("search hyprland layer shell"));
}

QTEST_GUILESS_MAIN(TestIpc)
#include "tst_ipc.moc"
