<!--
  SPDX-License-Identifier: MulanPSL-2.0
  Copyright (c) 2026 Charry
  Freeime11 is licensed under Mulan PSL v2.
  You can use this software according to the terms and conditions of the Mulan PSL v2.
  You may obtain a copy of Mulan PSL v2 at: http://license.coscl.org.cn/MulanPSL2
  THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
  EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
  MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
  See the Mulan PSL v2 for more details.
-->

# 极点五笔 11 (FreeIME 11)

极点五笔（极点中文汉字输入平台）的现代重制版：以现代 **TSF（Text Services Framework）** 完整重写，
目标平台 **Windows 11 26H2**（兼容 Windows 10 1809+），支持 **x64 / ARM64** 双原生架构，
版本号 **11.1.0**。

本重制基于对原版 `setup10.8.9.exe`（极点五笔纪念版，纯 IMM32 架构）的逆向分析，以及原作者
在 openKylin 开源的 [FreeWB](https://gitee.com/openkylin/freewb)（Mulan PSL v2.0）作为引擎蓝本，
保持原版的按键逻辑、候选交互、配置项与用户词典语义不变，仅做现代 Windows 兼容适配。

> 本项目（极点五笔 11 / FreeIME 11）的逆向分析、源码重构、编译打包与测试发布，**完全由 AI + REA 完成**。

[![License](https://img.shields.io/badge/license-Mulan%20PSL%20v2-blue.svg)](LICENSE)
[![Release](https://img.shields.io/badge/release-v11.1.0-green.svg)](https://github.com/bbqldhd/jidian-wubi/releases)

| 平台 | 地址 |
|---|---|
| GitHub | https://github.com/bbqldhd/jidian-wubi |
| Gitee  | https://gitee.com/red-egg-carrying-ice-cream/jidian-wubi |

## 目录结构

```
freeime11-win/
├── CMakeLists.txt           # 构建（MSVC x64/ARM64；MinGW 用于可移植层验证）
├── src/
│   ├── engine/              # 五笔/拼音/五笔拼音/英文引擎（vendor 自 FreeWB，Mulan PSL v2.0）
│   ├── utils/               # 配置/键码/sqlite/UTF 转换等（vendor 自 FreeWB，已做 Windows 补丁）
│   ├── session/             # 跨平台输入会话层（复刻 10.8.9 按键/候选/标点/造词行为）
│   └── win/
│       ├── tsf/             # TSF 文本服务、候选窗、状态工具栏
│       ├── config/          # 10.8.9 旧版 GBK INI → 新配置迁移
│       ├── migrate/         # 迁移工具
│       └── settings_app/    # 设置程序
├── cmake/                   # 版本资源 11.1.0、DPI 感知 manifest
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

1. 运行 `FreeIME11-11.1.0-setup.exe`（Inno Setup 标准安装向导：自动请求管理员、注册 TSF 文本服务、
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

## v11.1.0：输入法状态工具栏

- **新增浮动工具栏**：跟随当前编辑窗口出现，可拖动、位置记忆（`HKCU\Software\FreeIME11\Toolbar`），
  点它不抢焦点，因此不会打断正在进行的输入。
- **按钮**：`中/英`、`半/全`、`，。/,.`、`简/繁`、`五笔/五拼/拼音/英文`、`设置`、`×`。
  开关类按钮点亮淡蓝底 + 蓝字；右 Shift 切中英、快捷键切方案时工具栏同步刷新。
- **只在有编辑上下文时出现**：每个线程都可能创建一个文本服务实例，若在 `Activate` 里无条件弹窗，
  切窗口时会叠出多条工具栏；现在改为接管到焦点上下文才显示、焦点离开文档即收起，`×` 只收起本次激活。
- **切换中英/方案时清理组合**：若正在组字，先结束组合并清掉文档里的编码串，避免残留字母。
- **`设置` 直接拉起同目录的 `freeime11-settings.exe`**。
- **Shift 单击切换中英**：左/右 Shift 都支持。实现要点是必须先把 Shift 的按下/抬起事件
  「吃下」——`OnTestKeyDown` 返回 `FALSE` 时 TSF 根本不会回调 `OnKeyDown`，原来在按下阶段
  切换永远等不到事件，这就是「按 Shift 没反应」的根因。现在按下只记录，**松开时**才判定：
  期间只要出现过任何其它按键（含被放行的方向键、Ctrl 组合）就取消，所以 Shift+字母大写、
  Shift+方向键选区都不会误切。
- **默认输入方案改为五笔拼音**：`inputMode` 默认 `engine:wbpy`；启动时按该配置选方案，
  工具栏切换方案会写回配置，下次仍是上次用的方案。方案轮转只在 五笔 / 五笔拼音 / 拼音 之间，
  并遵循 `WbzxEngine`/`WbpyEngine`/`PyEngine` 三个开关。
- 自测：`tests/toolbar_window_test.cpp`（显示、`MA_NOACTIVATE`、逐个按钮命中与回调、
  状态切换不改变整体宽度、位置记忆）通过。

## v11.0.8：选词后候选窗不收

- **候选窗残留**：上屏路径 `commitText()` 只结束组合、从未隐藏候选窗。选完词后文字已进文档，
  候选窗却一直挂在输入框下方，看起来像「没选中、还在输入」。现在上屏（含自动上屏、标点上屏）
  立即收起候选窗，不等异步编辑会话执行。
- **上屏改为「替换文字再结束组合」**：原来先把组合区间清空、`EndComposition()` 之后再在区间上
  插入正文，属于「空组合 + 组合外插入」两步操作；部分应用会把这段插入当成未完成输入继续显示。
  现在直接把组合区间里的编码串替换为最终文本，设置光标后一次 `EndComposition()`，文字自然留在
  文档里，也不再依赖结束后的区间是否仍然有效。
- **`GUID_PROP_COMPOSING` 清理**仅保留在无组合的降级路径（有组合时由 `EndComposition` 负责），
  避免提前清属性导致应用把正文当成普通插入或重复提交。
- **候选窗丢失 / 点击失效**：候选窗原本用 `DefWindowProcW` 当窗口过程，鼠标消息无人处理，
  点击既选不中候选，也可能被系统默认处理牵走激活状态，导致文档失焦、组合终止、候选窗消失且
  无法自行恢复。现在改用自定义窗口过程：`WM_MOUSEACTIVATE` 返回 `MA_NOACTIVATE`（点候选窗
  不抢焦点）、窗口被销毁/隐藏后 `show()` 会自动重建并重新显示。
- **鼠标点选候选**：窗口按布局缓存每个候选与翻页箭头的矩形，`WM_LBUTTONUP` 命中后回调
  TSF 服务，走与键盘选词完全相同的上屏流程（`selectCandidateOnPage` / `pageCandidates`）。
- **悬停反馈**：`WM_MOUSEMOVE` 命中候选时画淡蓝底 + 淡边框，翻页箭头同样高亮，并切换手型
  光标；`WM_MOUSELEAVE` 清除悬停态。
- **↑/↓ 与 PageUp/PageDown**：组字中 ↑/↓ 在当前页内移动高亮（空格/回车上屏高亮项），
  PageUp/PageDown 翻页并回到页首；长按可连续移动（不再被「忽略重复键」挡掉）。

## v11.0.7：上屏残留与切换响应

- **选词后编码串残留**：`ITfComposition::EndComposition()` 只解除组合、**不会删除组合区间里的文字**
  （官方文档明确：仅移除 `GUID_PROP_COMPOSING`）。旧实现上屏时直接 End，`wqvb` 这类编码就留在
  文档里。现在上屏/取消前先对组合区间 `SetText("")` 再 `Collapse(START)`，随后结束组合并在该位置
  插入最终文本，最后把光标移到插入内容之后。
- **编辑会话数据串扰**：原先服务对象自己兼作 `ITfEditSession`，`RequestEditSession` 排队期间字段会被
  后续按键覆盖，旧会话可能拿新数据重放（重复上屏/把已清掉的编码写回）。现在每次请求创建独立的
  `EditRequest` 快照对象。
- **切换输入法后迟迟不响应**：`context_` 只在 `ITfThreadMgrEventSink::OnSetFocus` 赋值，而切换输入法
  时服务往往在焦点建立之后才激活，收不到该回调 → `context_` 为空，按键被判定为「要处理」并吃掉，
  却没有任何编辑会话可执行。现在 `Activate` 主动 `GetFocus()` 接管上下文；`OnKeyDown` 一律以实际
  收到按键的 `ITfContext` 为准；`OnPopContext`（关闭模态框等）后重新接管焦点。
- **启动开销**：拼音词库（8 MB）改为首次真正输入拼音时才解析，切换输入法的服务初始化从
  约 215 ms 降到约 55 ms（同一台机器、热缓存）。

## v11.0.6：按键归属

- **按键不再被无条件吃掉**：新增 `Session::wantsKey()`，空闲态的空格/回车/退格/Esc/Tab/数字交给
  应用处理；只有组字中才接管编辑键。之前这些键被吞，表现为「好多按键都不对」。
- **Ctrl/Alt/Win 组合一律放行**（修复 `Ctrl+空格` 切不了输入法）；英文态整键透传，不再自行插字符
  破坏快捷键与自动补全。

## v11.0.5：上屏与组字定位

- **组字与上屏改用插入符位置**：之前两处都调 `ITfContext::GetStart()`——那是**文档开头**，
  组字还额外 `Collapse(END)` 跑到**文档末尾**。结果是组字永远在文件末尾、上屏永远插到文件最前面，
  在非空文档里字会乱跑、错位。现在统一走 `GetSelection()` 取插入符。
- **上屏后光标移到插入内容之后**（并写回 selection），否则下一次输入会覆盖刚打出的字。
- **不支持 TSF 组合的应用降级**：若 `StartComposition` 失败（终端、部分 Electron 应用常见），
  不再整体失败，改为「只出候选窗 + 提交时直接插入」。

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

本项目以 **木兰宽松许可证，第 2 版（Mulan Permissive Software License, Version 2，Mulan PSL v2.0）** 开源，
许可证全文见 [`LICENSE`](LICENSE)，第三方组件与来源说明见 [`NOTICE`](NOTICE)。

```
Copyright (c) 2026 Charry
Freeime11 is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at: http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details.
```

每个源文件头部均带有同一段声明（按语言注释语法书写），由 `tools/add_license_headers.py` 统一注入。

注：根目录 `LICENSE` 已按许可证要求填入实际信息（`Copyright (c) 2026 Charry` / `Freeime11`）。
GitHub 的许可证自动识别对木兰许可证较严格，填入真实信息后会显示为 `Other`（Gitee 仍识别为
MulanPSL-2.0）；如需 GitHub 侧显示许可证徽标，可把这两处改回官方占位符。

- 你可以自由使用、修改、分发本软件（含商业用途），但须保留版权与许可声明，并在分发时附上许可证副本。
- `src/engine/`、`src/utils/` 中的引擎代码派生自 [FreeWB](https://gitee.com/openkylin/freewb)，版权归其贡献者所有，同样以 Mulan PSL v2.0 分发。
- `third_party/spdlog` 为 MIT 许可；SQLite 为 Public Domain；libiconv 为 LGPL-2.1+（MinGW 运行库，静态链接）。详见 `NOTICE`。
- 「极点五笔」「极点中文」名称与图标的权利归原作者所有；本项目为兼容性重制，与原作者无隶属关系。
