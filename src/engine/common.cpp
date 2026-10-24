#include "common.h"

#include <algorithm>
#include <vector>

#include "log.h"

namespace freewb
{
const uint32_t kMaxHzFieldBytes = 7U * 30U;

void MbDictionaryTable::appendCandidatesForPrefix(const std::string &prefix, CandidatePayload &out) const
{
    if (prefix.empty())
    {
        return;
    }

    /* records_ 按编码有序，前缀匹配码是连续区间：一次 lower_bound 后顺序扫描即可。 */
    const auto begin =
        std::lower_bound(records_.begin(), records_.end(), prefix,
                         [](const std::pair<std::string, std::string> &rec, const std::string &p) { return rec.first < p; });
    for (auto it = begin; it != records_.end(); ++it)
    {
        const std::string &code = it->first;
        if (code.size() < prefix.size() || code.compare(0, prefix.size(), prefix) != 0)
        {
            break;
        }
        if (it->second.empty())
        {
            continue;
        }
        out.texts.push_back(it->second);
        out.fullCodes.push_back(code);
        if (out.texts.size() >= kMaxCandidatesPerQuery)
        {
            return;
        }
    }
}

void MbDictionaryTable::clear()
{
    singleChardict_.clear();
    multiChardict_.clear();
    records_.clear();
    tableName_.clear();
    tableInfo_.clear();
    tableCreateTime_.clear();
    strEndKeys_.clear();
    strSpecialKeys_.clear();
    strCodeType_.clear();
    strStraightUPKeys_.clear();
    strInputCode_.clear();
    cWildChar_ = 0;
    bRule_ = 0;
    iCodeLength_ = 4;
    rules_.clear();
    recordCount_ = 0;
}

bool MbDictionaryTable::loadFromStream(std::ifstream &in, const char *linePrefix)
{
    const char *p = linePrefix != nullptr ? linePrefix : "";

    if (!readNulTerminatedField(in, tableName_) || !readNulTerminatedField(in, tableInfo_) ||
        !readNulTerminatedField(in, tableCreateTime_) || !readNulTerminatedField(in, strEndKeys_) ||
        !readNulTerminatedField(in, strSpecialKeys_) || !readNulTerminatedField(in, strCodeType_) ||
        !readNulTerminatedField(in, strStraightUPKeys_))
    {
        clear();
        return false;
    }

    uint32_t inputCodeLen = 0;
    if (!readU32(in, inputCodeLen))
    {
        clear();
        return false;
    }

    {
        std::vector<char> buf(static_cast<size_t>(inputCodeLen) + 1U);
        in.read(buf.data(), static_cast<std::streamsize>(inputCodeLen) + 1);
        if (!in || static_cast<uint32_t>(in.gcount()) != inputCodeLen + 1U)
        {
            clear();
            return false;
        }
        strInputCode_.assign(buf.data());
    }
    if (!readExact(in, &cWildChar_, 1))
    {
        clear();
        return false;
    }
    iCodeLength_ = 4;
    if (!readExact(in, &bRule_, 1))
    {
        clear();
        return false;
    }
    if (bRule_)
    {
        rules_.resize(static_cast<size_t>(iCodeLength_ - 1U));
        for (uint32_t ri = 0; ri < iCodeLength_ - 1U; ++ri)
        {
            if (!readExact(in, &rules_[ri].iFlag, 1) || !readExact(in, &rules_[ri].iWords, 1))
            {
                clear();
                return false;
            }
            rules_[ri].cells.resize(static_cast<size_t>(iCodeLength_));
            for (uint32_t k = 0; k < iCodeLength_; ++k)
            {
                EngineRuleCell &cell = rules_[ri].cells[k];
                if (!readExact(in, &cell.iFlag, 1) || !readExact(in, &cell.iWhich, 1) || !readExact(in, &cell.iIndex, 1))
                {
                    clear();
                    return false;
                }
            }
        }
    }

    if (!readU32(in, recordCount_))
    {
        clear();
        return false;
    }
    singleChardict_.clear();
    multiChardict_.clear();
    for (uint32_t i = 0; i < recordCount_; ++i)
    {
        uint32_t codeFieldLen = 0;
        if (!readU32(in, codeFieldLen))
        {
            clear();
            return false;
        }
        std::vector<char> codeBuf(static_cast<size_t>(codeFieldLen));
        in.read(codeBuf.data(), static_cast<std::streamsize>(codeFieldLen));
        if (!in || static_cast<uint32_t>(in.gcount()) != codeFieldLen)
        {
            clear();
            return false;
        }
        if (codeFieldLen == 0 || codeBuf[static_cast<size_t>(codeFieldLen) - 1U] != '\0')
        {
            clear();
            return false;
        }
        const std::string strCode(codeBuf.data());
        uint32_t hzFieldLen = 0;
        if (!readU32(in, hzFieldLen))
        {
            clear();
            return false;
        }
        if (hzFieldLen > kMaxHzFieldBytes)
        {
            clear();
            return false;
        }
        std::vector<char> hzBuf(static_cast<size_t>(hzFieldLen));
        in.read(hzBuf.data(), static_cast<std::streamsize>(hzFieldLen));
        if (!in || static_cast<uint32_t>(in.gcount()) != hzFieldLen)
        {
            clear();
            return false;
        }
        if (hzFieldLen == 0 || hzBuf[static_cast<size_t>(hzFieldLen) - 1U] != '\0')
        {
            clear();
            return false;
        }

        int8_t typ = 0;
        if (!readExact(in, &typ, 1))
        {
            clear();
            return false;
        }
        (void)typ;
        std::string hz(hzBuf.data());
        records_.emplace_back(strCode, hz);
        rebuildLexiconFromRecord(strCode, hz);
    }

    FREEWB_DEBUG("{}tableName={}\ntableInfo={}\ntableCreateTime={}\nstrEndKeys={}\nstrSpecialKeys={}\nstrCodeType={}"
                 "\nstrStraightUPKeys={}\ninputCodeLen={}\ncWildChar={}\nbRule={}\niCodeLength={}\nrecordCount={}",
                 p, tableName_, tableInfo_, tableCreateTime_, strEndKeys_, strSpecialKeys_, strCodeType_, strStraightUPKeys_,
                 inputCodeLen, cWildChar_, bRule_, iCodeLength_, recordCount_);
    return true;
}

const std::string &MbDictionaryTable::strInputCode() const
{
    return strInputCode_;
}

bool MbDictionaryTable::hasEntry(const std::string &code, const std::string &text) const
{
    if (code.empty() || text.empty())
    {
        return false;
    }

    const auto scan = [&code, &text](const std::unordered_map<std::string, std::vector<std::string>> &dict) -> bool
    {
        const auto it = dict.find(code);
        if (it == dict.end())
        {
            return false;
        }
        for (const std::string &hz : it->second)
        {
            if (hz == text)
            {
                return true;
            }
        }
        return false;
    };

    return scan(singleChardict_) || scan(multiChardict_);
}

bool MbDictionaryTable::hasExactCode(const std::string &code) const
{
    if (code.empty())
    {
        return false;
    }

    const auto hasNonEmpty = [&code](const std::unordered_map<std::string, std::vector<std::string>> &dict) -> bool
    {
        const auto it = dict.find(code);
        if (it == dict.end())
        {
            return false;
        }
        for (const std::string &hz : it->second)
        {
            if (!hz.empty())
            {
                return true;
            }
        }
        return false;
    };

    return hasNonEmpty(singleChardict_) || hasNonEmpty(multiChardict_);
}

bool MbDictionaryTable::saveToStream(std::ofstream &out) const
{
    if (!writeNulTerminatedField(out, tableName_) || !writeNulTerminatedField(out, tableInfo_) ||
        !writeNulTerminatedField(out, tableCreateTime_) || !writeNulTerminatedField(out, strEndKeys_) ||
        !writeNulTerminatedField(out, strSpecialKeys_) || !writeNulTerminatedField(out, strCodeType_) ||
        !writeNulTerminatedField(out, strStraightUPKeys_))
    {
        return false;
    }

    const uint32_t inputCodeLen = static_cast<uint32_t>(strInputCode_.size());
    if (!writeU32(out, inputCodeLen) || !writeExact(out, strInputCode_.data(), static_cast<std::streamsize>(inputCodeLen + 1U)) ||
        !writeExact(out, &cWildChar_, 1) || !writeExact(out, &bRule_, 1))
    {
        return false;
    }

    if (bRule_)
    {
        for (uint32_t ri = 0; ri < iCodeLength_ - 1U; ++ri)
        {
            if (!writeExact(out, &rules_[ri].iFlag, 1) || !writeExact(out, &rules_[ri].iWords, 1))
            {
                return false;
            }
            for (uint32_t k = 0; k < iCodeLength_; ++k)
            {
                const EngineRuleCell &cell = rules_[ri].cells[k];
                if (!writeExact(out, &cell.iFlag, 1) || !writeExact(out, &cell.iWhich, 1) || !writeExact(out, &cell.iIndex, 1))
                {
                    return false;
                }
            }
        }
    }

    const uint32_t total = static_cast<uint32_t>(records_.size());
    if (!writeU32(out, total))
    {
        return false;
    }

    for (const auto &record : records_)
    {
        const std::string &code = record.first;
        const std::string &text = record.second;
        const uint32_t codeFieldLen = static_cast<uint32_t>(code.size() + 1U);
        const uint32_t hzFieldLen = static_cast<uint32_t>(text.size() + 1U);
        const int8_t typ = 0;
        if (!writeU32(out, codeFieldLen) || !writeExact(out, code.data(), static_cast<std::streamsize>(codeFieldLen)) ||
            !writeU32(out, hzFieldLen) || !writeExact(out, text.data(), static_cast<std::streamsize>(hzFieldLen)) ||
            !writeExact(out, &typ, 1))
        {
            return false;
        }
    }
    return true;
}

bool MbDictionaryTable::loadFromFile(const std::string &path, const char *linePrefix)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        return false;
    }
    return loadFromStream(in, linePrefix);
}

