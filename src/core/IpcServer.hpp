#pragma once

#include <QByteArray>
#include <QHash>
#include <QLocalServer>
#include <QObject>
#include <QString>

class QLocalSocket;

namespace dex {

/// Newline-delimited control protocol on `$XDG_RUNTIME_DIR/dex.sock`.
///
///   SHOW | HIDE | TOGGLE | QUIT | CONFIRM | CANCEL
///   PARTIAL:<text>
///   FINAL:<text>
///   SET_MODEL:<moonshine|parakeet|stub>
class IpcServer : public QObject
{
    Q_OBJECT

public:
    explicit IpcServer(QObject *parent = nullptr);
    ~IpcServer() override;

    bool listen(const QString &socketPath, QString *error = nullptr);
    void close();

    QString socketPath() const { return m_path; }

    /// `/run/user/<uid>/dex.sock` (or the XDG runtime dir equivalent).
    static QString defaultSocketPath();

signals:
    void showRequested();
    void hideRequested();
    void toggleRequested();
    void partialReceived(const QString &text);
    void finalReceived(const QString &text);
    void setModelRequested(const QString &profile);
    void confirmRequested();
    void cancelRequested();
    void quitRequested();

private:
    void handleConnection();
    void readClient(QLocalSocket *client);
    void handleLine(const QString &line);

    QLocalServer m_server;
    QHash<QLocalSocket *, QByteArray> m_buffers;
    QString m_path;
};

} // namespace dex
