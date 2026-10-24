#ifndef WBPY_H
#define WBPY_H

#include <string>
#include <vector>

#include "engine.h"
#include "ifreewb.h"
#include "py.h"
#include "wbzx.h"

namespace freewb
{

class Wbpy : public IFreewbEngine, public IFreewb
{
public:
    Wbpy(WbzxEngine *wbzxEngine, PyEngine *pyEngine);
    ~Wbpy() override;

    const char *name() const override;
    bool available() const override;
    void changeAvailable() override;

    void putKey(const char *strCode) override;
    const CandidatePayload &getResult() const override;
    void reset() override;

    bool shouldProcessKey(const char *key) const override;
    bool isExactDictionaryKey(const std::string &preedit) const override;
    std::size_t minTopScreenPreeditLength() const override;

private:
    WbzxEngine *wbzxEngine_ = nullptr;
    PyEngine *pyEngine_ = nullptr;

    bool available_ = true;
    CandidatePayload result_;
};
} // namespace freewb

#endif
