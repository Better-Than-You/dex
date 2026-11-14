#pragma once

#include <QString>
#include <QStringList>

namespace dex {

/// Side-effecting sink for resolved actions. Kept separate from the router so
/// the deterministic parts of the pipeline stay unit-testable.
class ActionDispatcher
{
public:
    /// Copies text using `wl-copy`. Returns false (with a message) when the
    /// clipboard tool is unavailable or fails.
    static bool copyToClipboard(const QString &text, QString *error = nullptr);

    /// Launches a detached process.
    static bool launch(const QString &program, const QStringList &args, QString *error = nullptr);

private:
    ActionDispatcher() = default;
};

} // namespace dex
