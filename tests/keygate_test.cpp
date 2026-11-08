// SPDX-License-Identifier: MulanPub-2.0
// Copyright (c) 2026 Charry
// Freeime11 is licensed under Mulan PubL v2.
// You can use this software according to the terms and conditions of the Mulan PubL v2.
// You may obtain a copy of Mulan PubL v2 at: http://license.coscl.org.cn/MulanPubL-2.0
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
// EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
// MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
// See the Mulan PubL v2 for more details.

// 拦键判断回归测试：
//   wantsKey() 决定 TSF 要不要吃掉按键，processKey() 决定吃掉以后输出什么。
//   两者必须一致：说「不归我」的键，processKey 就不该有产出；
//   说「归我」的键，processKey 必须真的产出对应字符。
//
//   ★ 这里所有符号键都用**真实的 Windows 虚拟键码**（VK_OEM_*），
//     不是 ASCII。曾经因为测试里拿 ASCII '.'(0x2E) 去喂，而系统实际发的是
//     VK_OEM_PERIOD(0xBE)，两边永远对不上，于是中文标点整个失效却一直测不出来。
//
//   历史事故二：wantsKey 的数字分支无条件 return fullShape()，把 Shift+数字
//   整体放行给应用，而 processKey 会把它转成「！」——Shift 打出来全是半角符号。
#include "../src/session/session.h"
#include "../src/utils/settings.h"

#include <cassert>
#include <cstdio>
#include <string>

using namespace freeime11;

namespace {

// Windows 虚拟键码（与 WinUser.h 一致；这里不引 windows.h，避免污染跨平台测试）
enum : int {
    VK_OEM_1_ = 0xBA,      // ;
    VK_OEM_PLUS_ = 0xBB,   // =
    VK_OEM_COMMA_ = 0xBC,  // ,
    VK_OEM_MINUS_ = 0xBD,  // -
    VK_OEM_PERIOD_ = 0xBE, // .
    VK_OEM_2_ = 0xBF,      // /
    VK_OEM_3_ = 0xC0,      // `
    VK_OEM_4_ = 0xDB,      // [
    VK_OEM_5_ = 0xDC,      // '\'
    VK_OEM_6_ = 0xDD,      // ]
    VK_OEM_7_ = 0xDE,      // '
};

struct Expect {
    int vk;
    bool shift;
    bool take;        // wantsKey 期望值
    const char *text; // processKey 期望上屏（take=false 时必须为空）
};

// 中文标点开、全角关时的完整键盘表（空闲态）
const Expect kCnTable[] = {
    {'1', false, false, ""},
    {'1', true, true, "！"},
    {'2', true, true, "·"},
    {'3', true, false, ""},   // # 无中文映射，交给应用
    {'4', true, true, "￥"},
    {'5', true, false, ""},   // %
    {'6', true, true, "…"},
    {'7', true, true, "—"},
    {'8', true, false, ""},   // *
    {'9', true, true, "（"},
    {'0', true, true, "）"},
    {VK_OEM_COMMA_, false, true, "，"},
    {VK_OEM_COMMA_, true, true, "《"},
    {VK_OEM_PERIOD_, false, true, "。"},
    {VK_OEM_PERIOD_, true, true, "》"},
    {VK_OEM_1_, false, true, "；"},
    {VK_OEM_1_, true, true, "："},
    {VK_OEM_2_, false, true, "／"},
    {VK_OEM_2_, true, true, "？"},
    {VK_OEM_4_, false, true, "【"},
    {VK_OEM_4_, true, true, "『"},
    {VK_OEM_6_, false, true, "】"},
    {VK_OEM_6_, true, true, "』"},
    {VK_OEM_5_, false, true, "、"},
    {VK_OEM_5_, true, true, "｜"},
    {VK_OEM_MINUS_, false, true, "－"},
    {VK_OEM_MINUS_, true, true, "—"},
    {VK_OEM_PLUS_, false, true, "＝"},
    {VK_OEM_PLUS_, true, true, "＋"},
    {VK_OEM_3_, false, false, ""},  // 反引号映射成自己，不能吞
    {VK_OEM_3_, true, true, "～"},
    {'A', false, true, ""},   // 字母起编码，不上屏
    {'A', true, false, ""},   // Shift+字母是大写，留给应用
};

void check(const Expect &e, Session &s) {
    s.reset();
    const bool take = s.wantsKey(e.vk, e.shift, false, false);
    if (take != e.take) {
        printf("FAIL wantsKey vk=0x%02X shift=%d 得到 %d 期望 %d\n", e.vk, e.shift ? 1 : 0, take ? 1 : 0,
               e.take ? 1 : 0);
        assert(false);
    }

    s.reset();
    const SessionOutcome o = s.processKey(e.vk, e.shift, false, false);
    if (std::string(o.commitText) != e.text) {
        printf("FAIL processKey vk=0x%02X shift=%d 得到 \"%s\" 期望 \"%s\"\n", e.vk, e.shift ? 1 : 0,
               o.commitText.c_str(), e.text);
        assert(false);
    }
    // 放行的键绝不能自己产出字符，否则上层既打字又上屏，重复一遍
    if (!take) assert(o.commitText.empty());
}

} // namespace

