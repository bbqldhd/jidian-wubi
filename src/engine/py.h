#ifndef PY_H
#define PY_H

#include <functional>
#include <string>
#include <vector>

#include "common.h"
#include "engine.h"
#include "freqorder.h"
#include "ifreewb.h"

namespace freewb
{

/** 单字 UTF-8 → 五笔首选码；空表示不做反查。 */
using WubiPrimaryCodeLookupCallback = std::function<std::string(const std::string &hzUtf8)>;

class PyEngine : public IFreewbEngine, public IFreewb
{
public:
    PyEngine();
    ~PyEngine();

    const char *name() const override;
    bool available() const override;
    void changeAvailable() override;

    void putKey(const char *strCode) override;
    const CandidatePayload &getResult() const override;
    void reset() override;

    bool shouldProcessKey(const char *key) const override;
    bool isExactDictionaryKey(const std::string &preedit) const override;
    std::size_t minTopScreenPreeditLength() const override;

    /** 单字五笔首选码查询 */
    void setWubiPrimaryCodeLookupCallback(WubiPrimaryCodeLookupCallback callback);

    void reloadMainDictionary();
    void adjustFreq(const std::string &code, const std::string &text);
    void reloadFreqOrder();

private:
    void clearMbLoadState();
    void loadDictionary();
    // 首次真正用到拼音时才解析词库：默认是五笔，切输入法时不该为拼音词库付启动成本
    void ensureLoaded();
    void fillCandidatePayloadPrompts(const std::string &preedit, CandidatePayload &payload) const;

private:
    MbDictionaryTable mbTable_;
    FreqOrder freqOrder_;
    WubiPrimaryCodeLookupCallback wubiPrimaryCodeLookupCallback_;
    std::string inputCodes_;
    bool available_ = true;
    bool loaded_ = false;
    CandidatePayload result_;
};

} // namespace freewb

#endif
