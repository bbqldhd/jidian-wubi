#ifndef AUTOPHRASE_H
#define AUTOPHRASE_H

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "common.h"

namespace freewb
{

class WbzxEngine;

struct AutoPhraseEntry
{
    std::string hz;
    std::string code;
};

/** 五笔自动造词：会话缓冲、码表与 autophrase.mb 落盘。 */
class AutoPhrase
{
public:
    explicit AutoPhrase(WbzxEngine &owner);
    ~AutoPhrase();

    bool enabled() const;

    /** 上屏后处理自动词组；选中会话自动词组上屏时写入 autophrase.mb。 */
    void add(const std::string &committedText, const std::string &code);

    void reloadDictionary();

    bool hasExactCode(const std::string &code) const;
    /** preedit 与编码键完全一致时追加候选（非前缀续码）。 */
    void appendCandidatesForExactCode(const std::string &code, CandidatePayload &out) const;

private:
    void addSingleChar(const std::string &committedText, const std::string &code);
    bool hzKnownInTables(const std::string &hz) const;
    static bool candidatePayloadHasEntry(const CandidatePayload &out, const std::string &code, const std::string &hz);
    void saveDictionary();
    void initTableMetadataIfEmpty();
    static std::string formatTableCreateTime();

    WbzxEngine &owner_;
    MbDictionaryTable table_;
    std::array<std::string, 4> ring_{};
    std::uint64_t recordIndex_ = 0;
    std::vector<AutoPhraseEntry> phrases_;
    std::unordered_set<std::string> phraseHzSet_;
    std::unordered_map<std::string, std::uint32_t> phraseIndexByCode_;
};

} // namespace freewb

#endif
