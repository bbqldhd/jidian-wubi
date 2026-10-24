#ifndef VKLAYOUTS_H
#define VKLAYOUTS_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

/** 虚拟键盘按键：SYMBOL_NUM 之前为可自定义符号键，其后为控制键。 */
typedef enum
{
    VK_KEY_DIGIT0 = 0,
    VK_KEY_DIGIT1,
    VK_KEY_DIGIT2,
    VK_KEY_DIGIT3,
    VK_KEY_DIGIT4,
    VK_KEY_DIGIT5,
    VK_KEY_DIGIT6,
    VK_KEY_DIGIT7,
    VK_KEY_DIGIT8,
    VK_KEY_DIGIT9,

    VK_KEY_A,
    VK_KEY_B,
    VK_KEY_C,
    VK_KEY_D,
    VK_KEY_E,
    VK_KEY_F,
    VK_KEY_G,
    VK_KEY_H,
    VK_KEY_I,
    VK_KEY_J,
    VK_KEY_K,
    VK_KEY_L,
    VK_KEY_M,
    VK_KEY_N,
    VK_KEY_O,
    VK_KEY_P,
    VK_KEY_Q,
    VK_KEY_R,
    VK_KEY_S,
    VK_KEY_T,
    VK_KEY_U,
    VK_KEY_V,
    VK_KEY_W,
    VK_KEY_X,
    VK_KEY_Y,
    VK_KEY_Z,

    VK_KEY_BACKQUOTE,
    VK_KEY_SUB,
    VK_KEY_EQUAL,
    VK_KEY_LEFT_BRACKET,
    VK_KEY_RIGHT_BRACKET,
    VK_KEY_BACKSLASH,
    VK_KEY_SEMICOLON,
    VK_KEY_QUOTE,
    VK_KEY_COMMA,
    VK_KEY_PERIOD,
    VK_KEY_SLASH,

    VK_KEY_SYMBOL_NUM,

    VK_KEY_BACKSPACE = VK_KEY_SYMBOL_NUM,
    VK_KEY_TAB,
    VK_KEY_CAPS,
    VK_KEY_ENTER,
    VK_KEY_SHIFT,
    VK_KEY_INSERT,
    VK_KEY_DEL,
    VK_KEY_SPACE,
    VK_KEY_ESC,

    VK_KEY_ALL_NUM
} VkKey;

typedef enum
{
    VK_MODE_CLOSED = -1,
    VK_MODE_PC = 0,
    VK_MODE_GREEK,
    VK_MODE_RUSSIAN,
    VK_MODE_PHONETIC,
    VK_MODE_PINYIN,
    VK_MODE_JAPAN_FLAT,
    VK_MODE_JAPAN_PIECE,
    VK_MODE_PUNCTUATION,
    VK_MODE_DIGITAL_ORDER,
    VK_MODE_MATH,
    VK_MODE_UNIT,
    VK_MODE_TABS,
    VK_MODE_SPECIAL,
    VK_MODE_USER_CHAR,
    VK_MODE_CUSTOM_CHAR,
    VK_MODE_CUSTOM_MARK,
    VK_MODE_COUNT
} VirtualKeyboardMode;

inline constexpr int kVkSymbolKeyCount = VK_KEY_SYMBOL_NUM;
inline constexpr int kVkKeyCount = VK_KEY_ALL_NUM;

namespace freewb
{

/** 一键两档文字（内置盘 / 自定义布局共用）。 */
struct VkKeyPair
{
    std::string normal;
    std::string shift;
};

using VkLayout = std::array<VkKeyPair, static_cast<std::size_t>(kVkSymbolKeyCount)>;

class VkLayouts
{
public:
    /** 画键面：内置盘某键两档文字。越界或未定义为空串。 */
    static const VkKeyPair &getKeyPair(VirtualKeyboardMode mode, VkKey key);
    /** 物理键改写：内置虚拟键盘。对不上布局键则为空串。 */
    static std::string convertToVkText(VirtualKeyboardMode mode, unsigned char ascii);
    /** 物理键改写：自定义符号布局。对不上布局键则为空串。 */
    static std::string convertToCustomSymbol(const VkLayout &layout, unsigned char ascii);
    /** 解析设置里空格分隔的布局串（每键 normal+shift）。 */
    static VkLayout parse(const std::string &text);

private:
    struct AsciiIndex
    {
        int8_t key = -1;
        bool shift = false;
    };

    VkLayouts();
    void initLayouts();
    bool locateAscii(unsigned char ascii, VkKey &key, bool &shift) const;

    VkKeyPair table_[VK_MODE_USER_CHAR][kVkSymbolKeyCount];
    AsciiIndex ascii_[128];

    static const VkLayouts &instance();
};

} // namespace freewb

#endif /* VKLAYOUTS_H */
