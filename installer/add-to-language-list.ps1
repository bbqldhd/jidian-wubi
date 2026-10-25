# FreeIME 11 —— 将输入法加入当前用户的语言列表
# 由 Inno Setup 安装程序以原用户身份（runasoriginaluser）调用。
$ErrorActionPreference = 'SilentlyContinue'

$clsid   = "{B6F2C8A1-7D4E-4F1A-9C3B-2E5D8A7F6E10}"
$profile = "{C9D4E2B7-5A3F-4B8C-9D1E-6F0A2C4B8D53}"
$tip     = "$clsid$profile"

try {
    $list = Get-WinUserLanguageList
    $zh = $list | Where-Object { $_.LanguageTag -like "zh-Han*" } | Select-Object -First 1
    if (-not $zh) {
        $list.Add((New-WinUserLanguageList "zh-Hans-CN"))
        $zh = $list | Where-Object { $_.LanguageTag -like "zh-Han*" } | Select-Object -First 1
    }
    if ($zh -and -not ($zh.InputMethodTips -contains $tip)) {
        $zh.InputMethodTips.Add($tip)
        Set-WinUserLanguageList $list -Force
    }
} catch {
    # 静默失败，不阻断安装；用户仍可在系统设置中手动添加
}