bool MbDictionaryTable::saveToFile(const std::string &path) const
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
    {
        return false;
    }
    return saveToStream(out);
}

void MbDictionaryTable::setMetadata(const std::string &tableName, const std::string &tableInfo,
                                    const std::string &tableCreateTime, const std::string &endKeys,
                                    const std::string &specialKeys, const std::string &codeType,
                                    const std::string &straightUpKeys, const std::string &inputCode, uint8_t wildChar,
                                    uint8_t hasRule, const std::vector<EngineRuleBlock> &rules)
{
    tableName_ = tableName;
    tableInfo_ = tableInfo;
    tableCreateTime_ = tableCreateTime;
    strEndKeys_ = endKeys;
    strSpecialKeys_ = specialKeys;
    strCodeType_ = codeType;
    strStraightUPKeys_ = straightUpKeys;
    strInputCode_ = inputCode;
    cWildChar_ = wildChar;
    bRule_ = hasRule;
    rules_ = rules;
    iCodeLength_ = 4;
}

bool MbDictionaryTable::appendSortedRecord(const std::string &code, const std::string &text)
{
    if (code.empty() || text.empty())
    {
        return true;
    }

    if (!records_.empty())
    {
        const auto &last = records_.back();
        if (last.first == code && last.second == text)
        {
            return true;
        }
        if (code < last.first)
        {
            return false;
        }
    }

    records_.emplace_back(code, text);
    rebuildLexiconFromRecord(code, text);
    recordCount_ = static_cast<uint32_t>(records_.size());
    return true;
}

