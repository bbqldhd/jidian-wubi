#ifndef ENGINE_H
#define ENGINE_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "types.h"

namespace freewb
{
struct EngineRuleCell
{
    uint8_t iFlag = 0;
    uint8_t iWhich = 0;
    uint8_t iIndex = 0;
};

struct EngineRuleBlock
{
    uint8_t iFlag = 0;
    uint8_t iWords = 0;
    std::vector<EngineRuleCell> cells;
};

class IFreewbEngine
{
public:
    virtual ~IFreewbEngine() = default;
    virtual void putKey(const char *key) = 0;
    virtual const CandidatePayload &getResult() const = 0;
    virtual void reset() = 0;

    virtual bool shouldProcessKey(const char *key) const = 0;

    /** @p preedit 是否与词典中某条编码键完全一致（整码）；供上屏/拆码策略查询。 */
    virtual bool isExactDictionaryKey(const std::string &preedit) const = 0;

    /** 顶字上屏要求的最短预编辑长度。五笔/五笔拼音满码为 4；拼音、英文无满码为 0。 */
    virtual std::size_t minTopScreenPreeditLength() const = 0;
};

} // namespace freewb

#endif
