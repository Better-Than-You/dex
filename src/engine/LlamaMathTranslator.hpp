#pragma once

#include "core/Config.hpp"

#include <QObject>
#include <QString>
#include <QTimer>

#include <memory>
#include <optional>

namespace dex {

/// Canonical JSON command produced by the maths SLM.
struct MathCommand {
    QString mode; ///< "eval" or "diff"
    QString expr; ///< Clean algebraic expression.
    QString var;  ///< Differentiation variable (mode == "diff").
};

/// On-demand llama.cpp translator that turns noisy ASR transcripts into a
/// single canonical `{"mode":"eval|diff",...}` command.
///
/// Lifecycle: the GGUF is *not* loaded at startup. The first maths query pays
/// the cold-start cost; afterwards the model stays warm for
/// `math_slm.idle_unload_sec` and is then freed by a QTimer so idle RSS drops
/// back to zero. Each query uses its own short-lived context, because Qwen3.5
/// is a hybrid attention/recurrent model. Output is constrained by a GBNF
/// grammar, and the prompt prefills `<think\n</think\n` to suppress reasoning.
class LlamaMathTranslator : public QObject
{
    Q_OBJECT

public:
    explicit LlamaMathTranslator(MathSlmConfig config, QObject *parent = nullptr);
    ~LlamaMathTranslator() override;

    /// True when the backend was compiled in, the feature is enabled and the
    /// GGUF file exists. Does not load anything.
    bool available() const;

    /// True while the model/context are resident.
    bool loaded() const;

    /// Translates one transcript. Loads the model on demand. Returns nullopt
    /// when the model is unavailable, loading fails, or the JSON is invalid.
    std::optional<MathCommand> translate(const QString &transcript, QString *error = nullptr);

    /// Frees the context and model, returning idle memory to zero.
    void unload();

signals:
    /// Emitted after a successful translation; useful for live diagnostics.
    void translated(const QString &transcript, const QString &json);

private:
    struct Impl;

    bool ensureLoaded(QString *error);

    MathSlmConfig m_config;
    QTimer m_idleTimer;
    std::unique_ptr<Impl> m_impl;
};

} // namespace dex
