#ifndef EN_H
#define EN_H

#include <string>

#include "engine.h"
#include "ifreewb.h"

namespace freewb
{
class En : public IFreewbEngine, public IFreewb
{
public:
    En() = default;
    ~En() = default;

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
    bool available_ = true;
};
} // namespace freewb

#endif // EN_H
