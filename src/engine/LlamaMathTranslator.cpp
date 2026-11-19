#include "engine/LlamaMathTranslator.hpp"

#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>

#include <cstdio>
#include <string>
#include <vector>

#if defined(DEX_HAVE_LLAMA)
#    include <llama.h>
#endif

namespace dex {
namespace {

Q_LOGGING_CATEGORY(slmLog, "dex.slm")

constexpr int ContextTokens = 1024;
constexpr int MaxGeneratedTokens = 96;

#if defined(DEX_HAVE_LLAMA)
void quietLlamaLog(ggml_log_level level, const char *text, void *)
{
    if (level >= GGML_LOG_LEVEL_ERROR)
        std::fputs(text, stderr);
}

/// Frees a per-query context. Qwen3.5 is a hybrid attention/recurrent model, so
/// a context is created for each translation instead of being reset; that keeps
/// the rolling SSM state from leaking between queries.
struct ContextGuard {
    llama_context *context = nullptr;

    ~ContextGuard()
    {
        if (context)
            llama_free(context);
    }
};
#endif

const char *kGrammar = R"GBNF(
root   ::= "{\"mode\":" mode ",\"expr\":" string (",\"var\":" string)? "}"
mode   ::= "\"eval\"" | "\"diff\""
string ::= "\"" [^"\\]* "\""
)GBNF";

std::string buildPrompt(const std::string &transcript)
{
    // Few-shot examples live in the user turn as `Input:`/`Output:` pairs, and
    // the transcript is the final `Input:`. With the 0.8B Qwen this ordering
    // is markedly more stable than `transcript => json` lines inside the
    // system message (which the model would occasionally answer with the
    // previous example's JSON).
    //
    // Qwen 3.5 would otherwise emit a reasoning block; the assistant turn is
    // pre-filled with an empty `<think … </think …>` block, and a GBNF grammar
    // forces a single compact JSON object.
    std::string prompt = R"PROMPT(<|im_start|>system
You convert spoken maths and machine-learning phrases into one compact JSON object.
Reply with JSON only, never with explanations.
<|im_end|>
<|im_start|>user
Input: sigma function of ln2
Output: {"mode":"eval","expr":"sigmoid(ln(2))"}

Input: softness function for the vector one, zero point one comma minus three
Output: {"mode":"eval","expr":"softmax([1, 0.1, -3])"}

Input: softmax of one two and three
Output: {"mode":"eval","expr":"softmax([1, 2, 3])"}

Input: evaluate relu of minus three point five
Output: {"mode":"eval","expr":"relu(-3.5)"}

Input: ray loo of minus three point five plus e power two
Output: {"mode":"eval","expr":"relu(-3.5)+exp(2)"}

Input: absolute value of log base ten of hundred minus square root of sixteen
Output: {"mode":"eval","expr":"abs(log(100)-sqrt(16))"}

Input: binary cross entropy of y and y_hat
Output: {"mode":"eval","expr":"-(y*ln(y_hat)+(1-y)*ln(1-y_hat))"}

Input: half into y hat minus y whole square in respect to y hat
Output: {"mode":"diff","expr":"0.5*(y_hat-y)^2","var":"y_hat"}

Input: derivative of sigmoid of x with respect to x
Output: {"mode":"diff","expr":"sigmoid(x)","var":"x"}

Input: derivative of sigmoid of w x plus b with respect to x
Output: {"mode":"diff","expr":"sigmoid(w*x+b)","var":"x"}

Input: differentiate sine x into cosine x wrt x
Output: {"mode":"diff","expr":"sin(x)*cos(x)","var":"x"}

Input: minus. is zero point eight
Output: {"mode":"eval","expr":"-0.8"}

Input: )PROMPT";
    prompt += transcript;
    prompt += R"PROMPT(
Output:
<|im_end|>
<|im_start|>assistant
<think
</think
)PROMPT";
    return prompt;
}

} // namespace

struct LlamaMathTranslator::Impl {
#if defined(DEX_HAVE_LLAMA)
    llama_model *model = nullptr;
#endif
};

