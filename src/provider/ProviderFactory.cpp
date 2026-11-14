#include "provider/ProviderFactory.hpp"

#include "provider/ModelLayout.hpp"
#include "provider/SherpaTranscriptProvider.hpp"
#include "provider/StubTranscriptProvider.hpp"

#include <utility>

namespace dex {

ProviderSelection ProviderFactory::resolve(const AsrConfig &config, bool forceStub)
{
    if (forceStub)
        return {ProviderKind::Stub, QStringLiteral("--stub")};

    const QString provider = config.provider.trimmed().toLower();
    if (provider == QStringLiteral("stub")) {
        return {ProviderKind::Stub, QStringLiteral("asr.provider = \"stub\"")};
    }

    if (!provider.isEmpty() && provider != QStringLiteral("sherpa")
        && provider != QStringLiteral("auto")) {
        return {ProviderKind::Stub,
                QStringLiteral("Unknown asr.provider \"%1\"; using stub")
                    .arg(config.provider)};
    }

    return {ProviderKind::Sherpa, QStringLiteral("sherpa-onnx (Parakeet)")};
}

std::unique_ptr<ITranscriptProvider> ProviderFactory::create(const AsrConfig &config,
                                                             bool forceStub,
                                                             QObject *parent,
                                                             QString *error)
{
    const ProviderSelection selection = resolve(config, forceStub);
    if (selection.kind == ProviderKind::Stub)
        return std::make_unique<StubTranscriptProvider>(parent);

    auto provider = std::make_unique<SherpaTranscriptProvider>(config, parent);

    QString prepareError;
    if (!provider->prepare(&prepareError)) {
        if (error)
            *error = prepareError;
        return nullptr;
    }

    return provider;
}

} // namespace dex
