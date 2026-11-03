#include "autophrase.h"

#include <ctime>

#include "log.h"
#include "settings.h"
#include "utils.h"
#include "wbzx.h"

namespace freewb
{

AutoPhrase::AutoPhrase(WbzxEngine &owner) : owner_(owner)
{
}

AutoPhrase::~AutoPhrase() = default;

bool AutoPhrase::enabled() const
{
    return settings::instance().get_autoPhrase();
}

void AutoPhrase::reloadDictionary()
{
    if (!enabled())
    {
        table_.clear();
        FREEWB_DEBUG("AutoPhrase::reloadDictionary: disabled, table cleared");
        return;
    }

    table_.clear();

    const std::string path = userFreewbPath() + "/data/autophrase.mb";
    if (path.empty() || !table_.loadFromFile(path, "autophrase "))
    {
        table_.clear();
        initTableMetadataIfEmpty();
        FREEWB_DEBUG("AutoPhrase::reloadDictionary: no file or load failed path={}", path);
        return;
    }

    if (table_.tableName().empty() || table_.strInputCode().empty())
    {
        initTableMetadataIfEmpty();
    }

    FREEWB_DEBUG("AutoPhrase::reloadDictionary: loaded {} records", table_.records().size());
}

void AutoPhrase::initTableMetadataIfEmpty()
{
    if (!table_.tableName().empty() && !table_.strInputCode().empty())
    {
        return;
    }

    const MbDictionaryTable &main = owner_.mainDictionaryTable();
    table_.setMetadata("自动词组", "freewb 自动词组", formatTableCreateTime(), main.endKeys(), main.specialKeys(),
                       main.codeType(), main.straightUpKeys(), main.strInputCode(), main.wildChar(), main.hasRule(),
                       main.phraseEncodeRules());
}

std::string AutoPhrase::formatTableCreateTime()
{
    const std::time_t nowSec = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &nowSec);
#else
    localtime_r(&nowSec, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tm);
    return buf;
}

void AutoPhrase::saveDictionary()
{
    if (!enabled() || table_.records().empty())
    {
        return;
    }

    initTableMetadataIfEmpty();

    const std::string path = userFreewbPath() + "/data/autophrase.mb";
    if (path.empty() || table_.tableName().empty() || table_.strInputCode().empty())
    {
        FREEWB_ERROR("AutoPhrase::saveDictionary: cannot save path={}", path);
        return;
    }

    if (!table_.saveToFile(path))
    {
        FREEWB_ERROR("AutoPhrase::saveDictionary: save failed path={}", path);
        return;
    }

    FREEWB_DEBUG("AutoPhrase::saveDictionary: saved {} records to {}", table_.records().size(), path);
}

void AutoPhrase::add(const std::string &committedText, const std::string &code)
{
    if (!enabled())
    {
        return;
    }

    if (MbDictionaryTable::utf8CharCount(committedText) != 1U && !code.empty())
    {
        for (const AutoPhraseEntry &entry : phrases_)
        {
            if (entry.code != code || entry.hz != committedText || table_.hasEntry(code, committedText))
            {
                continue;
            }
            if (!table_.insertRecord(code, committedText))
            {
                FREEWB_WARN("AutoPhrase::add: insert failed code={} text={}", code, committedText);
                break;
            }
            saveDictionary();
            FREEWB_DEBUG("AutoPhrase::add: persisted session phrase code={} text={}", code, committedText);
            break;
        }
    }

    addSingleChar(committedText, code);
}