int main() {
    const bool oldCn = settings::instance().get_chinesePunc();
    const bool oldFull = settings::instance().get_fullWidthFlg();

    settings::instance().set_chinesePunc(true);
    settings::instance().set_fullWidthFlg(false);
    {
        Session s;
        for (const Expect &e : kCnTable) check(e, s);

        // 引号是成对交替的，单独测两轮
        s.reset();
        assert(s.wantsKey(VK_OEM_7_, true, false, false));
        assert(s.processKey(VK_OEM_7_, true, false, false).commitText == "\xE2\x80\x9C");  // “
        assert(s.processKey(VK_OEM_7_, true, false, false).commitText == "\xE2\x80\x9D");  // ”
        assert(s.processKey(VK_OEM_7_, false, false, false).commitText == "\xE2\x80\x98"); // ‘
        assert(s.processKey(VK_OEM_7_, false, false, false).commitText == "\xE2\x80\x99"); // ’
    }

    // 撞号键：这些键的虚拟键码和 ASCII 标点数值相同，绝不能被当成标点吃掉
    // （v11.2.3 初版就是把 VK_DELETE(0x2E) 当成 '.'，按 Delete 打出「。」）
    {
        Session s;
        const int notChars[] = {0x2E /*VK_DELETE*/, 0x2D /*VK_INSERT*/, 0x2C /*VK_SNAPSHOT*/,
                                0x5B /*VK_LWIN*/,   0x5C /*VK_RWIN*/,   0x5D /*VK_APPS*/,
                                0x60 /*VK_NUMPAD0*/, 0x24 /*VK_HOME*/,   0x23 /*VK_END*/};
        for (int vk : notChars) {
            assert(vkToCodeChar(vk, false) == 0);
            s.reset();
            assert(!s.wantsKey(vk, false, false, false));
            assert(s.processKey(vk, false, false, false).commitText.empty());
        }
    }

    // 全角模式：数字自己上屏，Shift+数字里没有中文映射的也变全角
    settings::instance().set_chinesePunc(false);
    settings::instance().set_fullWidthFlg(true);
    {
        Session s;
        s.reset();
        assert(s.wantsKey('1', false, false, false));
        assert(s.processKey('1', false, false, false).commitText == "１");

        s.reset();
        assert(s.wantsKey('3', true, false, false));
        assert(s.processKey('3', true, false, false).commitText == "＃");

        s.reset();
        assert(!s.wantsKey('A', true, false, false));
    }

    // 中文标点关闭、全角关闭：Shift+数字全部交给应用，符号键也不接管
    settings::instance().set_chinesePunc(false);
    settings::instance().set_fullWidthFlg(false);
    {
        Session s;
        for (int vk : {'1', '4', '6', '7', '9', '0'}) {
            assert(!s.wantsKey(vk, true, false, false));
            assert(s.processKey(vk, true, false, false).commitText.empty());
        }
        s.reset();
        assert(!s.wantsKey(VK_OEM_PERIOD_, false, false, false));
        // 组字中仍然全部由输入法接管（数字是候选选择键、符号要顶字上屏）
        s.reset();
        s.processKey('A', false, false, false);
        assert(s.wantsKey('1', false, false, false));
        assert(s.wantsKey(VK_OEM_PERIOD_, false, false, false));
    }

    settings::instance().set_chinesePunc(oldCn);
    settings::instance().set_fullWidthFlg(oldFull);

    std::puts("key gate test: PASS");
    return 0;
}
