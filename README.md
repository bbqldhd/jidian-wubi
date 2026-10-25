# 极点五笔 11 (FreeIME 11)

极点五笔（极点中文汉字输入平台）的现代重制版：以现代 **TSF（Text Services Framework）** 完整重写，
目标平台 **Windows 11 26H2**（兼容 Windows 10 1809+），支持 **x64 / ARM64** 双原生架构，
版本号 **11.0.2**。

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
├── cmake/                   # 版本资源 11.0.2、DPI 感知 manifest
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

1. 运行 `FreeIME11-11.0.2-setup.exe`（Inno Setup 标准安装向导：自动请求管理员、注册 TSF 文本服务、
   写入「添加/删除程序」、创建开始菜单快捷方式）。
2. 安装程序会调用 `ITfInputProcessorProfileMgr::ActivateProfile` 把输入法启用为当前用户的一个键盘，
   并打开语言设置页。
3. 若「极点五笔 11」未立刻出现在输入法列表，请在「设置 → 时间和语言 → 语言和区域 → 中文(简体)」
   里点「添加键盘」选一次；Windows 11 的用户输入法列表由系统缓存维护，首次注册后需要一次用户确认
   或重新登录才会刷新。
4. 可选：`freeime11-migrate "旧版极点目录"` 迁移 10.8.9 配置（已验证可迁移 19 项配置）。

## 卸载

在「设置 → 应用 → 已安装的应用」中找到「极点五笔 11」卸载；或运行安装目录下的
`freeime11-uninstall.exe`（注销 TSF 文本服务、删除安装文件，并可选删除用户词典与配置
`%APPDATA%\FreeIME11`）。

> 说明：MinGW 构建的 DLL 为 **x64** 目标。ARM64 需用 Visual Studio `-A ARM64` 构建。
> MinGW 的 `msctf.h` 不含系统集成候选 UI 接口，当前候选以组合内嵌文本显示；
> 用 MSVC + 完整 Windows SDK 构建可切换为 `ITfCandidateListUIElement` 系统候选 UI。

## v11.0.2 修复

- **输入法无法注册**：类别注册路径由错误的 `Category\Item\{catid}` 改为
  `Category\Category\{catid}`（微软输入法实际使用后者），此前系统因此不认这个 TIP。
- **`DllRegisterServer` 未初始化 COM**：`CoCreateInstance` 前补 `CoInitialize/CoUninitialize`，
  否则语言配置注册静默失败。
- **升级被「请先重启」挡住**：卸载时 DLL 被输入法宿主占用会登记「重启后删除」，导致下一次安装
  直接拒绝。安装程序现在会先清理仅指向自身目录的待删项，其它软件的条目原样保留。
- **多用户词库缺失**：词库随程序安装到 `{app}\data`，再由 `freeime11-addlang.exe` 按用户复制到
  `%APPDATA%\FreeIME11\data`，安装不再只服务管理员账户。

## 许可

- 引擎与工具源码来自 FreeWB，按 **Mulan PSL v2.0** 分发（见 `LICENSE`）。
- 本项目新增的 Windows TSF 前端、会话层、迁移工具同样以 Mulan PSL v2.0 分发。
- “极点五笔”名称与图标的权利归原作者，使用时请遵守相应授权。
