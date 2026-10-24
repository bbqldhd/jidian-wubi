#include "utils.h"

#include <cstdlib>
#include <string>
#include <vector>

#ifdef _WIN32
#include <shlobj.h>
#endif

namespace freewb
{
const std::string userFreewbPath()
{
    // 显式数据目录覆盖（测试/便携模式）
    const char *dataDir = std::getenv("FREEWB_DATA_DIR");
    if (dataDir && *dataDir)
    {
        return std::string(dataDir);
    }

#ifdef _WIN32
    // Windows：每用户数据目录 %APPDATA%\FreeIME11
    wchar_t appData[MAX_PATH] = {0};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, appData)))
    {
        char buf[MAX_PATH * 3] = {0};
        WideCharToMultiByte(CP_UTF8, 0, appData, -1, buf, sizeof(buf), nullptr, nullptr);
        return std::string(buf) + "\\FreeIME11";
    }
    return {};
#else
    const char *home = std::getenv("HOME");
    if (!home || !*home)
    {
        return {};
    }
    return std::string(home) + "/.local/freewb";
#endif
}
} // namespace freewb