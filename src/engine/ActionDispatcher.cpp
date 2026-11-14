#include "engine/ActionDispatcher.hpp"

#include <QProcess>

namespace dex {

bool ActionDispatcher::copyToClipboard(const QString &text, QString *error)
{
    QProcess process;
    process.setProgram(QStringLiteral("wl-copy"));
    process.start();

    if (!process.waitForStarted(1500)) {
        if (error)
            *error = QStringLiteral("wl-copy is not available: %1").arg(process.errorString());
        return false;
    }

    process.write(text.toUtf8());
    process.closeWriteChannel();

    if (!process.waitForFinished(2000)) {
        process.kill();
        process.waitForFinished(200);
        if (error)
            *error = QStringLiteral("wl-copy timed out.");
        return false;
    }

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (error) {
            const QString stderrText = QString::fromUtf8(process.readAllStandardError()).trimmed();
            *error = stderrText.isEmpty()
                ? QStringLiteral("wl-copy exited with code %1.").arg(process.exitCode())
                : stderrText;
        }
        return false;
    }

    return true;
}

bool ActionDispatcher::launch(const QString &program, const QStringList &args, QString *error)
{
    if (program.trimmed().isEmpty()) {
        if (error)
            *error = QStringLiteral("Refusing to launch an empty program.");
        return false;
    }

    if (!QProcess::startDetached(program, args)) {
        if (error)
            *error = QStringLiteral("Failed to launch `%1`.").arg(program);
        return false;
    }

    return true;
}

} // namespace dex