bool MbDictionaryTable::insertRecord(const std::string &code, const std::string &text)
{
    if (code.empty() || text.empty())
    {
        return false;
    }
    if (hasEntry(code, text))
    {
        return true;
    }

    const auto it =
        std::lower_bound(records_.begin(), records_.end(), code,
                         [](const std::pair<std::string, std::string> &rec, const std::string &c) { return rec.first < c; });
    records_.insert(it, {code, text});
    rebuildLexiconFromRecord(code, text);
    recordCount_ = static_cast<uint32_t>(records_.size());
    return true;
}

void MbDictionaryTable::rebuildLexiconFromRecord(const std::string &code, const std::string &text)
{
    const size_t nChar = MbDictionaryTable::utf8CharCount(text);
    if (nChar == 1U)
    {
        singleChardict_[code].push_back(text);
    }
    else if (nChar >= 2U)
    {
        multiChardict_[code].push_back(text);
    }
}

bool MbDictionaryTable::writeNulTerminatedField(std::ofstream &out, const std::string &value) const
{
    const uint32_t n = static_cast<uint32_t>(value.size());
    return writeU32(out, n) && writeExact(out, value.data(), static_cast<std::streamsize>(n + 1U));
}

bool MbDictionaryTable::writeU32(std::ofstream &out, uint32_t value) const
{
    const unsigned char b[4] = {static_cast<unsigned char>(value & 0xFFU), static_cast<unsigned char>((value >> 8U) & 0xFFU),
                                static_cast<unsigned char>((value >> 16U) & 0xFFU),
                                static_cast<unsigned char>((value >> 24U) & 0xFFU)};
    return writeExact(out, b, 4);
}

