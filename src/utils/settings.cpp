#include "settings.h"

#include <cerrno>
#include <cstdlib>
#include <string>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#endif

#include <sys/stat.h>

#include "utils.h"

namespace settings
{

const char kDefaultCoustomChar[] =
    u8R"cc(ˉ  ，  、  ；  ：  ？  ！  …  —  •  〔  （  【  〈  “  〉  《  》  ∶  「  」  『  ［  ）  ＂  ＇  ‘  ”  〕  々  ‖  】  ’  〗  ～  〖  。  ˇ  ¨  `  |  "  』  .  ］  中  国  )cc";
const char kDefaultCoustomMark[] =
    u8R"cc( ）  ！  ·  #  ￥  %  …  —  *  （                                                     ` ~ － — ＝ + 【 { 】 } 、 ｜ ； ： ’  ，  。  、 ？ )cc";

void Settings::applyOneEntryToIni(CSimpleIniA &ini, const ConfigEntry &e)
{
    switch (e.type)
    {
    case ValueType::Bool:
        ini.SetBoolValue(e.section, e.uniquename, e.value.boolValue, nullptr, true);
        break;
    case ValueType::Int:
        ini.SetLongValue(e.section, e.uniquename, static_cast<long>(e.value.intValue), nullptr, false, true);
        break;
    case ValueType::String:
        ini.SetValue(e.section, e.uniquename, e.value.stringValue.c_str(), nullptr, true);
        break;
    }
}

bool Settings::ensureDir(const std::string &dir)
{
    if (dir.empty())
        return true;

    std::string partial;
    partial.reserve(dir.size());
    for (std::size_t i = 0; i <= dir.size(); ++i)
    {
        if (i == dir.size() || dir[i] == '/' || dir[i] == '\\')
        {
            if (!partial.empty())
            {
#ifdef _WIN32
                // 跳过盘符段（如 "C:"）
                if (partial.size() == 2 && partial[1] == ':')
                {
                    /* skip */
                }
                else if (::_mkdir(partial.c_str()) != 0 && errno != EEXIST)
                    return false;
#else
                if (::mkdir(partial.c_str(), 0755) != 0 && errno != EEXIST)
                    return false;
#endif
            }
        }
        if (i < dir.size())
            partial.push_back(dir[i]);
    }
    return true;
}

ConfigEntry::ValueSlots Settings::makeValueSlots(ValueType type, const char *text)
{
    const std::string s = text ? text : "";
    ConfigEntry::ValueSlots slots = {false, 0, s};
    if (type == ValueType::Bool)
    {
        if (s == "true" || s == "1" || s == "yes" || s == "on")
            slots.boolValue = true;
        else if (s == "false" || s == "0" || s == "no" || s == "off")
            slots.boolValue = false;
        else
            slots.boolValue = !s.empty();
    }
    else if (type == ValueType::Int)
        slots.intValue = std::stoi(s);
    return slots;
}

Settings::Settings()
{
    const std::string userRoot = freewb::userFreewbPath();
    ini_path_ = userRoot.empty() ? ".local/freewb/config.ini" : userRoot + "/config.ini";
    appendEntriesFromDef();
    load();
}

void Settings::load()
{
    CSimpleIniA ini(true, false, false);
    ini.SetQuotes(true);
    const SI_Error e = ini.LoadFile(ini_path_.c_str());
    if (e != SI_OK)
        return;
    for (auto &kv : entries_)
    {
        ConfigEntry &entry = kv.second;
        switch (entry.type)
        {
        case ValueType::Bool:
            entry.value.boolValue = ini.GetBoolValue(entry.section, entry.uniquename, entry.defaultValue.boolValue);
            break;
        case ValueType::Int:
            entry.value.intValue = static_cast<int>(
                ini.GetLongValue(entry.section, entry.uniquename, static_cast<long>(entry.defaultValue.intValue)));
            break;
        case ValueType::String:
        {
            const char *v = ini.GetValue(entry.section, entry.uniquename, entry.defaultValue.stringValue.c_str(), nullptr);
            entry.value.stringValue = v ? std::string(v) : entry.defaultValue.stringValue;
            break;
        }
        }
    }
}

ConfigEntry *Settings::findEntryByName(const std::string &uniquename)
{
    const auto it = entries_.find(uniquename);
    if (it == entries_.end())
        return nullptr;
    return &it->second;
}

