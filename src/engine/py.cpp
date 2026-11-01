#include "py.h"

#include <algorithm>
#include <climits>
#include <fstream>
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
    freqOrder_.applyToCandidates(result_, prefix);
    fillCandidatePayloadPrompts(prefix, result_);
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
    const auto pick = [&](const std::unordered_map<std::string, std::vector<std::string>> &lex) {
        const auto it = lex.find(code);
        if (it == lex.end())
        {
            return false;
        }
        for (const std::string &candidate : it->second)
        {
            if (static_cast<int>(MbDictionaryTable::utf8CharCount(candidate)) == syllables)
            {
                text = candidate;
                return true;
            }
        }
        return false;
    };

    if (pick(mbTable_.multiCharLexicon()))
    {
        return true;
    }
    return pick(mbTable_.singleCharLexicon());
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
    const auto &single = mbTable_.singleCharLexicon();
    for (std::size_t i = n; i-- > 0U;)
    {
        const std::size_t maxLen = std::min(kMaxSyllableLen, n - i);
        for (std::size_t len = allowSingleLetter ? 1U : 2U; len <= maxLen; ++len)
        {
            if (single.find(code.substr(i, len)) == single.end())
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
