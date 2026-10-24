#ifndef FREQORDER_H
#define FREQORDER_H

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include "types.h"

namespace freewb
{

/** 同码词频调序：每码一条词频列表；五笔/拼音分文件。 */
class FreqOrder
{
public:
    static constexpr std::size_t kMaxPrefsPerCode = 16;

    /** @p fileName 如 freq_order_wb.txt，落在 userFreewbPath()/data/ 下。 */
    explicit FreqOrder(const std::string &fileName);

    void reload();
    void adjustFreq(const std::string &code, const std::string &text);
    /**
     * 按 @p preedit 对应的词频偏好，对整份候选稳定重排（对齐旧版按当前输入码调序）。
     * 无该码偏好或候选不足 2 个时不动。
     */
    void applyToCandidates(CandidatePayload &payload, const std::string &preedit) const;

private:
    bool save() const;

    std::string filePath_;
    std::unordered_map<std::string, std::vector<std::string>> entries_;
};

} // namespace freewb

#endif