LlamaMathTranslator::LlamaMathTranslator(MathSlmConfig config, QObject *parent)
    : QObject(parent)
    , m_config(std::move(config))
    , m_impl(std::make_unique<Impl>())
{
    m_idleTimer.setSingleShot(true);
    connect(&m_idleTimer, &QTimer::timeout, this, &LlamaMathTranslator::unload);
}

LlamaMathTranslator::~LlamaMathTranslator()
{
    unload();
}

bool LlamaMathTranslator::available() const
{
#if defined(DEX_HAVE_LLAMA)
    if (!m_config.enabled || m_config.modelPath.isEmpty())
        return false;
    return QFileInfo::exists(m_config.modelPath);
#else
    return false;
#endif
}

bool LlamaMathTranslator::loaded() const
{
#if defined(DEX_HAVE_LLAMA)
    return m_impl->model != nullptr;
#else
    return false;
#endif
}

bool LlamaMathTranslator::ensureLoaded(QString *error)
{
#if !defined(DEX_HAVE_LLAMA)
    if (error)
        *error = QStringLiteral("Dex was built without llama.cpp support.");
    return false;
#else
    if (m_impl->model)
        return true;

    if (!m_config.enabled) {
        if (error)
            *error = QStringLiteral("The maths SLM is disabled.");
        return false;
    }
    if (!QFileInfo::exists(m_config.modelPath)) {
        if (error)
            *error = QStringLiteral("Maths SLM model not found: %1").arg(m_config.modelPath);
        return false;
    }

    static bool backendInitialised = false;
    if (!backendInitialised) {
        llama_log_set(quietLlamaLog, nullptr);
        llama_backend_init();
        backendInitialised = true;
    }

    QElapsedTimer clock;
    clock.start();

    llama_model_params modelParams = llama_model_default_params();
    modelParams.n_gpu_layers = 0;

    const QByteArray path = m_config.modelPath.toUtf8();
    m_impl->model = llama_model_load_from_file(path.constData(), modelParams);
    if (!m_impl->model) {
        if (error)
            *error = QStringLiteral("Failed to load maths SLM: %1").arg(m_config.modelPath);
        return false;
    }

    qCInfo(slmLog) << "Loaded" << m_config.modelPath << "in" << clock.elapsed() << "ms";
    return true;
#endif
}

