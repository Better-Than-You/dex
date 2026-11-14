#include "provider/StubTranscriptProvider.hpp"

namespace dex {

StubTranscriptProvider::StubTranscriptProvider(QObject *parent)
    : ITranscriptProvider(parent)
{
}

void StubTranscriptProvider::start()
{
    emit stateChanged(State::Listening);
}

void StubTranscriptProvider::stop()
{
    emit stateChanged(State::Idle);
}

bool StubTranscriptProvider::setModelProfile(const QString &profile)
{
    m_profile = profile;
    emit stateChanged(State::Listening);
    return true;
}

void StubTranscriptProvider::feedPartial(const QString &text)
{
    emit stateChanged(State::Listening);
    emit partialTextChanged(text);
}

void StubTranscriptProvider::feedFinal(const QString &text)
{
    emit stateChanged(State::Finalizing);
    emit utteranceFinalized(text);
    emit stateChanged(State::Listening);
}

} // namespace dex
