#ifndef COMMON_H
#define COMMON_H

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "engine.h"

namespace freewb
{

class MbDictionaryTable
{
public:
    MbDictionaryTable() = default;

    /** UTF-8 字节串中的字符数（按首字节判定宽度；非法序列按单字节步进）。 */
    static std::size_t utf8CharCount(const std::string &s);
    /** 取 UTF-8 串中从 0 起第 @p index 个字符（越界返回空串）。 */
    static std::string utf8CharAt(const std::string &s, std::size_t index);
    /** 取 UTF-8 串中从末尾起第 @p which 个字符（1 表示最后一字）。 */
    static std::string utf8CharAtFromEnd(const std::string &s, std::size_t which);

    void clear();
    bool loadFromStream(std::ifstream &in, const char *linePrefix = nullptr);
    bool saveToStream(std::ofstream &out) const;
    bool loadFromFile(const std::string &path, const char *linePrefix = nullptr);
    bool saveToFile(const std::string &path) const;
    /** 按编码递增顺序追加一条记录（txt 词库导入）；乱序或重复则失败/跳过。 */
    bool appendSortedRecord(const std::string &code, const std::string &text);
    /** 按编码序插入一条记录（自动词组等运行时写入）；已存在则跳过。 */
    bool insertRecord(const std::string &code, const std::string &text);
    void setMetadata(const std::string &tableName, const std::string &tableInfo, const std::string &tableCreateTime,
                     const std::string &endKeys, const std::string &specialKeys, const std::string &codeType,
                     const std::string &straightUpKeys, const std::string &inputCode, uint8_t wildChar, uint8_t hasRule,
                     const std::vector<EngineRuleBlock> &rules);
    /** 向 @p out 追加候选：与 texts 同步写入 fullCodes（完整编码键）。
     * 编码按字典序；同码内按码表 records_ 写入顺序（单字/词组混排，不拆分）。 */
    void appendCandidatesForPrefix(const std::string &prefix, CandidatePayload &out) const;
    const std::string &strInputCode() const;
    bool hasExactCode(const std::string &code) const;
    /** 主码表是否含 code=text 条目（精确匹配）。 */
    bool hasEntry(const std::string &code, const std::string &text) const;

    /** 单字码表（code → 若干 hz）；供五笔引擎构建单字→首选码索引等。 */
    const std::unordered_map<std::string, std::vector<std::string>> &singleCharLexicon() const
    {
        return singleChardict_;
    }

    /** 词组码表（code → 若干 hz）；与 singleCharLexicon 对称，供引擎精确编码查找。 */
    const std::unordered_map<std::string, std::vector<std::string>> &multiCharLexicon() const
    {
        return multiChardict_;
    }

    const std::vector<EngineRuleBlock> &phraseEncodeRules() const
    {
        return rules_;
    }

    uint32_t codeLength() const
    {
        return static_cast<uint32_t>(iCodeLength_);
    }

    const std::string &tableName() const
    {
        return tableName_;
    }

    const std::string &tableInfo() const
    {
        return tableInfo_;
    }

    const std::string &tableCreateTime() const
    {
        return tableCreateTime_;
    }

    const std::string &endKeys() const
    {
        return strEndKeys_;
    }

    const std::string &specialKeys() const
    {
        return strSpecialKeys_;
    }

    const std::string &codeType() const
    {
        return strCodeType_;
    }

    const std::string &straightUpKeys() const
    {
        return strStraightUPKeys_;
    }

    uint8_t wildChar() const
    {
        return cWildChar_;
    }

    uint8_t hasRule() const
    {
        return bRule_;
    }

    const std::vector<std::pair<std::string, std::string>> &records() const
    {
        return records_;
    }

private:
    bool readNulTerminatedField(std::ifstream &in, std::string &out);
    bool readU32(std::ifstream &in, uint32_t &out);
    bool readExact(std::ifstream &in, void *dst, std::streamsize len);
    bool writeNulTerminatedField(std::ofstream &out, const std::string &value) const;
    bool writeU32(std::ofstream &out, uint32_t value) const;
    bool writeExact(std::ofstream &out, const void *src, std::streamsize len) const;
    void rebuildLexiconFromRecord(const std::string &code, const std::string &text);

private:
    std::unordered_map<std::string, std::vector<std::string>> singleChardict_;
    std::unordered_map<std::string, std::vector<std::string>> multiChardict_;

    std::string tableName_;
    std::string tableInfo_;
    std::string tableCreateTime_;
    std::string strEndKeys_;
    std::string strSpecialKeys_;
    std::string strCodeType_;
    std::string strStraightUPKeys_;
    std::string strInputCode_;
    uint8_t cWildChar_ = 0;
    uint8_t bRule_ = 0;
    uint32_t iCodeLength_ = 4;
    std::vector<EngineRuleBlock> rules_;
    std::vector<std::pair<std::string, std::string>> records_;
    uint32_t recordCount_ = 0;

    /** 单次前缀查询最多物化的候选条数（软上限）。 */
    static const std::size_t kMaxCandidatesPerQuery = 1000;
};

} // namespace freewb

#endif // COMMON_H