bool MbDictionaryTable::writeExact(std::ofstream &out, const void *src, std::streamsize len) const
{
    out.write(static_cast<const char *>(src), len);
    return static_cast<bool>(out);
}

bool MbDictionaryTable::readNulTerminatedField(std::ifstream &in, std::string &out)
{
    uint32_t n = 0;
    if (!readU32(in, n))
    {
        return false;
    }
    std::vector<char> buf(static_cast<size_t>(n) + 1U);
    in.read(buf.data(), static_cast<std::streamsize>(n) + 1);
    if (!in || static_cast<uint32_t>(in.gcount()) != n + 1U)
    {
        return false;
    }
    out.assign(buf.data());
    return true;
}

bool MbDictionaryTable::readU32(std::ifstream &in, uint32_t &out)
{
    unsigned char b[4];
    in.read(reinterpret_cast<char *>(b), 4);
    if (!in || in.gcount() != 4)
    {
        return false;
    }
    out = static_cast<uint32_t>(b[0]) | (static_cast<uint32_t>(b[1]) << 8) | (static_cast<uint32_t>(b[2]) << 16) |
          (static_cast<uint32_t>(b[3]) << 24);
    return true;
}

bool MbDictionaryTable::readExact(std::ifstream &in, void *dst, std::streamsize len)
{
    in.read(static_cast<char *>(dst), len);
    return in && in.gcount() == len;
}

std::size_t MbDictionaryTable::utf8CharCount(const std::string &s)
{
    std::size_t n = 0;
    for (std::size_t i = 0; i < s.size();)
    {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 0x80U)
        {
            ++i;
        }
        else if ((c >> 5U) == 6U)
        {
            i += 2;
        }
        else if ((c >> 4U) == 14U)
        {
            i += 3;
        }
        else if ((c >> 3U) == 30U)
        {
            i += 4;
        }
        else
        {
            ++i;
            continue;
        }
        ++n;
    }
    return n;
}

std::string MbDictionaryTable::utf8CharAt(const std::string &s, std::size_t index)
{
    for (std::size_t i = 0, n = 0; i < s.size();)
    {
        const std::size_t start = i;
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 0x80U)
        {
            ++i;
        }
        else if ((c >> 5U) == 6U)
        {
            i += 2;
        }
        else if ((c >> 4U) == 14U)
        {
            i += 3;
        }
        else if ((c >> 3U) == 30U)
        {
            i += 4;
        }
        else
        {
            ++i;
            continue;
        }
        if (n == index)
        {
            return s.substr(start, i - start);
        }
        ++n;
    }
    return {};
}

std::string MbDictionaryTable::utf8CharAtFromEnd(const std::string &s, std::size_t which)
{
    if (which == 0)
    {
        return {};
    }
    const std::size_t len = utf8CharCount(s);
    if (which > len)
    {
        return {};
    }
    return utf8CharAt(s, len - which);
}

} // namespace freewb
