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
版本号 **11.2.3**。

本重制基于对原版 `setup10.8.9.exe`（极点五笔纪念版，纯 IMM32 架构）的逆向分析，以及原作者
在 openKylin 开源的 [FreeWB](https://gitee.com/openkylin/freewb)（Mulan PSL v2.0）作为引擎蓝本，
保持原版的按键逻辑、候选交互、配置项与用户词典语义不变，仅做现代 Windows 兼容适配。

> 本项目（极点五笔 11 / FreeIME 11）的逆向分析、源码重构、编译打包与测试发布，**完全由 AI + REA 完成**。

[![License](https://img.shields.io/badge/license-Mulan%20PSL%20v2-blue.svg)](LICENSE)
[![Release](https://img.shields.io/badge/release-v11.2.3-green.svg)](https://github.com/bbqldhd/jidian-wubi/releases)

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
├── cmake/                   # 版本资源 11.2.3、DPI 感知 manifest
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
bash build_mingw.sh            # 默认并发 = 物理核心数（i5-12500H 上是 12）
JOBS=16 bash build_mingw.sh    # 想拉满逻辑线程
```

每个 `.cpp` 单独编成 `build/obj/*.o` 并行编译，源文件没改动就跳过，再统一链接。
全量约 60 秒、增量约 25 秒。

产出：`build/FreeIME11.dll`（TSF 输入法）、`build/freeime11-migrate.exe`、
`build/freeime11-settings.exe`、`build/freeime11-uninstall.exe`、
`build/freeime11-addlang.exe`、`build/session_test.exe`。

### 数据文件（词库不入库）

本仓库**不含词库文件**：`.mb` 是词库数据，其授权不覆盖再分发，因此只在本地构建目录里存在
（`.gitignore` 已拦截）。

打安装包需要这两个文件，默认从仓库**同级目录**的 `upstream-freewb` 取：

```
<工作目录>/
├── freeime11-win/          # 本仓库
└── upstream-freewb/
    └── data/mb/default/
        ├── wbzx.mb         # 五笔词库
        └── pinyin.mb       # 拼音词库
```

`installer/freeime11.iss` 里的路径指向 `..\..\upstream-freewb\...`，换成别处请自行改这两行。

## 安装

1. 运行 `FreeIME11-11.2.3-setup.exe`（Inno Setup 标准安装向导：自动请求管理员、注册 TSF 文本服务、
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

## v11.2.3：修 Shift 打不出中文标点

**现象**：Shift+1 打出来是半角 `!`，不是 `！`；Shift+4 / 6 / 7 / 9 / 0 一起失灵
（`￥` `…` `—` `（` `）` 全都出不来）。

**原因**：输入法有两道关——`wantsKey()` 决定这个键归不归输入法管，`processKey()` 决定归它以后
输出什么。v11.2.2 里 `wantsKey()` 的数字分支写成了「数字一律不接管（全角除外）」，
把 Shift+数字也一起放行了；可 `processKey()` 明明会把 Shift+1 转成「！」。
两道关的判断对不上，按键在门口就被交给应用，后面那套标点转换永远轮不到执行。

**修复**：数字键按 Shift 分开判断——不按 Shift 保持原样（数字归应用，全角模式自己上屏全角数字），
按了 Shift 就按标点表判断该不该接管。`#` `%` `*` 这种映射成自己的键仍然原样交给应用。

**验证**：新增 `tests/keygate_test.cpp`，把 `wantsKey()` 与 `processKey()` 逐键对照——
数字 × Shift、全部 OEM 符号 × Shift、字母 × Shift，要求两边结论一致；
八个自测全部通过。字母、空格、方向键、Ctrl/Alt 组合的行为没有任何变化。

## v11.2.2：拼音智能纠错、四模式轮换、中文标点

**拼音会猜你打错的字母**：`z/zh`、`c/ch`、`s/sh`、`an/ang`、`en/eng`、`in/ing` 打错一个都能纠回来。

- `zongguo` → **中国**（整串没有词条命中，纠错结果直接顶到第一位）
- `zonghuarenmingongheguo` → **中华人民共和国**
- 打对的时候一点不受影响：`zhongguo`、`nihao` 的首选与候选条数和上一版一致
- 只在一个整串都命不中词条时才纠错，正常打字不会混进多余候选
- 不想要可以在 `config.ini` 里写 `pinyinFuzzy=false`

**键盘模式合成一条链**：去掉单独的「中/英」开关，左右 Shift 单击都按
**五笔拼音 → 五笔 → 拼音 → 英文 → 五笔拼音** 轮换，工具栏第一个按钮直接显示当前档位，
点它也能切。

**中文标点补齐**：

| 按键 | 输出 | 按键 | 输出 |
|---|---|---|---|
| `,` / `.` | ，。 | `;` / `:` | ；： |
| `?` / `!` | ？！ | `<` / `>` | 《》 |
| `[` / `]` | 【】 | `{` / `}` | 『』 |
| `\` / `$` | 、￥ | `^` | … |

`Shift+'` 依次给 `“` `”`，`'` 依次给 `‘` `’`（智能配对开着时才交替）；
全角模式下数字、以及没有中文映射的标点也变全角。`#` `%` `*` 这类映射成自己的键不接管，
原样交给应用。

## v11.2.1：五笔词库扩容

五笔模式换上了扩充后的码表：

| | 原来 | 现在 |
|---|---|---|
| 词条 | 87,927 | **108,302** |
| 编码 | 76,592 | **86,151** |
| 词库文件 | 1.82 MB | 2.15 MB |

**多出来的 20,375 条**主要是网络热词、专名与常用词组，例如：

- `aaeu` → **萧敬腾**、`aaff` → **蓬蓬勃勃**、`aady` → **工矿**、`aagk` → **工事**
- `wqy` → 新增 **保外就医**、**集贸市场**；`khlg` → 新增 **贵国**；`mfk` → 新增 **周口**

**同码内的排位也更合理**：以前一些编码下排第一的是生僻字（如 `acaf` 的「茎」、`aalf` 的「盞」），
现在常见词提到前面（`acaf` → **芭蕾**、`aalf` → **葡萄园**、`abkm` → **警卫员**）。
76,057 个共有编码里，相对顺序变化的只有 1.5%。

原有条目一条没丢：一级简码（`a` → 工/戈/式/戒）、二级简码、`zz` 特殊键下的拼音声调字符
都按原样保留，词组编码规则块也原样带回。

## v11.2.0：拼音词库扩容，打字更省事

**词库已更新**：拼音词库从 355,454 条扩到 **526,475 条**，新增 171,021 条——4 字成语约
4.6 万条、5–7 字诗词名句约 7.8 万条，另有一批法律、物理、军事、财经、农业等专业词汇。
词库文件 8.2 MB → 14.3 MB。

**用起来更省事**：

- **想打的词现在直接就能打出来**：成语、诗句、专业术语不用再切到别的输入法。
  `chuangqianmingyueguang` → **床前明月光**、`buzhaobianji` → **不着边际**
- **常用词的首选完全没变**：是 / 我 / 你 / 中国 / 人民 / 世界 / 工作 / 学习 / 电脑 / 知道 /
  没有 / 生活 —— 原来的手感不受影响
- **配合 v11.1.1 的拼音整句输入**，一长串拼音能一次上屏成整句
- **常驻内存反而更省**（见下表），输入法挂在系统里不占资源

| | 原来 | 现在 |
|---|---|---|
| 词条 | 355,454 | **526,475** |
| 编码数 | — | 325,164 |
| 词库文件 | 8.2 MB | 14.3 MB |

新增词条按「全为汉字、且字数等于音节数」筛选，正好匹配引擎整句切分的约束；
人名、地名、公司名一类的条目已剔除，避免混进候选。

**顺带把词库内存砍掉一半**。原来 `MbDictionaryTable` 同时存 `records_`（有序数组）和两张
`unordered_map<string, vector<string>>`（单字表/词组表），每个词条的编码与文本都要存两份——
拼音词库涨到 52 万条后，仅这份副本就多占 55 MB。现在统一走 `records_` 二分查找
（`codeRange()` / `hasCode()`），映射表整个删掉：

| | 改造前 | 改造后 |
|---|---|---|
| 扩容前的词库 | 61.6 MB | **31.7 MB** |
| 扩容后的词库 | 100.2 MB | **45.4 MB** |
| 首次加载（扩容后） | 395 ms | **178 ms** |
| 单次查询 | 15 µs | 18 µs |

也就是说：**词库大了 48%，内存反而比原来少了 26%**。整数二分带来的 3 µs 查询开销对打字无感。

## v11.1.2：修 3 个 bug

**严重 · 拼音整句被自动上屏截断**（v11.1.1 的核心功能实际不可用）
默认「五笔拼音」方案下打 `woxihuanni`，到第 6 个键 `woxihu` 时只有一条整句候选，
触发自动上屏变成「我西湖」，一句话永远打不完。根因是自动上屏规则写成
`minLen = (engineIdx == kWbzx) ? 4 : 2` —— `else` 把拼音也算成「2 码」。
现在按方案区分：五笔仍是「满 4 码唯一 / 简码唯一」；五笔拼音只认「满 4 码 **且** 是五笔精确码」；
纯拼音与英文一律不自动上屏，靠空格/数字确认。

**严重 · 异步编辑请求可能写错窗口**
`EditRequest` 排队等 TSF 执行期间若焦点切到了别的程序，`applyEdit` 用的是**当前**的
`context_`，但 `TfEditCookie` 属于**旧**上下文——拿 A 的 cookie 去改 B 的文档，
轻则失败重则把字插进别的程序。现在请求携带发起时的 `ITfContext`（AddRef），执行时比对，
不一致就丢弃；例外是 `ES_Clear`，它带着自己的组合对象，必须执行，否则旧文档里的编码串会残留。

**中 · 组字中按数字会漏进文档**
拼音打到没有候选时（如 `woxih`）按 `1`，会走 `else if (isDigit) return out;`，
`handled=false` → 数字被打进文档，把编码打断。现在组字中吞掉数字，并把当前编码
`fillOutcome` 原样回填，否则上层会当成空结果把组合区清掉。

**自测**：新增 `tests/autocommit_test.cpp`，覆盖 五笔 / 五笔拼音 / 拼音 三种方案 ×
6 个场景（4 码唯一自动上屏、五笔拼音不被截断、拼音整句、空格确认、组字中数字吞掉），
已接入 `build_mingw.sh`。另跑 30 万次随机按键的模糊测试，无崩溃。

## v11.1.1：拼音整句输入

**问题**：拼音引擎只做「整串前缀匹配」——只有词库里存在这一整串编码才出候选。实测词库最长
词条 11 个汉字、最长编码 32 个字母，所以拼音模式实际上「一次只能出一个词」，打一串长拼音
（`woxihuanni`、`jintiantianqi`）直接 0 候选，什么也上不了屏。

**改动**

1. **整句组词**：整串在词库里查不到时，按音节把它切成若干词再拼成一整句，最多给 3 条整句
   候选。词库里有整词时行为完全不变。
2. **切分算法**：从后往前的动态规划，`best[i]` = 把 `code[i..]` 全部切成词能拿到的最高分。
   每个位置只试 1~32 个字母，候选直接查词组表/单字表的哈希，整体复杂度 `O(n×32)`，
   实测 48 个字母逐键重算合计 **3ms**。
3. **打分规则**：每多切一段 −1（整串音节总数固定，所以「段数最少」等价于「尽量用长词」），
   同分取更长的切法。
4. **两条质量约束**（否则会拼出乱码）：
   - 候选词的字数必须等于它编码的音节数 —— 挡掉词库里的简码/缩写码（`wo→玩偶`、`hua→护岸`）；
   - 多音节片段里不允许夹单字母音节 —— 挡掉 `n+de→恩德`、`n+qi→暖气` 这类硬切噪声。
5. **排序保守**：整串有完整匹配、或已有前缀候选时，整句一律排在**末尾**；只有原本「0 候选」
   的情况才顶到第一位。打字过程中看到的前缀候选顺序不受任何影响。
6. **长度上限**：整句切分只在 ≤128 个字母时进行（约 40 个汉字以上），超长输入只做原有前缀
   匹配，避免每个按键都跑大 DP。单次上屏字数从「受词库词条限制（≤11 字）」提升到「约 40 字」。

**效果**：`woxihuanni` → 我喜欢你；`woxihuannima` → 我喜欢你吗；`jintiantianqizhenhao` →
今天天气真好。`nihao` 仍是唯一候选「你好」，`niha` 仍是「你好」在前，`abc`/`zzzzzzzz` 这类
非法串仍不产生候选 —— 原有行为零回归。

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
