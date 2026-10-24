#include "freqorder.h"

#include <algorithm>
#include <fstream>

#include "log.h"
#include "utils.h"

namespace freewb
{

FreqOrder::FreqOrder(const std::string &fileName)
{
    filePath_ = userFreewbPath() + "/data/" + fileName;
    reload();
}

void FreqOrder::reload()
{
    entries_.clear();
    if (filePath_.empty())
    {
        return;
    }

    std::ifstream in(filePath_, std::ios::binary);
    if (!in)
    {
        return;
    }

    std::string line;
    while (std::getline(in, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }

        std::size_t first = 0;
        while (first < line.size() && (line[first] == ' ' || line[first] == '\t'))
        {
            ++first;
        }
        std::size_t last = line.size();
        while (last > first && (line[last - 1] == ' ' || line[last - 1] == '\t' || line[last - 1] == '\r' || line[last - 1] == '\n'))
        {
            --last;
        }
        if (first >= last)
        {
            continue;
        }
        const std::string trimmed = line.substr(first, last - first);
        if (trimmed[0] == '#' || trimmed[0] == '[')
        {
            continue;
        }

        const std::size_t eq = trimmed.find('=');
        if (eq == std::string::npos || eq == 0 || eq + 1 >= trimmed.size())
        {
            continue;
        }
        std::string code = trimmed.substr(0, eq);
        std::string text = trimmed.substr(eq + 1);
        while (!code.empty() && (code.back() == ' ' || code.back() == '\t'))
        {
            code.pop_back();
        }
        first = 0;
        while (first < text.size() && (text[first] == ' ' || text[first] == '\t'))
        {
            ++first;
        }
        text.erase(0, first);
        if (code.empty() || text.empty())
        {
            continue;
        }

        std::vector<std::string> &list = entries_[code];
        if (list.size() >= kMaxPrefsPerCode || std::find(list.begin(), list.end(), text) != list.end())
        {
            continue;
        }
        list.push_back(std::move(text));
    }
}

bool FreqOrder::save() const
{
    if (filePath_.empty())
    {
        FREEWB_ERROR("FreqOrder::save: empty file path");
        return false;
    }

    std::ofstream out(filePath_, std::ios::binary | std::ios::trunc);
    if (!out)
    {
        FREEWB_ERROR("FreqOrder::save: open failed path={}", filePath_);
        return false;
    }

    std::vector<std::string> codes;
    codes.reserve(entries_.size());
    for (const auto &kv : entries_)
    {
        if (!kv.second.empty())
        {
            codes.push_back(kv.first);
        }
    }
    std::sort(codes.begin(), codes.end());
    for (const std::string &code : codes)
    {
        for (const std::string &text : entries_.at(code))
        {
            if (!text.empty())
            {
                out << code << '=' << text << '\n';
            }
        }
    }
    return static_cast<bool>(out);
}

void FreqOrder::adjustFreq(const std::string &code, const std::string &text)
{
    if (code.empty() || text.empty())
    {
        return;
    }

    std::vector<std::string> &list = entries_[code];
    if (!list.empty() && list.front() == text)
    {
        return;
    }

    list.erase(std::remove(list.begin(), list.end(), text), list.end());
    list.insert(list.begin(), text);
    if (list.size() > kMaxPrefsPerCode)
    {
        list.resize(kMaxPrefsPerCode);
    }

    if (!save())
    {
        FREEWB_ERROR("FreqOrder::adjustFreq: save failed path={} code={} text={}", filePath_, code, text);
        return;
    }
    FREEWB_DEBUG("FreqOrder::adjustFreq path={} code={} text={}", filePath_, code, text);
}

void FreqOrder::applyToCandidates(CandidatePayload &payload, const std::string &preedit) const
{
    if (preedit.empty() || entries_.empty())
    {
        return;
    }

    const auto prefIt = entries_.find(preedit);
    if (prefIt == entries_.end() || prefIt->second.empty())
    {
        return;
    }

    const std::size_t n = std::min(payload.texts.size(), payload.fullCodes.size());
    if (n < 2)
    {
        return;
    }
    if (payload.prompts.size() < n)
    {
        payload.prompts.resize(n);
    }

    const std::vector<std::string> &pref = prefIt->second;
    std::vector<char> used(n, 0);
    std::vector<std::size_t> order;
    order.reserve(n);
    for (const std::string &want : pref)
    {
        for (std::size_t k = 0; k < n; ++k)
        {
            if (used[k] == 0 && payload.texts[k] == want)
            {
                used[k] = 1;
                order.push_back(k);
            }
        }
    }
    if (order.empty())
    {
        return;
    }
    for (std::size_t k = 0; k < n; ++k)
    {
        if (used[k] == 0)
        {
            order.push_back(k);
        }
    }

    bool unchanged = true;
    for (std::size_t k = 0; k < n; ++k)
    {
        if (order[k] != k)
        {
            unchanged = false;
            break;
        }
    }
    if (unchanged)
    {
        return;
    }

    auto permute = [n, &order](std::vector<std::string> &vec)
    {
        std::vector<std::string> tmp(n);
        for (std::size_t k = 0; k < n; ++k)
        {
            tmp[k] = std::move(vec[order[k]]);
        }
        for (std::size_t k = 0; k < n; ++k)
        {
            vec[k] = std::move(tmp[k]);
        }
    };
    permute(payload.texts);
    permute(payload.prompts);
    permute(payload.fullCodes);
}

} // namespace freewb