void Settings::persistEntry(const ConfigEntry &entry)
{
    struct stat st
    {
    };

    if (::stat(ini_path_.c_str(), &st) != 0)
    {
        const auto pos = ini_path_.rfind('/');
        if (pos == std::string::npos || !ensureDir(ini_path_.substr(0, pos)))
            return;
    }

    CSimpleIniA ini(true, false, false);
    ini.SetQuotes(true);
    const SI_Error err = ini.LoadFile(ini_path_.c_str());
    if (err < 0 && err != SI_FILE)
        return;
    applyOneEntryToIni(ini, entry);
    (void)ini.SaveFile(ini_path_.c_str(), false);
}

bool Settings::readBool(const std::string &uniquename) const
{
    const auto it = entries_.find(uniquename);
    return it == entries_.end() ? false : it->second.value.boolValue;
}

int Settings::readInt(const std::string &uniquename) const
{
    const auto it = entries_.find(uniquename);
    return it == entries_.end() ? 0 : it->second.value.intValue;
}

const std::string &Settings::readString(const std::string &uniquename) const
{
    static const std::string kEmpty;
    const auto it = entries_.find(uniquename);
    return it == entries_.end() ? kEmpty : it->second.value.stringValue;
}

void Settings::writeBool(const std::string &uniquename, bool value)
{
    ConfigEntry *e = findEntryByName(uniquename);
    if (!e)
        return;
    e->value.boolValue = value;
    persistEntry(*e);
}

void Settings::writeInt(const std::string &uniquename, int value)
{
    ConfigEntry *e = findEntryByName(uniquename);
    if (!e)
        return;
    e->value.intValue = value;
    persistEntry(*e);
}

void Settings::writeString(const std::string &uniquename, const std::string &value)
{
    ConfigEntry *e = findEntryByName(uniquename);
    if (!e)
        return;
    e->value.stringValue = value;
    persistEntry(*e);
}

bool Settings::restore_default_shortcutkey()
{
    for (auto &kv : entries_)
    {
        if (std::string(kv.second.section) == "ShortcutKey")
            kv.second.value = kv.second.defaultValue;
    }
    return save();
}

void Settings::restoreAllDefaults()
{
    for (auto &kv : entries_)
        kv.second.value = kv.second.defaultValue;
}

bool Settings::setByKey(const std::string &uniquename, const std::string &value)
{
    /* 通用写入必须按条目类型写进对应的槽位：applyOneEntryToIni() 保存 Bool 时读
       boolValue、保存 Int 时读 intValue，只改 stringValue 等于什么都没存。
       设置 GUI 走的正是这个接口。 */
    ConfigEntry *e = findEntryByName(uniquename);
    if (!e)
        return false;

    switch (e->type)
    {
    case ValueType::Bool:
    {
        const bool b = (value == "true" || value == "1" || value == "yes" || value == "on");
        e->value.boolValue = b;
        e->value.stringValue = b ? "true" : "false";
        break;
    }
    case ValueType::Int:
    {
        int n = 0;
        try
        {
            n = std::stoi(value);
        }
        catch (...)
        {
            return false;   // 非数字：拒绝，避免把错误值写进配置
        }
        e->value.intValue = n;
        e->value.stringValue = std::to_string(n);
        break;
    }
    case ValueType::String:
        e->value.stringValue = value;
        break;
    }
    persistEntry(*e);
    return true;
}

std::string Settings::getByKey(const std::string &uniquename) const
{
    /* load() 对 Bool/Int 只填 boolValue/intValue，stringValue 还是默认值，
       所以这里同样要按类型格式化，GUI 才读得到用户当前值。 */
    const auto it = entries_.find(uniquename);
    if (it == entries_.end())
        return std::string();
    const ConfigEntry &e = it->second;
    switch (e.type)
    {
    case ValueType::Bool: return e.value.boolValue ? "true" : "false";
    case ValueType::Int: return std::to_string(e.value.intValue);
    default: return e.value.stringValue;
    }
}

bool Settings::save()
{
    struct stat st
    {
    };

    if (::stat(ini_path_.c_str(), &st) != 0)
    {
        const auto pos = ini_path_.rfind('/');
        if (pos == std::string::npos || !ensureDir(ini_path_.substr(0, pos)))
            return false;
    }

    CSimpleIniA ini(true, false, false);
    ini.SetQuotes(true);
    (void)ini.LoadFile(ini_path_.c_str());
    for (const auto &kv : entries_)
        applyOneEntryToIni(ini, kv.second);
    return ini.SaveFile(ini_path_.c_str(), false) >= 0;
}

void Settings::reload()
{
    for (auto &kv : entries_)
        kv.second.value = kv.second.defaultValue;
    load();
}

Settings &instance()
{
    static Settings s;
    return s;
}

} // namespace settings
