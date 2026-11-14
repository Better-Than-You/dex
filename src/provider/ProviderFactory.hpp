#pragma once

#include "core/Config.hpp"
#include "provider/ITranscriptProvider.hpp"

#include <QString>

#include <memory>

namespace dex {

enum class ProviderKind { Stub, Sherpa };

struct ProviderSelection {
    ProviderKind kind = ProviderKind::Stub;
    QString reason; ///< Why this backend was chosen (useful in logs/tests).
};

/// Decides which transcript provider to build from `[asr]` configuration.
///
/// There is a single live backend: NVIDIA Parakeet (INT8, single-shot per VAD
/// utterance). `forceStub` comes from the `--stub` command-line flag and always
/// wins.
class ProviderFactory
{
public:
    /// Pure decision function; never touches the filesystem or audio.
    static ProviderSelection resolve(const AsrConfig &config, bool forceStub);

    /// Builds the selected provider. Returns nullptr and fills `error` when
    /// the Parakeet bundle is missing or incomplete.
    static std::unique_ptr<ITranscriptProvider> create(const AsrConfig &config,
                                                       bool forceStub,
                                                       QObject *parent = nullptr,
                                                       QString *error = nullptr);
};

} // namespace dex
