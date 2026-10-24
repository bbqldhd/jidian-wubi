#include "key.h"

#include <cstddef>
#include <cstring>

namespace freewb
{

FreewbKeySym toUpperKeySymbol(FreewbKeySym sym)
{
    if (sym >= FreewbKey_a && sym <= FreewbKey_z)
    {
        return static_cast<FreewbKeySym>(sym - FreewbKey_a + FreewbKey_A);
    }
    return sym;
}

FreewbKeyState Key::modifiers(FreewbKeyState state)
{
    unsigned int value = static_cast<unsigned int>(state);
    if ((value & static_cast<unsigned int>(FreewbKeyState_Super2)) != 0U)
    {
        value = (value & ~static_cast<unsigned int>(FreewbKeyState_Super2)) | static_cast<unsigned int>(FreewbKeyState_Super);
    }
    return static_cast<FreewbKeyState>(value & static_cast<unsigned int>(FreewbKeyState_SimpleMask));
}

bool Key::hasNoModifier(FreewbKeyState state)
{
    return modifiers(state) == FreewbKeyState_None;
}

bool Key::hasOnlyShiftModifier(FreewbKeyState state)
{
    const unsigned int mods = static_cast<unsigned int>(modifiers(state));
    return (mods & ~static_cast<unsigned int>(FreewbKeyState_Shift)) == 0U;
}

bool Key::isSameKeySymbol(FreewbKeySym lhs, FreewbKeySym rhs)
{
    return toUpperKeySymbol(lhs) == toUpperKeySymbol(rhs);
}

FreewbKeySym Key::keySymFromUniqueName(const char *uniqueName)
{
    if (!uniqueName)
    {
        return FreewbKey_None;
    }
    const auto n = sizeof(FreewbKeyNameList) / sizeof(FreewbKeyNameList[0]);
    for (std::size_t i = 0; i < n; ++i)
    {
        if (std::strcmp(uniqueName, FreewbKeyNameList[i].uniqueName) == 0)
        {
            return FreewbKeyNameList[i].sym;
        }
    }
    return FreewbKey_None;
}

const char *Key::keySymToName(FreewbKeySym sym)
{
    const auto n = sizeof(FreewbKeyNameList) / sizeof(FreewbKeyNameList[0]);
    for (std::size_t i = 0; i < n; ++i)
    {
        if (FreewbKeyNameList[i].sym == sym)
        {
            return FreewbKeyNameList[i].name;
        }
    }
    return "";
}

const char *Key::keySymToUniqueName(FreewbKeySym sym)
{
    const auto n = sizeof(FreewbKeyNameList) / sizeof(FreewbKeyNameList[0]);
    for (std::size_t i = 0; i < n; ++i)
    {
        if (FreewbKeyNameList[i].sym == sym)
        {
            return FreewbKeyNameList[i].uniqueName;
        }
    }
    return "";
}

bool Key::isModifierKeySym(FreewbKeySym sym)
{
    switch (sym)
    {
    case FreewbKey_Shift_L:
    case FreewbKey_Shift_R:
    case FreewbKey_Control_L:
    case FreewbKey_Control_R:
    case FreewbKey_Meta_L:
    case FreewbKey_Meta_R:
    case FreewbKey_Alt_L:
    case FreewbKey_Alt_R:
    case FreewbKey_Super_L:
    case FreewbKey_Super_R:
    case FreewbKey_Hyper_L:
    case FreewbKey_Hyper_R:
    case FreewbKey_Shift_Lock:
    case FreewbKey_Caps_Lock:
    case FreewbKey_Num_Lock:
    case FreewbKey_Scroll_Lock:
    case FreewbKey_ISO_Level3_Shift:
    case FreewbKey_ISO_Level5_Shift:
    case FreewbKey_ISO_Group_Shift:
        return true;
    default:
        return false;
    }
}

bool Key::isKeyAZ(FreewbKeySym sym, FreewbKeyState state)
{
    return hasNoModifier(state) && sym >= FreewbKey_A && sym <= FreewbKey_Z;
}

bool Key::isKeyaz(FreewbKeySym sym, FreewbKeyState state)
{
    return hasNoModifier(state) && sym >= FreewbKey_a && sym <= FreewbKey_z;
}

bool Key::isKey09(FreewbKeySym sym, FreewbKeyState state)
{
    return hasNoModifier(state) && sym >= FreewbKey_0 && sym <= FreewbKey_9;
}

const char *Key::readKeyString(const char *str)
{
    const char *ctrl = strstr(str, "CTRL+");
    if (ctrl != NULL)
    {
        return ctrl + 5;
    }
    const char *alt = strstr(str, "ALT+");
    if (alt != NULL)
    {
        return alt + 4;
    }
    const char *shift = strstr(str, "SHIFT+");
    if (shift != NULL)
    {
        return shift + 6;
    }
    const char *super = strstr(str, "SUPER+");
    if (super != NULL)
    {
        return super + 6;
    }
    return str;
}

FreewbKeySym Key::shiftedKeySymbol(FreewbKeySym sym)
{
    switch (sym)
    {
    case FreewbKey_0:
        return FreewbKey_parenright;
    case FreewbKey_1:
        return FreewbKey_exclam;
    case FreewbKey_2:
        return FreewbKey_at;
    case FreewbKey_3:
        return FreewbKey_numbersign;
    case FreewbKey_4:
        return FreewbKey_dollar;
    case FreewbKey_5:
        return FreewbKey_percent;
    case FreewbKey_6:
        return FreewbKey_asciicircum;
    case FreewbKey_7:
        return FreewbKey_ampersand;
    case FreewbKey_8:
        return FreewbKey_asterisk;
    case FreewbKey_9:
        return FreewbKey_parenleft;
    case FreewbKey_comma:
        return FreewbKey_less;
    case FreewbKey_slash:
        return FreewbKey_question;
    case FreewbKey_apostrophe:
        return FreewbKey_quotedbl;
    case FreewbKey_semicolon:
        return FreewbKey_colon;
    case FreewbKey_grave:
        return FreewbKey_asciitilde;
    case FreewbKey_minus:
        return FreewbKey_underscore;
    case FreewbKey_equal:
        return FreewbKey_plus;
    case FreewbKey_bracketleft:
        return FreewbKey_braceleft;
    case FreewbKey_bracketright:
        return FreewbKey_braceright;
    case FreewbKey_period:
        return FreewbKey_greater;
    case FreewbKey_backslash:
        return FreewbKey_bar;
    default:
        return FreewbKey_None;
    }
}

FreewbKeySym Key::normalizedKeySymbol(FreewbKeySym sym, FreewbKeyState state)
{
    if (isModifierKeySym(sym))
    {
        return FreewbKey_None;
    }

    const bool isLetterDigit =
        isKey09(sym, FreewbKeyState_None) || isKeyAZ(sym, FreewbKeyState_None) || isKeyaz(sym, FreewbKeyState_None);
    const bool isPrintableAscii = sym >= FreewbKey_space && sym <= static_cast<FreewbKeySym>(0x007e);

    if (hasNoModifier(state))
    {
        return isPrintableAscii ? sym : FreewbKey_None;
    }

    if (!hasOnlyShiftModifier(state))
    {
        return FreewbKey_None;
    }
    if (isLetterDigit || isPrintableAscii)
    {
        return sym;
    }

    return shiftedKeySymbol(sym);
}
} // namespace freewb