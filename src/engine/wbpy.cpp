#include "wbpy.h"

namespace freewb
{

Wbpy::Wbpy(WbzxEngine *wbzxEngine, PyEngine *pyEngine) : wbzxEngine_(wbzxEngine), pyEngine_(pyEngine)
{
}

Wbpy::~Wbpy()
{
}

const char *Wbpy::name() const
{
    return "engine:wbpy";
}

bool Wbpy::available() const
{
    return available_;
}

void Wbpy::changeAvailable()
{
    available_ = !available_;
}

void Wbpy::putKey(const char *strCode)
{
    result_.clearRows();

    if (strCode == nullptr)
    {
        return;
    }

    if (wbzxEngine_ == nullptr || pyEngine_ == nullptr)
    {
        return;
    }

    wbzxEngine_->putKey(strCode);
    pyEngine_->putKey(strCode);

    const CandidatePayload &wubiHits = wbzxEngine_->getResult();
    const CandidatePayload &pinyinHits = pyEngine_->getResult();
    WbzxEngine::mergeCandidatesInCodeOrder(result_, wubiHits, pinyinHits);
}

const CandidatePayload &Wbpy::getResult() const
{
    return result_;
}

void Wbpy::reset()
{
    wbzxEngine_->reset();
    pyEngine_->reset();
    result_.clearRows();
}

bool Wbpy::shouldProcessKey(const char *key) const
{
    return wbzxEngine_->shouldProcessKey(key) || pyEngine_->shouldProcessKey(key);
}

bool Wbpy::isExactDictionaryKey(const std::string &preedit) const
{
    if (wbzxEngine_ == nullptr || pyEngine_ == nullptr)
    {
        return false;
    }
    return wbzxEngine_->isExactDictionaryKey(preedit) || pyEngine_->isExactDictionaryKey(preedit);
}

std::size_t Wbpy::minTopScreenPreeditLength() const
{
    return 4;
}

} // namespace freewb
