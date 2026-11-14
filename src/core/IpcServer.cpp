#include "core/IpcServer.hpp"

#include <QDir>
#include <QLocalSocket>

#include <unistd.h>

#include <utility>

namespace dex {

IpcServer::IpcServer(QObject *parent)
    : QObject(parent)
{
    connect(&m_server, &QLocalServer::newConnection, this, &IpcServer::handleConnection);
}

IpcServer::~IpcServer()
{
    close();
}

QString IpcServer::defaultSocketPath()
{
    const QString runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
    if (!runtime.isEmpty())
        return QDir(runtime).filePath(QStringLiteral("dex.sock"));
    return QDir::temp().filePath(QStringLiteral("dex-%1.sock").arg(::getuid()));
}

bool IpcServer::listen(const QString &socketPath, QString *error)
{
    close();
    m_path = socketPath;

    if (!m_server.listen(m_path)) {
        // A stale socket from a previous crashed daemon.
        if (m_server.serverError() == QAbstractSocket::AddressInUseError) {
            QLocalServer::removeServer(m_path);
            if (m_server.listen(m_path))
                return true;
        }
        if (error)
            *error = QStringLiteral("Cannot listen on %1: %2").arg(m_path, m_server.errorString());
        return false;
    }

    return true;
}

void IpcServer::close()
{
    for (QLocalSocket *client : m_buffers.keys())
        client->deleteLater();
    m_buffers.clear();

    if (m_server.isListening())
        m_server.close();
    if (!m_path.isEmpty())
        QLocalServer::removeServer(m_path);
}

void IpcServer::handleConnection()
{
    while (QLocalSocket *client = m_server.nextPendingConnection()) {
        m_buffers.insert(client, QByteArray());
        connect(client, &QLocalSocket::readyRead, this, [this, client] { readClient(client); });
        connect(client, &QLocalSocket::disconnected, this, [this, client] {
            m_buffers.remove(client);
            client->deleteLater();
        });
    }
}

void IpcServer::readClient(QLocalSocket *client)
{
    auto buffer = m_buffers.find(client);
    if (buffer == m_buffers.end())
        return;

    buffer->append(client->readAll());

    int newline = buffer->indexOf('\n');
    while (newline >= 0) {
        const QByteArray line = buffer->left(newline);
        buffer->remove(0, newline + 1);
        handleLine(QString::fromUtf8(line).trimmed());
        newline = buffer->indexOf('\n');
    }
}

void IpcServer::handleLine(const QString &line)
{
    if (line.isEmpty())
        return;

    if (line.startsWith(QStringLiteral("PARTIAL:"))) {
        emit partialReceived(line.mid(8));
        return;
    }
    if (line.startsWith(QStringLiteral("FINAL:"))) {
        emit finalReceived(line.mid(6));
        return;
    }

    const QString command = line.toUpper();
    if (command == QStringLiteral("SHOW")) {
        emit showRequested();
    } else if (command == QStringLiteral("HIDE")) {
        emit hideRequested();
    } else if (command == QStringLiteral("TOGGLE")) {
        emit toggleRequested();
    } else if (command == QStringLiteral("CONFIRM")) {
        emit confirmRequested();
    } else if (command == QStringLiteral("CANCEL")) {
        emit cancelRequested();
    } else if (command == QStringLiteral("QUIT")) {
        emit quitRequested();
    }
}

} // namespace dex
