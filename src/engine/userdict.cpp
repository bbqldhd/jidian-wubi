#include "userdict.h"

#include <algorithm>
#include <fstream>
#include <sstream>

#include "log.h"
#include "utils.h"

namespace freewb
{

UserDict::UserDict() : filePath_(filePath())
{
    reload();
}

UserDict::~UserDict() = default;

void UserDict::reload()
{
    userEntries_.clear();
    deletedEntries_.clear();
    if (filePath_.empty())
    {
        return;
    }

    parseFile(filePath_);
}

bool UserDict::save() const
{
    if (filePath_.empty())
    {
        FREEWB_ERROR("UserDict::save: empty file path");
        return false;
    }

    std::ofstream out(filePath_, std::ios::binary | std::ios::trunc);
    if (!out)
    {
        FREEWB_ERROR("UserDict::save: open failed path={}", filePath_);
        return false;
    }

    appendSectionEntries(out, Section::UserWord, userEntries_);
    out << '\n';
    appendSectionEntries(out, Section::DeletedWord, deletedEntries_);
    out << '\n';
    return static_cast<bool>(out);
}

bool UserDict::contains(const std::string &code) const
{
    return userEntries_.find(code) != userEntries_.end();
}

bool UserDict::hasUserEntry(const std::string &code, const std::string &text) const
{
    const auto it = userEntries_.find(code);
    if (it == userEntries_.end())
    {
        return false;
    }
    return std::find(it->second.begin(), it->second.end(), text) != it->second.end();
}

bool UserDict::isDeleted(const std::string &code, const std::string &text) const
{
    const auto it = deletedEntries_.find(code);
    if (it == deletedEntries_.end())
    {
        return false;
    }
    return std::find(it->second.begin(), it->second.end(), text) != it->second.end();
}

void UserDict::appendCandidatesForPrefix(const std::string &code, CandidatePayload &out) const
{
    if (code.empty())
    {
        return;
    }

    std::vector<std::string> matchedCodes;
    for (const auto &kv : userEntries_)
    {
        const std::string &entryCode = kv.first;
        if (entryCode.size() >= code.size() && entryCode.compare(0, code.size(), code) == 0)
        {
            matchedCodes.push_back(entryCode);
        }
    }
    std::sort(matchedCodes.begin(), matchedCodes.end());

    for (const std::string &entryCode : matchedCodes)
    {
        const auto it = userEntries_.find(entryCode);
        if (it == userEntries_.end())
        {
            continue;
        }
        for (const std::string &text : it->second)
        {
            if (text.empty())
            {
                continue;
            }
            out.texts.push_back(text);
            out.fullCodes.push_back(entryCode);
        }
    }
}

bool UserDict::addUserEntry(const std::string &code, const std::string &text)
{
    if (code.empty() || text.empty())
    {
        FREEWB_ERROR("UserDict::addUserEntry: empty code or text");
        return false;
    }
    return appendUnique(userEntries_[code], text);
}

bool UserDict::removeUserEntry(const std::string &code, const std::string &text)
{
    const auto it = userEntries_.find(code);
    if (it == userEntries_.end())
    {
        return false;
    }
    if (!removeExact(it->second, text))
    {
        return false;
    }
    if (it->second.empty())
    {
        userEntries_.erase(it);
    }
    return true;
}

bool UserDict::markDeleted(const std::string &code, const std::string &text)
{
    if (code.empty() || text.empty())
    {
        FREEWB_ERROR("UserDict::markDeleted: empty code or text");
        return false;
    }
    return appendUnique(deletedEntries_[code], text);
}

bool UserDict::removeDeletedEntry(const std::string &code, const std::string &text)
{
    const auto it = deletedEntries_.find(code);
    if (it == deletedEntries_.end())
    {
        return false;
    }
    if (!removeExact(it->second, text))
    {
        return false;
    }
    if (it->second.empty())
    {
        deletedEntries_.erase(it);
    }
    return true;
}

std::string UserDict::filePath() const
{
    return userFreewbPath() + "/data/user_word.txt";
}

std::string UserDict::trim(const std::string &s)
{
    std::size_t first = 0;
    while (first < s.size() && (s[first] == ' ' || s[first] == '\t' || s[first] == '\r' || s[first] == '\n'))
    {
        ++first;
    }
    std::size_t last = s.size();
    while (last > first && (s[last - 1] == ' ' || s[last - 1] == '\t' || s[last - 1] == '\r' || s[last - 1] == '\n'))
    {
        --last;
    }
    return s.substr(first, last - first);
}

bool UserDict::appendUnique(std::vector<std::string> &list, const std::string &text)
{
    if (text.empty())
    {
        return false;
    }
    if (std::find(list.begin(), list.end(), text) != list.end())
    {
        return false;
    }
    list.push_back(text);
    return true;
}

bool UserDict::removeExact(std::vector<std::string> &list, const std::string &text)
{
    const auto it = std::find(list.begin(), list.end(), text);
    if (it == list.end())
    {
        return false;
    }
    list.erase(it);
    return true;
}

bool UserDict::parseFile(const std::string &filePath)
{
    std::ifstream in(filePath);
    if (!in)
    {
        return false;
    }

    Section section = Section::UserWord;
    std::string line;
    while (std::getline(in, line))
    {
        const std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#')
        {
            continue;
        }
        if (trimmed[0] == '[')
        {
            if (trimmed == "[UserWord]")
            {
                section = Section::UserWord;
            }
            else if (trimmed == "[DeletedWord]")
            {
                section = Section::DeletedWord;
            }
            continue;
        }

        const std::size_t eq = trimmed.find('=');
        if (eq == std::string::npos || eq == 0)
        {
            continue;
        }

        const std::string code = trim(trimmed.substr(0, eq));
        const std::string text = trim(trimmed.substr(eq + 1));
        if (code.empty() || text.empty())
        {
            continue;
        }

        if (section == Section::DeletedWord)
        {
            appendUnique(deletedEntries_[code], text);
        }
        else
        {
            appendUnique(userEntries_[code], text);
        }
    }
    return true;
}

void UserDict::appendSectionEntries(std::ostream &out, Section section,
                                    const std::unordered_map<std::string, std::vector<std::string>> &entries)
{
    out << (section == Section::UserWord ? "[UserWord]\n" : "[DeletedWord]\n");

    std::vector<std::string> keys;
    keys.reserve(entries.size());
    for (const auto &kv : entries)
    {
        if (!kv.second.empty())
        {
            keys.push_back(kv.first);
        }
    }
    std::sort(keys.begin(), keys.end());

    for (const std::string &code : keys)
    {
        const auto it = entries.find(code);
        if (it == entries.end())
        {
            continue;
        }
        std::vector<std::string> texts = it->second;
        std::sort(texts.begin(), texts.end());
        for (const std::string &text : texts)
        {
            if (!text.empty())
            {
                out << code << '=' << text << '\n';
            }
        }
    }
}

} // namespace freewb
