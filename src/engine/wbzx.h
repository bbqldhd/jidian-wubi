#ifndef WBZX_H
#define WBZX_H

#include <string>
#include <unordered_map>
#include <vector>

#include "autophrase.h"
#include "common.h"
#include "engine.h"
#include "freqorder.h"
#include "gb2312filter.h"
#include "ifreewb.h"
#include "userdict.h"

namespace freewb
{

class WbzxEngine : public IFreewbEngine, public IFreewb
{
public:
    WbzxEngine();
    ~WbzxEngine();

    const char *name() const override;
    bool available() const override;
    void changeAvailable() override;

    void putKey(const char *strCode) override;
    const CandidatePayload &getResult() const override;
    void reset() override;

    bool shouldProcessKey(const char *key) const override;
    bool isExactDictionaryKey(const std::string &preedit) const override;
    std::size_t minTopScreenPreeditLength() const override;
    /** 主五笔码表单字 UTF-8 → 首选码；无索引或查无则空串。供拼音 [xxxx] 反查等。 */
    std::string primaryWubiCodeForSingleHanziUtf8(const std::string &hz) const;
    /** 按码表造词规则为 UTF-8 词组计算五笔编码；失败或非五笔引擎返回空串。 */
    std::string calculateWubiPhraseCode(const std::string &phrase) const;

    void toggleCharset();
    /** 0=GB（启用 GB2312 过滤）, 1=GBK。 */
    int charSet() const;

    void reloadMainDictionary();
    void reloadUserDictionary();
    void reloadAutoPhraseDictionary();

    bool addUserWord(const std::string &code, const std::string &text);
    bool deleteUserWord(const std::string &code, const std::string &text);

    /** 上屏后处理自动词组。 */
    void addAutoPhrase(const std::string &committedText, const std::string &code);

    void adjustFreq(const std::string &code, const std::string &text);
    void reloadFreqOrder();

    /** 主五笔码表；供自动造词读取元数据与已有词条。 */
    const MbDictionaryTable &mainDictionaryTable() const;

    /** 按 fullCode 归并追加到 @p out；相同则 first 在前。
    其中 first 和 second 需已按 fullCode 排序。*/
    static void mergeCandidatesInCodeOrder(CandidatePayload &out, const CandidatePayload &first, const CandidatePayload &second);

private:
    void clearMbLoadState();
    void loadDictionary();
    /** 从 mbTable_ 单字码表构建单字→首选码（仅首字节为字母的编码参与）。 */
    void initSingleHanziPrimaryCodeFromMbTable();
    void fillCandidatePayloadPrompts(const std::string &preedit, CandidatePayload &payload) const;
    /** 按 [DeletedWord] 与 charset 过滤主码表候选（整段均为主表条目）。 */
    void filterMainDictCandidates(CandidatePayload &payload) const;

    MbDictionaryTable mbTable_;
    UserDict userDict_;
    Gb2312Filter gb2312Filter_;
    std::unordered_map<std::string, std::string> singleHanziPrimaryCode_;
    std::string inputCodes_;
    bool available_ = true;
    CandidatePayload result_;
    AutoPhrase autoPhrase_;
    FreqOrder freqOrder_;
};
} // namespace freewb

#endif
