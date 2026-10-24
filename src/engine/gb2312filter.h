#ifndef GB2312FILTER_H
#define GB2312FILTER_H

#include <iconv.h>

#include <cstddef>
#include <string>
#include <unordered_map>

#include "ifreewb.h"

namespace freewb
{

class Gb2312Filter : public IFreewb
{
public:
    Gb2312Filter();
    ~Gb2312Filter() override;

    Gb2312Filter(const Gb2312Filter &) = delete;
    Gb2312Filter &operator=(const Gb2312Filter &) = delete;

    const char *name() const override;
    /** true 表示 GB 模式（启用单字 GB2312 过滤）；false 表示 GBK 模式。 */
    bool available() const override;
    void changeAvailable() override;

    /** 候选是否需被过滤掉；GBK 模式或非单字恒为 false。 */
    bool needFilt(const std::string &hz) const;

    /** iconv 不可用或字符在 GB2312 内时为 true。 */
    bool isGb2312(const std::string &utf8Char) const;

private:
    bool valid() const;

private:
    iconv_t conv_ = reinterpret_cast<iconv_t>(-1);
    bool available_ = true;
    /** 单字 UTF-8 → 是否属于 GB2312；避免短码下反复 iconv。 */
    mutable std::unordered_map<std::string, bool> gb2312Cache_;
};

} // namespace freewb

#endif // GB2312FILTER_H
