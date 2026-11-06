#include "py.h"

#include <algorithm>
#include <climits>
#include <cstring>
#include <fstream>
#include <utility>
#include <vector>

#include "log.h"
#include "settings.h"
#include "utils.h"

namespace freewb
{

namespace
{

// 词库里最长的拼音编码（实测 32 个字母，如 zhonghuarenmingongheguo）
constexpr std::size_t kMaxWordCodeLen = 32;
// 单字编码（音节）最长 8 个字母，如 zhuang / chuang 一类
constexpr std::size_t kMaxSyllableLen = 8;
// 整句切分的输入长度上限：超长输入只做前缀匹配，避免每个按键都跑一次大 DP
constexpr std::size_t kMaxSentenceInputLen = 128;
// 一次最多给几条整句候选
constexpr std::size_t kMaxSentenceVariants = 3;
// 打分：每多切一段 -1。整串的音节总数是固定的，所以「段数最少」等价于「尽量用长词」；
// 同分时取更长的切法（见 DP 里的 >= 比较）。
constexpr int kSegmentPenalty = 1;

/* 模糊音：长式 / 短式。两组字母最容易被打错，也最容易靠规则救回来：
   zhi/zi、chi/ci、shi/si、an/ang、en/eng、in/ing。 */
struct FuzzyPair
{
    const char *longForm;
    const char *shortForm;
};

const FuzzyPair kFuzzyPairs[] = {
    {"zh", "z"}, {"ch", "c"}, {"sh", "s"}, {"ang", "an"}, {"eng", "en"}, {"ing", "in"},
};
// 变体数量上限：每个变体都要查一次词库 + 跑一次整句 DP，必须封顶
constexpr std::size_t kMaxFuzzyVariants = 16;
// 超长输入不做纠错：这时按前缀匹配更符合预期
constexpr std::size_t kMaxFuzzyInputLen = 32;

/* pos 处是不是一段真正的短式。zh 里的 z 不算：那已经是长式的一部分，
   再补一个 h 会得到 zhh。判断办法就是看紧随其后的字母能不能把它补成长式。 */
bool isShortFormAt(const std::string &code, std::size_t pos, const FuzzyPair &pair)
{
    const std::size_t shortLen = std::strlen(pair.shortForm);
    if (pos + shortLen > code.size() || code.compare(pos, shortLen, pair.shortForm) != 0)
    {
        return false;
    }
    const char tail = pair.longForm[shortLen];
    return code.compare(pos + shortLen, 1, std::string(1, tail)) != 0;
}

bool isLongFormAt(const std::string &code, std::size_t pos, const FuzzyPair &pair)
{
    const std::size_t longLen = std::strlen(pair.longForm);
    return pos + longLen <= code.size() && code.compare(pos, longLen, pair.longForm) == 0;
}

} // namespace

PyEngine::PyEngine() : freqOrder_("freq_order_py.txt")
{
    clearMbLoadState();
    // 不在构造时加载 8MB 拼音词库：等真正输入拼音时再加载（见 ensureLoaded）
}

PyEngine::~PyEngine() = default;

const char *PyEngine::name() const
{
    return "engine:py";
}

bool PyEngine::available() const
{
    return available_;
}

void PyEngine::changeAvailable()
{
    available_ = !available_;
}

void PyEngine::fillCandidatePayloadPrompts(const std::string &preedit, CandidatePayload &payload) const
{
    const std::size_t n = payload.texts.size();
    const std::size_t rawLen = preedit.size();
    payload.prompts.assign(n, std::string{});

    for (std::size_t i = 0; i < n; ++i)
    {
        const std::string &commitText = payload.texts[i];
        const std::string &rowCode =
            (i < payload.fullCodes.size() && !payload.fullCodes[i].empty()) ? payload.fullCodes[i] : preedit;
        std::string &out = payload.prompts[i];

        const std::size_t nch = MbDictionaryTable::utf8CharCount(commitText);
        if (nch == 1U && preedit == rowCode && wubiPrimaryCodeLookupCallback_)
        {
            const std::string w = wubiPrimaryCodeLookupCallback_(commitText);
            if (!w.empty())
            {
                out.push_back('[');
                out.append(w);
                out.push_back(']');
            }
        }

        if (rowCode.size() > rawLen)
        {
            out.append(rowCode, rawLen, std::string::npos);
        }
    }
}

void PyEngine::setWubiPrimaryCodeLookupCallback(WubiPrimaryCodeLookupCallback callback)
{
    wubiPrimaryCodeLookupCallback_ = std::move(callback);
}

void PyEngine::ensureLoaded()
{
    if (loaded_)
    {
        return;
    }
    loaded_ = true;
    loadDictionary();
}

void PyEngine::putKey(const char *strCode)
{
    ensureLoaded();
    result_.clearRows();

    if (strCode == nullptr)
    {
        return;
    }

    const std::string prefix(strCode);
    mbTable_.appendCandidatesForPrefix(prefix, result_);
    // 词库里没有这一整串时，按音节切成词再拼成整句（原来这种情况直接 0 候选）
    appendSentenceCandidates(prefix);
    // 打错一个字母时把正确的词/句捞回来（对正确输入零影响，见函数内的提前返回）
    appendFuzzyCandidates(prefix);
    freqOrder_.applyToCandidates(result_, prefix);
    fillCandidatePayloadPrompts(prefix, result_);
}

std::vector<std::string> PyEngine::fuzzyVariants(const std::string &code) const
{
    std::vector<std::string> out;
    if (code.size() < 2U || code.size() > kMaxFuzzyInputLen)
    {
        return out;
    }

    const auto push = [&](std::string variant) {
        if (variant == code || out.size() >= kMaxFuzzyVariants)
        {
            return;
        }
        if (std::find(out.begin(), out.end(), variant) != out.end())
        {
            return;
        }
        out.push_back(std::move(variant));
    };

    for (const FuzzyPair &pair : kFuzzyPairs)
    {
        const std::size_t shortLen = std::strlen(pair.shortForm);
        const std::size_t longLen = std::strlen(pair.longForm);

        // 少打一个字母：zongguo → zhongguo，最典型的错法，优先尝试
        for (std::size_t i = 0; i + shortLen <= code.size(); ++i)
        {
            if (!isShortFormAt(code, i, pair))
            {
                continue;
            }
            std::string variant = code;
            variant.replace(i, shortLen, pair.longForm, longLen);
            push(std::move(variant));
        }
        // 多打一个字母：zhongguo → zongguo
        for (std::size_t i = 0; i + longLen <= code.size(); ++i)
        {
            if (!isLongFormAt(code, i, pair))
            {
                continue;
            }
            std::string variant = code;
            variant.replace(i, longLen, pair.shortForm, shortLen);
            push(std::move(variant));
        }
    }
    return out;
}

void PyEngine::appendFuzzyCandidates(const std::string &prefix)
{
    if (!settings::instance().get_pinyinFuzzy())
    {
        return;
    }

    /* 整串在词库里就有词条，说明用户打的就是对的：纠错一律不插手，避免候选里混进噪声。
       注意不能用 result_ 里的 fullCodes 判断 —— 整句候选的 fullCode 也是用户输入串，
       拿它判断会把「词库没这个词、只是被切成了句子」误当成命中。 */
    if (mbTable_.hasExactCode(prefix))
    {
        return;
    }

    const std::vector<std::string> variants = fuzzyVariants(prefix);
    if (variants.empty())
    {
        return;
    }

    std::vector<std::string> headTexts, headCodes;   // 变体能整串命中的词：最可能是用户真正想打的
    std::vector<std::string> tailTexts, tailCodes;   // 变体的前缀匹配与整句：往后排
    // 整句 DP 比查词贵得多，只给前几个变体留预算，别让每次按键都跑十几遍
    int sentenceBudget = 4;

    const auto seen = [&](const std::string &text) {
        if (std::find(result_.texts.begin(), result_.texts.end(), text) != result_.texts.end())
        {
            return true;
        }
        return std::find(headTexts.begin(), headTexts.end(), text) != headTexts.end() ||
               std::find(tailTexts.begin(), tailTexts.end(), text) != tailTexts.end();
    };

    for (const std::string &variant : variants)
    {
        CandidatePayload rows;
        mbTable_.appendCandidatesForPrefix(variant, rows);
        for (std::size_t i = 0; i < rows.texts.size() && i < rows.fullCodes.size(); ++i)
        {
            if (rows.texts[i].empty() || seen(rows.texts[i]))
            {
                continue;
            }
            if (rows.fullCodes[i] == variant)
            {
                // 顶到最前面的只能是一小撮「整串命中」的词，多了会把正常候选挤到第二页
                if (headTexts.size() < 16U)
                {
                    headTexts.push_back(rows.texts[i]);
                    headCodes.push_back(prefix);
                }
            }
            else
            {
                tailTexts.push_back(rows.texts[i]);
                tailCodes.push_back(prefix);
            }
        }

        // 只有完整词也救不回来时才让整句兜底：多字长串里错一个音节，整句是唯一出路
        if (sentenceBudget > 0 && headTexts.empty())
        {
            --sentenceBudget;
            for (const std::string &sentence : composeSentences(variant))
            {
                if (sentence.empty() || seen(sentence))
                {
                    continue;
                }
                tailTexts.push_back(sentence);
                tailCodes.push_back(prefix);
            }
        }
    }

    if (headTexts.empty() && tailTexts.empty())
    {
        return;
    }

    const std::size_t limit = MbDictionaryTable::maxCandidatesPerQuery();
    if (!headTexts.empty())
    {
        result_.texts.insert(result_.texts.begin(), headTexts.begin(), headTexts.end());
        result_.fullCodes.insert(result_.fullCodes.begin(), headCodes.begin(), headCodes.end());
    }
    for (std::size_t i = 0; i < tailTexts.size() && result_.texts.size() < limit; ++i)
    {
        result_.texts.push_back(tailTexts[i]);
        result_.fullCodes.push_back(tailCodes[i]);
    }
    if (result_.texts.size() > limit)
    {
        result_.texts.resize(limit);
    }
    if (result_.fullCodes.size() > limit)
    {
        result_.fullCodes.resize(limit);
    }
}

bool PyEngine::pickWordForSentence(const std::string &code, std::string &text) const
{
    const int syllables = syllableCount(code);
    if (syllables <= 0)
    {
        return false;
    }
    // 多音节片段里出现单字母音节（n / m 这类叹词）基本都是从中间硬切出来的噪声，
    // 例如 n+de→恩德、n+qi→暖气，要求每个音节至少两个字母可以挡掉绝大多数。
    if (code.size() > 1U && syllableCount(code, false) <= 0)
    {
        return false;
    }

    // 只接受「字数 == 音节数」的词条：词库里的简码/缩写（wo→玩偶、hua→护岸）字数对不上，
    // 会被跳过而不是整串失败，这样才能正常回落到单字表里的「我」。
    // 词组优先，其次单字：与原来的「先查词组表再查单字表」等价。
    const auto pick = [&](bool singleCharOnly) {
        std::size_t begin = 0, end = 0;
        if (!mbTable_.codeRange(code, begin, end))
        {
            return false;
        }
        for (std::size_t i = begin; i < end; ++i)
        {
            const std::string &candidate = mbTable_.records()[i].second;
            const std::size_t n = MbDictionaryTable::utf8CharCount(candidate);
            if (singleCharOnly ? (n != 1U) : (n < 2U))
            {
                continue;
            }
            if (static_cast<int>(n) == syllables)
            {
                text = candidate;
                return true;
            }
        }
        return false;
    };

    if (pick(false))
    {
        return true;
    }
    return pick(true);
}

int PyEngine::syllableCount(const std::string &code, bool allowSingleLetter) const
{
    auto &cache = allowSingleLetter ? syllableCache_ : strictSyllableCache_;
    const auto cached = cache.find(code);
    if (cached != cache.end())
    {
        return cached->second;
    }

    const std::size_t n = code.size();
    std::vector<int> best(n + 1U, INT_MAX);
    best[n] = 0;
    for (std::size_t i = n; i-- > 0U;)
    {
        const std::size_t maxLen = std::min(kMaxSyllableLen, n - i);
        for (std::size_t len = allowSingleLetter ? 1U : 2U; len <= maxLen; ++len)
        {
            if (!mbTable_.hasCode(code.substr(i, len), true))
            {
                continue;
            }
            if (best[i + len] == INT_MAX)
            {
                continue;
            }
            best[i] = std::min(best[i], 1 + best[i + len]);
        }
    }

    const int value = (best[0] == INT_MAX) ? -1 : best[0];
    cache[code] = value;
    return value;
}

std::vector<std::string> PyEngine::composeSentences(const std::string &code) const
{
    std::vector<std::string> out;
    const std::size_t n = code.size();
    if (n < 2U || n > kMaxSentenceInputLen)
    {
        return out;
    }

    /* 从后往前做 DP：best[i] = 把 code[i..] 全部切成词能拿到的最高分。
       每个位置只试 1..kMaxWordCodeLen 个字母，词表是哈希查找，整个 DP 是 O(n*32)。 */
    std::vector<int> best(n + 1U, INT_MIN);
    std::vector<std::size_t> step(n + 1U, 0U);
    std::vector<std::string> wordAt(n + 1U);
    best[n] = 0;

    for (std::size_t i = n; i-- > 0U;)
    {
        const std::size_t maxLen = std::min(kMaxWordCodeLen, n - i);
        for (std::size_t len = 1U; len <= maxLen; ++len)
        {
            const std::string segment = code.substr(i, len);
            std::string word;
            if (!pickWordForSentence(segment, word))
            {
                continue;
            }
            if (best[i + len] == INT_MIN)
            {
                continue;
            }
            const int score = best[i + len] - kSegmentPenalty;
            if (score >= best[i])
            {
                best[i] = score;
                step[i] = len;
                wordAt[i] = word;
            }
        }
    }

    // 整串无法完全切分（含非法拼音串）就不给整句，保持原有行为
    if (best[0] == INT_MIN)
    {
        return out;
    }

    // 首段用不同的词可以得到不同的整句，按分数排序取前几条
    std::vector<std::pair<int, std::size_t>> firsts;
    const std::size_t maxFirst = std::min(kMaxWordCodeLen, n);
    for (std::size_t len = 1U; len <= maxFirst; ++len)
    {
        const std::string segment = code.substr(0U, len);
        std::string word;
        if (!pickWordForSentence(segment, word))
        {
            continue;
        }
        if (best[len] == INT_MIN)
        {
            continue;
        }
        firsts.emplace_back(best[len] - kSegmentPenalty, len);
    }
    std::stable_sort(firsts.begin(), firsts.end(),
                     [](const std::pair<int, std::size_t> &a, const std::pair<int, std::size_t> &b) {
                         return a.first > b.first;
                     });

    for (const auto &first : firsts)
    {
        if (out.size() >= kMaxSentenceVariants)
        {
            break;
        }
        std::string word;
        if (!pickWordForSentence(code.substr(0U, first.second), word))
        {
            continue;
        }
        std::string text = word;
        std::size_t i = first.second;
        bool broken = false;
        while (i < n)
        {
            const std::size_t len = step[i];
            if (len == 0U)
            {
                broken = true;
                break;
            }
            text += wordAt[i];
            i += len;
        }
        if (broken || text.empty())
        {
            continue;
        }
        if (std::find(out.begin(), out.end(), text) != out.end())
        {
            continue;
        }
        out.push_back(text);
    }
    return out;
}

void PyEngine::appendSentenceCandidates(const std::string &prefix)
{
    const std::vector<std::string> sentences = composeSentences(prefix);
    if (sentences.empty())
    {
        return;
    }

    bool hasExact = false;
    for (std::size_t i = 0; i < result_.texts.size() && i < result_.fullCodes.size(); ++i)
    {
        if (result_.fullCodes[i] == prefix)
        {
            hasExact = true;
            break;
        }
    }

    std::vector<std::string> add;
    for (const std::string &text : sentences)
    {
        if (std::find(result_.texts.begin(), result_.texts.end(), text) != result_.texts.end())
        {
            continue;
        }
        if (std::find(add.begin(), add.end(), text) != add.end())
        {
            continue;
        }
        add.push_back(text);
    }
    if (add.empty())
    {
        return;
    }

    // 已有候选时一律排在末尾：前缀匹配（用户在打字过程中看到的词）顺序保持原样，
    // 只有「一个候选都没有」这种原本完全打不出字的情况，才把整句顶到第一位。
    const bool prepend = !hasExact && result_.texts.empty();
    if (prepend)
    {
        result_.texts.insert(result_.texts.begin(), add.begin(), add.end());
        result_.fullCodes.insert(result_.fullCodes.begin(), add.size(), prefix);
    }
    else
    {
        for (const std::string &text : add)
        {
            result_.texts.push_back(text);
            result_.fullCodes.push_back(prefix);
        }
    }

    if (result_.texts.size() > MbDictionaryTable::maxCandidatesPerQuery())
    {
        result_.texts.resize(MbDictionaryTable::maxCandidatesPerQuery());
    }
    if (result_.fullCodes.size() > MbDictionaryTable::maxCandidatesPerQuery())
    {
        result_.fullCodes.resize(MbDictionaryTable::maxCandidatesPerQuery());
    }
}

const CandidatePayload &PyEngine::getResult() const
{
    return result_;
}

void PyEngine::reset()
{
    inputCodes_.clear();
    result_.clearRows();
}

void PyEngine::reloadMainDictionary()
{
    clearMbLoadState();
    loaded_ = true;
    loadDictionary();
}

void PyEngine::adjustFreq(const std::string &code, const std::string &text)
{
    freqOrder_.adjustFreq(code, text);
}

void PyEngine::reloadFreqOrder()
{
    freqOrder_.reload();
}

bool PyEngine::shouldProcessKey(const char *key) const
{
    const_cast<PyEngine *>(this)->ensureLoaded();
    return mbTable_.strInputCode().find(key) != std::string::npos;
}

bool PyEngine::isExactDictionaryKey(const std::string &preedit) const
{
    const_cast<PyEngine *>(this)->ensureLoaded();
    return mbTable_.hasExactCode(preedit);
}

std::size_t PyEngine::minTopScreenPreeditLength() const
{
    return 0;
}

void PyEngine::clearMbLoadState()
{
    mbTable_.clear();
    result_.clearRows();
    syllableCache_.clear();
    strictSyllableCache_.clear();
}

void PyEngine::loadDictionary()
{
    const std::string &relPath = settings::instance().get_pinyinTable();
    if (relPath.empty())
    {
        FREEWB_ERROR("Pinyin table is not set");
        return;
    }

    const std::string path = userFreewbPath() + "/data/mb/" + relPath;
    if (path.empty())
    {
        return;
    }

    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        FREEWB_ERROR("Cannot open pinyin dictionary: {}", path);
        return;
    }

    if (!mbTable_.loadFromStream(in, "py attach "))
    {
        mbTable_.clear();
        result_.clearRows();
    }
}

} // namespace freewb
