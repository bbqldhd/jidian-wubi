# 极点五笔 11 (FreeIME 11)

极点五笔（极点中文汉字输入平台）的现代重制版：以现代 **TSF（Text Services Framework）** 完整重写，
目标平台 **Windows 11 26H2**（兼容 Windows 10 1809+），支持 **x64 / ARM64** 双原生架构，
版本号 **11.0.4**。

本重制基于对原版 `setup10.8.9.exe`（极点五笔纪念版，纯 IMM32 架构）的逆向分析，以及原作者
在 openKylin 开源的 [FreeWB](https://gitee.com/openkylin/freewb)（Mulan PSL v2.0）作为引擎蓝本，
保持原版的按键逻辑、候选交互、配置项与用户词典语义不变，仅做现代 Windows 兼容适配。

## 目录结构

```
freeime11-win/
├── CMakeLists.txt           # 构建（MSVC x64/ARM64；MinGW 用于可移植层验证）
├── src/
│   ├── engine/              # 五笔/拼音/五笔拼音/英文引擎（vendor 自 FreeWB，Mulan PSL v2.0）
│   ├── utils/               # 配置/键码/sqlite/UTF 转换等（vendor 自 FreeWB，已做 Windows 补丁）
│   ├── session/             # 跨平台输入会话层（复刻 10.8.9 按键/候选/标点/造词行为）
│   └── win/
│       ├── tsf/             # TSF 文本服务（ITfTextInputProcessorEx 等）
│       ├── config/          # 10.8.9 旧版 GBK INI → 新配置迁移
│       ├── migrate/         # 迁移工具
│       └── settings_app/    # 设置程序
├── cmake/                   # 版本资源 11.0.4、DPI 感知 manifest
├── installer/               # WiX (MSI) 安装脚本
├── tests/                   # 会话层冒烟测试
└── docs/                    # 逆向分析报告、实现说明、数据格式、缺陷与验证清单
```

## 构建

### Windows（生成 TSF 文本服务，需 Visual Studio + Windows SDK）

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

ARM64：
```bat
cmake -S . -B build-arm64 -G "Visual Studio 17 2022" -A ARM64
cmake --build build-arm64 --config Release
```

### 可移植层独立验证（MinGW，无 Windows SDK 亦可）

```bash
cmake -S . -B build -G Ninja -DFREEWB_BUILD_TSF=OFF -DFREEWB_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build
```

### MinGW (Dev-C++) 一键构建（本机已验证）

```bash
bash build_mingw.sh
```

产出：`build/FreeIME11.dll`（TSF 输入法）、`build/freeime11-migrate.exe`、
`build/freeime11-settings.exe`、`build/freeime11-uninstall.exe`、
`build/freeime11-addlang.exe`、`build/session_test.exe`。

## 安装

1. 运行 `FreeIME11-11.0.4-setup.exe`（Inno Setup 标准安装向导：自动请求管理员、注册 TSF 文本服务、
   写入「添加/删除程序」、创建开始菜单快捷方式）。
2. 安装结束时 `freeime11-addlang.exe` 会以当前用户身份启用输入法
   （`ActivateProfile` + `EnableLanguageProfile` + 写入 `REG_DWORD` 语言列表项），
   装完即可在输入法列表里看到「极点五笔 11」，无需手动添加。
3. 若从旧版升级且当时输入法正在使用，DLL 会安排到下次重启替换；其余组件立即生效。
4. 可选：`freeime11-migrate "旧版极点目录"` 迁移 10.8.9 配置（已验证可迁移 19 项配置）。

## 卸载

在「设置 → 应用 → 已安装的应用」中找到「极点五笔 11」卸载；或运行安装目录下的
`freeime11-uninstall.exe`（注销 TSF 文本服务、删除安装文件，并可选删除用户词典与配置
`%APPDATA%\FreeIME11`）。

> 说明：MinGW 构建的 DLL 为 **x64** 目标。ARM64 需用 Visual Studio `-A ARM64` 构建。
> MinGW 的 `msctf.h` 不含 `ITfCandidateListUIElement`，因此候选窗为自绘实现
> （分层窗口 + GDI+，`src/win/tsf/CandidateWindow.cpp`），不依赖 TSF3 接口。

## v11.0.4：候选窗

- **新增自绘候选窗**：候选不再拼进组字串，改为跟随插入符的浮动窗口
  （圆角、阴影、首个候选高亮、翻页箭头）。组字区只显示输入的编码。
- 候选窗为 `WS_EX_NOACTIVATE | WS_EX_LAYERED` 弹出窗，不抢焦点、不打断输入。
- 位置取自 `ITfContextView::GetTextExt`；取不到时退回 GUI 线程插入符坐标。

## v11.0.3 修复（「设置里加完、返回就失效」）

- **根因：语言列表值类型写错了**。`HKCU\Control Panel\International\User Profile\zh-Hans-CN`
  下「值名 = `0804:{CLSID}{Profile}`」的条目**必须是 `REG_DWORD`**（序号）。早期版本写成了
  `REG_SZ`，Windows 静默忽略——设置界面能把输入法加进去（它读的是已注册配置列表），
  但一刷新就消失。现在 `freeime11-addlang.exe` 会写入/纠正为 `REG_DWORD`。
- **类别注册结构与系统输入法不一致**：正向表应为 `Category\Category\{catid}\{CLSID}`
  （早期误用了语言配置 GUID），并补上反向索引 `Category\Item\{CLSID}\{catid}`；
  同时移除了一个并非 `UIELEMENTENABLED` 的 GUID（`{F4BAA7A3-…}`，本实现也没有 UI 元素）。
- **`DllRegisterServer` 未初始化 COM**：补 `CoInitialize/CoUninitialize`。
- **启用输入法走官方接口**：`ITfInputProcessorProfileMgr::ActivateProfile` +
  `ITfInputProcessorProfiles::EnableLanguageProfile`。
- **升级装不上新 DLL**：TSF DLL 常被 explorer/ctfmon 占用，给文件加 `restartreplace`
  安排替换；安装前只清理指向自身目录的「纯删除」项，保留「改名/替换」项。
- **多用户词库缺失**：词库随程序装到 `{app}\data`，再由 `addlang` 按用户复制到
  `%APPDATA%\FreeIME11\data`。

## 许可

- 引擎与工具源码来自 FreeWB，按 **Mulan PSL v2.0** 分发（见 `LICENSE`）。
- 本项目新增的 Windows TSF 前端、会话层、迁移工具同样以 Mulan PSL v2.0 分发。
- “极点五笔”名称与图标的权利归原作者，使用时请遵守相应授权。
