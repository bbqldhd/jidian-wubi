#ifndef PY_H
#define PY_H

#include <functional>
#include <string>
#include <unordered_map>
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
    /** 把整串拼音切成音节/词组，拼成一整句候选（词库没有整串条目时才有意义）。
     *  返回按得分从高到低、最多 kMaxSentenceVariants 条整句文本。 */
    std::vector<std::string> composeSentences(const std::string &code) const;
    /** 把整句候选并入 result_：有完整匹配时排在末尾，否则排在最前。 */
    void appendSentenceCandidates(const std::string &prefix);
    /** 智能纠错：把用户可能漏打/多打的 h、g 纠回来（z/zh、c/ch、s/sh、an/ang、en/eng、in/ing）。
     *  只在「整串没有命中任何词条」时才启用，正确输入不受影响。 */
    void appendFuzzyCandidates(const std::string &prefix);
    /** 生成模糊音变体：每个位置只改一处，先补字母（短→长）再删字母（长→短）。 */
    std::vector<std::string> fuzzyVariants(const std::string &code) const;
    /** 一个编码最少能切成几个音节（无法完全切分返回 -1）。用于剔除词库里的简码/缩写码。 */
    int syllableCount(const std::string &code, bool allowSingleLetter = true) const;
    /** 为整句切分挑一个词：优先词组表，字数必须等于编码的音节数。 */
    bool pickWordForSentence(const std::string &code, std::string &text) const;

private:
    MbDictionaryTable mbTable_;
    FreqOrder freqOrder_;
    WubiPrimaryCodeLookupCallback wubiPrimaryCodeLookupCallback_;
    std::string inputCodes_;
    bool available_ = true;
    bool loaded_ = false;
    CandidatePayload result_;
    mutable std::unordered_map<std::string, int> syllableCache_;
    mutable std::unordered_map<std::string, int> strictSyllableCache_;
};

} // namespace freewb

#endif