void AutoPhrase::addSingleChar(const std::string &committedText, const std::string &code)
{
    const bool singleChar = MbDictionaryTable::utf8CharCount(committedText) == 1U;
    if (!singleChar)
    {
        recordIndex_ = 0;
        ring_.fill(std::string{});
    }
    if (!enabled() || !singleChar)
    {
        return;
    }

    ring_[static_cast<std::size_t>(recordIndex_ % 4ULL)] = committedText;

    if (recordIndex_ == 0U)
    {
        ++recordIndex_;
        FREEWB_DEBUG("AutoPhrase::addSingleChar text={} code={} recordIndex={}", committedText, code, recordIndex_);
        return;
    }

    const std::uint64_t commitCount = recordIndex_;
    const std::uint64_t spanStartMin = (commitCount < 3U) ? 0U : (commitCount - 3U);
    for (std::uint64_t spanStart = spanStartMin; spanStart < commitCount; ++spanStart)
    {
        std::string phraseHz;
        phraseHz.reserve(32U);
        for (std::uint64_t j = spanStart; j <= commitCount; ++j)
        {
            phraseHz += ring_[static_cast<std::size_t>(j % 4ULL)];
        }

        if (phraseHzSet_.count(phraseHz) != 0U || hzKnownInTables(phraseHz))
        {
            continue;
        }

        const std::string phraseCode = owner_.calculateWubiPhraseCode(phraseHz);
        if (phraseCode.empty() || phraseIndexByCode_.count(phraseCode) != 0U)
        {
            continue;
        }

        const std::uint32_t idx = static_cast<std::uint32_t>(phrases_.size());
        phrases_.push_back(AutoPhraseEntry{phraseHz, phraseCode});
        phraseHzSet_.insert(phrases_.back().hz);
        phraseIndexByCode_.emplace(phrases_.back().code, idx);
        FREEWB_DEBUG("AutoPhrase::addSingleChar phrase hz={} code={} sessionSize={}", phrases_.back().hz, phrases_.back().code,
                     phrases_.size());
    }

    ++recordIndex_;
    FREEWB_DEBUG("AutoPhrase::addSingleChar text={} code={} recordIndex={}", committedText, code, recordIndex_);
}

bool AutoPhrase::hasExactCode(const std::string &code) const
{
    return enabled() && !code.empty() && (table_.hasExactCode(code) || phraseIndexByCode_.count(code) != 0U);
}

bool AutoPhrase::hzKnownInTables(const std::string &hz) const
{
    if (hz.empty())
    {
        return false;
    }
    for (const auto &rec : owner_.mainDictionaryTable().records())
    {
        if (rec.second == hz)
        {
            return true;
        }
    }
    for (const auto &rec : table_.records())
    {
        if (rec.second == hz)
        {
            return true;
        }
    }
    return false;
}

bool AutoPhrase::candidatePayloadHasEntry(const CandidatePayload &out, const std::string &code, const std::string &hz)
{
    for (std::size_t i = 0; i < out.texts.size(); ++i)
    {
        if (out.texts[i] == hz && i < out.fullCodes.size() && out.fullCodes[i] == code)
        {
            return true;
        }
    }
    return false;
}

void AutoPhrase::appendCandidatesForExactCode(const std::string &code, CandidatePayload &out) const
{
    if (!enabled() || code.empty())
    {
        return;
    }

    std::size_t begin = 0, end = 0;
    if (table_.codeRange(code, begin, end))
    {
        const auto &recs = table_.records();
        for (std::size_t i = begin; i < end; ++i)
        {
            const std::string &hz = recs[i].second;
            // 与原「只看词组表」一致：单字由候选表自己产出，这里不重复追加
            if (hz.empty() || MbDictionaryTable::utf8CharCount(hz) < 2U ||
                candidatePayloadHasEntry(out, code, hz))
            {
                continue;
            }
            out.texts.push_back(hz);
            out.fullCodes.push_back(code);
        }
    }

    for (const AutoPhraseEntry &entry : phrases_)
    {
        if (entry.code != code || entry.hz.empty() || candidatePayloadHasEntry(out, entry.code, entry.hz))
        {
            continue;
        }
        out.texts.push_back(entry.hz);
        out.fullCodes.push_back(entry.code);
    }
}

} // namespace freewb
