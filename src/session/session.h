// SPDX-License-Identifier: MulanPub-2.0
// Copyright (c) 2026 Charry
// Freeime11 is licensed under Mulan PubL v2.
// You can use this software according to the terms and conditions of the Mulan PubL v2.
// You may obtain a copy of Mulan PubL v2 at: http://license.coscl.org.cn/MulanPubL-2.0
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
// EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
// MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
// See the Mulan PubL v2 for more details.

// FreeIME 11 —— 输入会话层（跨平台）
// 复刻极点五笔 10.8.9 的可感知输入行为，与 Windows TSF 前端解耦。
// 本层只依赖可移植的 engine/ 与 utils/，不依赖任何桌面框架。
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "engine.h"
#include "types.h"

namespace freeime11 {

// 输入状态机状态（对齐 10.8.9 的 state: idle / tempEnglish / tempPinyin / addPhrase / deletePhrase）
enum class SessionState {
    Idle,          // 空闲（英文直接输出态或未激活编码态）
    Composing,     // 五笔/拼音编码中
    TempEnglish,   // 临时英文（AutoEnStr 前缀或引导）
    TempPinyin,    // 临时拼音（z 引导）
    AddPhrase,     // 在线造词
    DeletePhrase,  // 在线删词
};

// 一次按键处理的结果，前端据此决定如何上屏/改预编辑。
struct SessionOutcome {
    bool handled = false;              // 是否被会话消费
    std::string commitText;            // 需要上屏的文本（UTF-8）
    std::string preedit;               // 预编辑串（含引导符）
    std::string auxText;               // 提示文本（如造词引导、外码提示）
    std::vector<std::string> cands;    // 当前候选文本
    std::vector<std::string> candCodes;// 候选完整编码（用于反查/加词）
    std::vector<std::string> candPrompts;
    int candCursor = 0;                // 当前高亮候选（0 起）
    bool candHasPrev = false;
    bool candHasNext = false;
    SessionState state = SessionState::Idle;
    bool englishMode = false;          // 当前是否英文（半角）态
};

class Session {
public:
    Session();
    ~Session();

    // 关键状态标志（状态栏显示用）
    bool chinesePunc() const;
    bool fullShape() const;
    bool simpTrad() const;   // true=简入繁出
    bool englishMode() const; // true=英文键盘模式（四模式循环的最后一档）
    // 是否还有未完成的编码（供前端判断编码输入框是否该保持可见）
    bool hasPendingInput() const;
    // 候选框输入框里的原文（编码串，如 "wqv"）；组字中才有，空表示没有
    std::string rawPreedit() const;
    // 引擎名与状态
    std::string engineName() const; // "wbzx" | "wbpy" | "py" | "en"

    // 这个按键是否应该由输入法接管。
    // 前端（TSF）用它决定是否吃掉按键：不带 Ctrl/Alt/Win，空闲态不吃空格/回车/退格/Esc/数字。
    bool wantsKey(int vk, bool shift, bool ctrl, bool alt) const;

    // 主入口：处理一次按键。vkey 使用 Windows 虚拟键码（兼容 keysym 映射）。
    // shift/ctrl/alt 为修饰键状态；按下=keydown 事件。
    SessionOutcome processKey(int vk, bool shift, bool ctrl, bool alt);

    // 直接选择第 n 个候选（0 起，全局下标），返回上屏结果并复位。
    SessionOutcome selectCandidate(int index);

    // 选择当前页内的第 n 个候选（0 起，鼠标点击用）。
    SessionOutcome selectCandidateOnPage(int index);

    // 翻页：delta = -1 上一页 / +1 下一页。返回刷新后的候选与预编辑。
    SessionOutcome pageCandidates(int delta);

    // 重置会话（切换窗口 / Esc）。
    void reset();

    // 切换功能（快捷键触发 / 设置面板触发）。
    // 键盘模式循环：五笔拼音 → 五笔 → 拼音 → 英文 → 五笔拼音（左右 Shift 单击都走这里）。
    // 不再提供独立的「中/英」开关：英文就是四模式循环的最后一档。
    void cycleMode();
    void toggleFullShape();    // 全半角
    void toggleChinesePunc();  // 中英文标点
    void toggleSimpTrad();     // 简繁
    void toggleCharset();      // GB2312 / GBK 字符集过滤

    // 配置加载（含旧版极点 INI 迁移）。返回是否成功。
    bool loadSettings(const std::string& dataDir);

    // 用户词典操作（供设置面板 / 快捷键）。
    bool addUserWord(const std::string& code, const std::string& text);
    bool deleteUserWord(const std::string& code, const std::string& text);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    // 按配置 inputMode 选择输入方案（engine:wbzx / engine:wbpy / engine:py / engine:en）
    void applyEngineFromSettings();
};

// Windows 虚拟键码 → 引擎可识别的码元（字母/数字/符号）。返回 '\0' 表示不可编码。
char vkToCodeChar(int vk, bool shift);

} // namespace freeime11
