#ifndef FREEWB_SETTINGS_H
#define FREEWB_SETTINGS_H

/* Xlib 等头文件常定义宏 Data，会与 SimpleIni.h 中的成员名 Data() 冲突 */
#ifdef Data
#pragma push_macro("Data")
#undef Data
#define FREEWB_RESTORE_X11_DATA_MACRO
#endif

#include "SimpleIni.h"

#ifdef FREEWB_RESTORE_X11_DATA_MACRO
#pragma pop_macro("Data")
#undef FREEWB_RESTORE_X11_DATA_MACRO
#endif

#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace settings
{

extern const char kDefaultCoustomChar[];
extern const char kDefaultCoustomMark[];

enum class ValueType
{
    Bool,
    Int,
    String,
};

#define FREEWB_CFG_Int_APPLY(id, GetFn, SetFn)                                                                                   \
    int GetFn() const                                                                                                            \
    {                                                                                                                            \
        return readInt(#id);                                                                                                     \
    }                                                                                                                            \
    void SetFn(int v)                                                                                                            \
    {                                                                                                                            \
        writeInt(#id, v);                                                                                                        \
    }

#define FREEWB_CFG_Bool_APPLY(id, GetFn, SetFn)                                                                                  \
    bool GetFn() const                                                                                                           \
    {                                                                                                                            \
        return readBool(#id);                                                                                                    \
    }                                                                                                                            \
    void SetFn(bool v)                                                                                                           \
    {                                                                                                                            \
        writeBool(#id, v);                                                                                                       \
    }

#define FREEWB_CFG_String_APPLY(id, GetFn, SetFn)                                                                                \
    const std::string &GetFn() const                                                                                             \
    {                                                                                                                            \
        return readString(#id);                                                                                                  \
    }                                                                                                                            \
    void SetFn(const std::string &v)                                                                                             \
    {                                                                                                                            \
        writeString(#id, v);                                                                                                     \
    }

#define FREEWB_AUTO_ACCESSOR_ENTRY_LIST(X)                                                                                       \
    X(charSet, Misc, Int, "0", Misc, "")                                                                                         \
    X(enterCommitPreedit, Common, Bool, "true", Common, "")                                                                      \
    X(inputMode, Misc, String, "engine:wbpy", Misc, "")                                                                          \
    X(simpTradFlg, Misc, Bool, "false", Misc, "")                                                                                \
    X(fullWidthFlg, Misc, Bool, "false", Misc, "")                                                                               \
    X(vkMode, Misc, Int, "-1", Misc, "")                                                                                         \
    X(separateChar, CandidateWinUi, String, ".", CandidateWinUi, "")                                                             \
    X(cnEnSwitch, ShortcutKey, String, "KEY_LEFT_SHIFT", ShortcutKey, "")                                                        \
    X(curUsedLexicon, Misc, String, "default", Misc, "")                                                                         \
    X(CoustomChar, Misc, String, kDefaultCoustomChar, Misc, "")                                                                  \
    X(CoustomMark, Misc, String, kDefaultCoustomMark, Misc, "")                                                                  \
    X(wubiTable, Misc, String, "default/wbzx.mb", Misc, "")                                                                      \
    X(pinyinTable, Misc, String, "default/pinyin.mb", Misc, "")                                                                  \
    X(pinyinFuzzy, Common, Bool, "true", Common, "Pinyin fuzzy correction: z/zh, c/ch, s/sh, an/ang, en/eng, in/ing")            \
    X(candiWinDispMode, CandidateWinUi, Int, "0", CandidateWinUi, "")                                                            \
    X(candiCharCount, CandidateWinUi, Int, "9", CandidateWinUi, "")                                                              \
    X(candiWordCount, CandidateWinUi, Int, "5", CandidateWinUi, "")                                                              \
    X(radius, CandidateWinUi, Int, "5", CandidateWinUi, "")                                                                      \
    X(transparency, CandidateWinUi, Int, "0", CandidateWinUi, "")                                                                \
    X(candiTextFontName, CandidateWinUi, String, "Noto Sans CJK SC", CandidateWinUi, "")                                         \
    X(candiTextFontSize, CandidateWinUi, Int, "14", CandidateWinUi, "")                                                          \
    X(bgColor, CandidateWinUi, String, "#ffffffff", CandidateWinUi, "")                                                          \
    X(bgImage, CandidateWinUi, String, "", CandidateWinUi, "")                                                                   \
    X(borderColor, CandidateWinUi, String, "#ffa4c1e2", CandidateWinUi, "")                                                      \
    X(gradientColor0, CandidateWinUi, String, "#ffffffff", CandidateWinUi, "")                                                   \
    X(gradientColor1, CandidateWinUi, String, "#ffffffff", CandidateWinUi, "")                                                   \
    X(candiWordTextColor, CandidateWinUi, String, "#ff5f5b52", CandidateWinUi, "")                                               \
    X(candiPromptTextColor, CandidateWinUi, String, "#ff5f5b52", CandidateWinUi, "")                                             \
    X(enableTiled, CandidateWinUi, Bool, "false", CandidateWinUi, "")                                                            \
    X(useGradientColor, CandidateWinUi, Bool, "false", CandidateWinUi, "")                                                       \
    X(useBgImage, CandidateWinUi, Bool, "false", CandidateWinUi, "")                                                             \
    X(showCandDictInfo, CandidateWinUi, Bool, "false", CandidateWinUi, "")                                                       \
    X(cursorFollow, CandidateWinOptions, Bool, "true", CandidateWinOptions, "")                                                  \
    X(showOpRemindInfo, CandidateWinOptions, Bool, "true", CandidateWinOptions, "")                                              \
    X(shiftSelectRecode, CandidateWinOptions, Bool, "false", CandidateWinOptions, "")                                            \
    X(secondRecodeKey, CandidateWinOptions, String, "KEY_SEMICOLON", CandidateWinOptions, "")                                    \
    X(thirdRecodeKey, CandidateWinOptions, String, "KEY_QUOTE", CandidateWinOptions, "")                                         \
    X(prevPageKey, CandidateWinOptions, String, "KEY_DASH", CandidateWinOptions, "")                                             \
    X(nextPageKey, CandidateWinOptions, String, "KEY_EQUAL", CandidateWinOptions, "")                                            \
    X(codeRemind, Common, Bool, "true", Common, "")                                                                              \
    X(spaceFullWhenCharHalf, Common, Bool, "false", Common, "")                                                                  \
    X(wordThink, Common, Bool, "false", Common, "")                                                                              \
    X(puncAutoPair, Punctuation, Bool, "true", Punctuation, "Automatic punctuation pairing")                                     \
    X(chinesePunc, Punctuation, Bool, "true", Punctuation, "Chinese punctuation")                                                \
    X(autoToHalfPuncAfterNumber, Punctuation, Bool, "false", Punctuation, "Auto convert half punctuation after number")          \
    X(remindExistWord, Common, Bool, "false", Common, "")                                                                        \
    X(alertWhenEmptyCode, Common, Bool, "false", Common, "")                                                                     \
    X(autoAdjustFreq, Common, Bool, "false", Common, "")                                                                         \
    X(zzSpecialEncodingSymbols, Advanced, Bool, "true", Advanced,                                                                \
      "Symbols corresponding to zz encoding, Only applicable to the Wubi input method code table.")                              \
    X(shiftCommitChar, Advanced, Bool, "true", Advanced, "")                                                                     \
    X(inputStatistic, Advanced, Bool, "false", Advanced, "")                                                                     \
    X(typeEffect, Advanced, Bool, "false", Advanced, "")                                                                         \
    X(recodeCalib, Advanced, Bool, "false", Advanced, "")                                                                        \
    X(autoPhrase, Advanced, Bool, "true", Advanced, "")                                                                          \
    X(autoToEnStr, Others, String, "www. ftp: http mail. bbs.", Others, "")                                                      \
    X(curSkinId, Ui, String, "default", Ui, "")                                                                                  \
    X(toolbarAutoLocate, Ui, Bool, "true", Ui, "")                                                                               \
    X(toolbarAutoExpand, Ui, Bool, "false", Ui, "")                                                                              \
    X(uiAudioEffect, Ui, Bool, "false", Ui, "")                                                                                  \
    X(useFreewbFont, CandidateWinOptions, Bool, "false", CandidateWinOptions, "")                                                \
    X(showRealtimeHelp, Ui, Bool, "true", Ui, "")                                                                                \
    X(hideToolbar, Ui, Bool, "false", Ui, "")                                                                                    \
    X(toolbarTransparency, Ui, Int, "0", Ui, "")                                                                                 \
    X(disableFullHalfSwitch, ShortcutKey, Bool, "false", ShortcutKey, "")                                                        \
    X(disableAllShortcutKey, ShortcutKey, Bool, "false", ShortcutKey, "")                                                        \
    X(addCharAfterOutput, ShortcutKey, String, "CTRL+KEY_NONE", ShortcutKey, "")                                                 \
    X(backFindCode, ShortcutKey, String, "CTRL+KEY_SLASH", ShortcutKey, "")                                                      \
    X(markAutoPair, ShortcutKey, String, "CTRL+KEY_DEL", ShortcutKey, "")                                                        \
    X(onlineAddWord, ShortcutKey, String, "CTRL+KEY_EQUAL", ShortcutKey, "")                                                     \
    X(onlineDelWord, ShortcutKey, String, "CTRL+KEY_DASH", ShortcutKey, "")                                                      \
    X(quickDelScreenItem, ShortcutKey, String, "CTRL+KEY_BACKSPACE", ShortcutKey, "")                                            \
    X(setupOption, ShortcutKey, String, "CTRL+KEY_COMMA", ShortcutKey, "")                                                       \
    X(shortcutInput, ShortcutKey, String, "KEY_QUOTE", ShortcutKey, "")                                                          \
    X(showHideToolbar, ShortcutKey, String, "CTRL+KEY_LEFT", ShortcutKey, "")                                                    \
    X(switchCharSet, ShortcutKey, String, "CTRL+KEY_M", ShortcutKey, "")                                                         \
    X(switchChttrans, ShortcutKey, String, "CTRL+KEY_J", ShortcutKey, "")                                                        \
    X(switchInputMode, ShortcutKey, String, "CTRL+KEY_BACK_SLASH", ShortcutKey, "")                                              \
    X(switchWordState, ShortcutKey, String, "CTRL+KEY_INSERT", ShortcutKey, "")                                                  \
    X(switchLexicon, ShortcutKey, String, "CTRL+KEY_QUOTE", ShortcutKey, "")                                                     \
    X(switchVKb, ShortcutKey, String, "CTRL+KEY_ESC", ShortcutKey, "")                                                           \
    X(tempEnglish, ShortcutKey, String, "KEY_SEMICOLON", ShortcutKey, "")                                                        \
    X(tempPinyin, ShortcutKey, String, "KEY_BACKQUOTE", ShortcutKey, "")                                                         \
    X(WbzxEngine, Engine, Bool, "true", Engine, "")                                                                              \
    X(WbpyEngine, Engine, Bool, "true", Engine, "")                                                                              \
    X(PyEngine, Engine, Bool, "true", Engine, "")

#define FREEWB_CFG_AUTO_FROM_ENTRY(id, cat, vtype, def, sect, desc) FREEWB_CFG_##vtype##_APPLY(id, get_##id, set_##id)

/**
 * 单条配置的元数据（编译期常量，由 FREEWB_AUTO_ACCESSOR_ENTRY_LIST 单一数据源生成）。
 * 供设置 GUI 枚举、分组、显示默认值与说明；不要在这里手写第二份清单。
 */
struct ConfigMeta
{
    const char *key;           // 唯一键名，与 ini 键一致
    ValueType type;            // Bool / Int / String
    const char *category;      // 逻辑分组（Misc / Common / ...）
    const char *section;       // ini 段名
    const char *defaultValue;  // 默认值字面量
    const char *description;   // 说明（可能为空）
};

inline const ConfigMeta *allConfigMeta(size_t *count = nullptr)
{
    static const ConfigMeta kMetas[] = {
#define FREEWB_META(id, cat, vtype, def, sect, desc) {#id, ValueType::vtype, #cat, #sect, def, desc},
        FREEWB_AUTO_ACCESSOR_ENTRY_LIST(FREEWB_META)
#undef FREEWB_META
    };
    if (count) *count = sizeof(kMetas) / sizeof(kMetas[0]);
    return kMetas;
}

/**
 * 已接线 = 会话层 / TSF 前端 / 候选窗真的会读取的配置键。
 *
 * 这是「实现进度」而非配置定义，无法从代码自动推导，新增接线时必须同步更新这一份。
 * 设置 GUI 与文档共用它：未接线的项会标灰并注明「暂未生效」，避免用户改了没反应。
 */
inline bool configIsWired(const char *key)
{
    static const char *const kWired[] = {
        // 会话 / 引擎层原本就在读的
        "charSet", "inputMode", "simpTradFlg", "fullWidthFlg", "chinesePunc",
        "puncAutoPair", "pinyinFuzzy", "candiWordCount", "wubiTable", "pinyinTable",
        "secondRecodeKey", "thirdRecodeKey", "WbzxEngine", "WbpyEngine", "PyEngine",
        "autoPhrase", "typeEffect", "uiAudioEffect", "zzSpecialEncodingSymbols",
        // 设置界面接线时新增（回车 / 翻页 / 候选窗外观）
        "enterCommitPreedit", "prevPageKey", "nextPageKey",
        "candiTextFontName", "candiTextFontSize", "bgColor", "borderColor",
        "candiWordTextColor", "candiPromptTextColor", "radius", "transparency",
    };
    for (const char *k : kWired)
    {
        if (std::strcmp(k, key) == 0) return true;
    }
    return false;
}

/**
 * 单条配置：元数据（字面量指针）+ 当前值。
 * map 的键与 uniquename 一致；构造时由 FREEWB_AUTO_ACCESSOR_ENTRY_LIST 填入。
 */
struct ConfigEntry
{
    struct ValueSlots
    {
        bool boolValue;
        int intValue;
        std::string stringValue;
    };

    const char *uniquename;
    ValueType type;
    ValueSlots defaultValue;
    const char *section;
    ValueSlots value;
};

/**
 * 配置对象（主类型）：读写 $HOME/.local/freewb/config/config.ini；
 * 构造时以代码默认值填充 entries_，用户 ini 存在则覆盖 value；保存时若目录或文件不存在则创建。
 */
class Settings
{
public:
    Settings();

    FREEWB_AUTO_ACCESSOR_ENTRY_LIST(FREEWB_CFG_AUTO_FROM_ENTRY)
    bool restore_default_shortcutkey();

    void restoreAllDefaults();

    bool save();
    void reload();

    // 通用键值访问（供迁移器等按运行时键名读写）
    bool setByKey(const std::string &uniquename, const std::string &value);
    std::string getByKey(const std::string &uniquename) const;

private:
    void load();
    static ConfigEntry::ValueSlots makeValueSlots(ValueType type, const char *text);

    void appendEntriesFromDef()
    {
        entries_.clear();
        entries_.reserve(96);
#define FREEWB_CONFIG(id, cat, vtype, def, sect, desc)                                                                           \
    entries_.emplace(#id, ConfigEntry{#id, ValueType::vtype, makeValueSlots(ValueType::vtype, def), #sect,                       \
                                      makeValueSlots(ValueType::vtype, def)});
        FREEWB_AUTO_ACCESSOR_ENTRY_LIST(FREEWB_CONFIG)
#undef FREEWB_CONFIG
    }

    ConfigEntry *findEntryByName(const std::string &uniquename);
    void persistEntry(const ConfigEntry &entry);
    bool readBool(const std::string &uniquename) const;
    int readInt(const std::string &uniquename) const;
    const std::string &readString(const std::string &uniquename) const;
    void writeBool(const std::string &uniquename, bool value);
    void writeInt(const std::string &uniquename, int value);
    void writeString(const std::string &uniquename, const std::string &value);

    static void applyOneEntryToIni(CSimpleIniA &ini, const ConfigEntry &entry);
    static bool ensureDir(const std::string &dir);

    std::string ini_path_;
    std::unordered_map<std::string, ConfigEntry> entries_;
};

Settings &instance();

} // namespace settings

#undef FREEWB_CFG_Int_APPLY
#undef FREEWB_CFG_Bool_APPLY
#undef FREEWB_CFG_String_APPLY
#undef FREEWB_CFG_AUTO_FROM_ENTRY
#undef FREEWB_AUTO_ACCESSOR_ENTRY_LIST

#endif
