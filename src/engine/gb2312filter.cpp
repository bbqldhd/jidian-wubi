#include "gb2312filter.h"

#include <cerrno>
#include <cstring>

#include "common.h"
#include "log.h"
#include "settings.h"

namespace freewb
{

Gb2312Filter::Gb2312Filter() : available_(settings::instance().get_charSet() == 0)
{
    conv_ = iconv_open("GB2312", "UTF-8");
    if (conv_ == reinterpret_cast<iconv_t>(-1))
    {
        FREEWB_WARN("Gb2312Filter: iconv_open(GB2312, UTF-8) failed: {}", std::strerror(errno));
    }
}

Gb2312Filter::~Gb2312Filter()
{
    if (conv_ != reinterpret_cast<iconv_t>(-1))
    {
        iconv_close(conv_);
    }
}

const char *Gb2312Filter::name() const
{
    return "charset:gb2312filter";
}

bool Gb2312Filter::available() const
{
    return available_;
}

void Gb2312Filter::changeAvailable()
{
    available_ = !available_;
}

bool Gb2312Filter::needFilt(const std::string &hz) const
{
    if (hz.empty())
    {
        return true;
    }
    if (!available_ || MbDictionaryTable::utf8CharCount(hz) != 1U)
    {
        return false;
    }
    return !isGb2312(hz);
}

bool Gb2312Filter::valid() const
{
    return conv_ != reinterpret_cast<iconv_t>(-1);
}

bool Gb2312Filter::isGb2312(const std::string &utf8Char) const
{
    if (!valid() || utf8Char.empty())
    {
        return true;
    }

    const auto cached = gb2312Cache_.find(utf8Char);
    if (cached != gb2312Cache_.end())
    {
        return cached->second;
    }

    iconv(conv_, nullptr, nullptr, nullptr, nullptr);

    char *inbuf = const_cast<char *>(utf8Char.data());
    std::size_t inbytes = utf8Char.size();
    unsigned char outbuf[8];
    char *outptr = reinterpret_cast<char *>(outbuf);
    std::size_t outbytes = sizeof(outbuf);

    const std::size_t rc = iconv(conv_, &inbuf, &inbytes, &outptr, &outbytes);
    const bool ok = rc != static_cast<std::size_t>(-1) && inbytes == 0;
    gb2312Cache_.emplace(utf8Char, ok);
    return ok;
}

} // namespace freewb