std::optional<MathCommand> LlamaMathTranslator::translate(const QString &transcript,
                                                          QString *error)
{
#if !defined(DEX_HAVE_LLAMA)
    Q_UNUSED(transcript)
    if (error)
        *error = QStringLiteral("Dex was built without llama.cpp support.");
    return std::nullopt;
#else
    if (!ensureLoaded(error))
        return std::nullopt;

    // Any successful query keeps the model warm for another idle window.
    if (m_config.idleUnloadSec > 0)
        m_idleTimer.start(m_config.idleUnloadSec * 1000);

    const llama_vocab *vocab = llama_model_get_vocab(m_impl->model);
    if (!vocab) {
        if (error)
            *error = QStringLiteral("The maths SLM has no vocabulary.");
        return std::nullopt;
    }

    const std::string prompt = buildPrompt(transcript.toStdString());

    std::vector<llama_token> tokens(prompt.size() + 16);
    int32_t count = llama_tokenize(vocab, prompt.c_str(), static_cast<int32_t>(prompt.size()),
                                   tokens.data(), static_cast<int32_t>(tokens.size()), true, false);
    if (count < 0) {
        tokens.resize(static_cast<std::size_t>(-count));
        count = llama_tokenize(vocab, prompt.c_str(), static_cast<int32_t>(prompt.size()),
                               tokens.data(), static_cast<int32_t>(tokens.size()), true, false);
    }
    // Leave room for the generated JSON object: the context must hold the
    // prompt plus every token of the reply.
    if (count <= 0 || count + MaxGeneratedTokens > ContextTokens) {
        if (error)
            *error = QStringLiteral("The maths SLM prompt does not fit the context window.");
        return std::nullopt;
    }
    tokens.resize(static_cast<std::size_t>(count));

    // A context is created per query (see ContextGuard); the model itself stays
    // memory-mapped until the idle timer unloads it.
    llama_context_params contextParams = llama_context_default_params();
    contextParams.n_ctx = ContextTokens;
    contextParams.n_batch = ContextTokens;
    contextParams.n_ubatch = ContextTokens;
    contextParams.n_threads = m_config.threads;
    contextParams.n_threads_batch = m_config.threads;

    ContextGuard context;
    context.context = llama_init_from_model(m_impl->model, contextParams);
    if (!context.context) {
        if (error)
            *error = QStringLiteral("Failed to create the maths SLM context.");
        return std::nullopt;
    }

    llama_sampler_chain_params samplerParams = llama_sampler_chain_default_params();
    samplerParams.no_perf = true;
    llama_sampler *sampler = llama_sampler_chain_init(samplerParams);
    llama_sampler *grammar = llama_sampler_init_grammar(vocab, kGrammar, "root");
    if (!grammar) {
        llama_sampler_free(sampler);
        if (error)
            *error = QStringLiteral("The maths SLM grammar failed to initialise.");
        return std::nullopt;
    }
    llama_sampler_chain_add(sampler, grammar);
    llama_sampler_chain_add(sampler, llama_sampler_init_greedy());

    const auto freeSampler = [&sampler] {
        if (sampler) {
            llama_sampler_free(sampler);
            sampler = nullptr;
        }
    };

    llama_batch batch = llama_batch_get_one(tokens.data(), static_cast<int32_t>(tokens.size()));
    if (llama_decode(context.context, batch) != 0) {
        freeSampler();
        if (error)
            *error = QStringLiteral("The maths SLM failed to decode the prompt.");
        return std::nullopt;
    }

    std::string output;
    for (int step = 0; step < MaxGeneratedTokens; ++step) {
        llama_token token = llama_sampler_sample(sampler, context.context, -1);
        if (llama_vocab_is_eog(vocab, token))
            break;

        char piece[256];
        const int32_t length = llama_token_to_piece(vocab, token, piece, sizeof(piece), 0, false);
        if (length > 0)
            output.append(piece, static_cast<std::size_t>(length));

        // The grammar only allows one object; stop as soon as it is closed so
        // no further tokens are needed.
        if (output.find('}') != std::string::npos)
            break;

        llama_batch next = llama_batch_get_one(&token, 1);
        if (llama_decode(context.context, next) != 0)
            break;
    }
    freeSampler();

    const std::size_t start = output.find('{');
    const std::size_t end = output.rfind('}');
    if (start == std::string::npos || end == std::string::npos || end <= start) {
        if (error)
            *error = QStringLiteral("The maths SLM returned no JSON object.");
        return std::nullopt;
    }

    const QByteArray json = QByteArray::fromStdString(output.substr(start, end - start + 1));
    qCDebug(slmLog) << "raw:" << QByteArray::fromStdString(output);
    const QJsonDocument document = QJsonDocument::fromJson(json);
    if (!document.isObject()) {
        if (error)
            *error = QStringLiteral("The maths SLM returned invalid JSON.");
        return std::nullopt;
    }

    const QJsonObject object = document.object();
    MathCommand command;
    command.mode = object.value(QStringLiteral("mode")).toString();
    command.expr = object.value(QStringLiteral("expr")).toString();
    command.var = object.value(QStringLiteral("var")).toString().trimmed();

    if ((command.mode != QStringLiteral("eval") && command.mode != QStringLiteral("diff"))
        || command.expr.isEmpty()) {
        if (error)
            *error = QStringLiteral("The maths SLM returned an unusable command.");
        return std::nullopt;
    }
    if (command.mode == QStringLiteral("diff") && command.var.isEmpty())
        command.var = QStringLiteral("x");

    emit translated(transcript, QString::fromUtf8(json));
    return command;
#endif
}

void LlamaMathTranslator::unload()
{
#if defined(DEX_HAVE_LLAMA)
    if (!m_impl)
        return;
    if (m_impl->model) {
        llama_model_free(m_impl->model);
        m_impl->model = nullptr;
        qCInfo(slmLog) << "Unloaded maths SLM; idle memory released.";
    }
#endif
    m_idleTimer.stop();
}

} // namespace dex
