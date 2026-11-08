// SPDX-License-Identifier: MulanPub-2.0
// Copyright (c) 2026 Charry
// Freeime11 is licensed under Mulan PubL v2.
// You can use this software according to the terms and conditions of the Mulan PubL v2.
// You may obtain a copy of Mulan PubL v2 at: http://license.coscl.org.cn/MulanPubL-2.0
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
// EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
// MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
// See the Mulan PubL v2 for more details.

// 自动上屏边界回归测试：
//   五笔       —— 满 4 码唯一要自动上屏
//   五笔拼音   —— 纯五笔 4 码仍自动上屏，但拼音整句不能被截断
//   拼音       —— 任何情况都不自动上屏（靠空格/数字确认）
#include "../src/session/session.h"

#include <cassert>
#include <cctype>
#include <cstdio>
#include <string>

using namespace freeime11;

namespace {

void switchTo(Session &s, const char *target) {
    for (int i = 0; i < 5 && s.engineName() != target; ++i) s.cycleMode();
    assert(s.engineName() == target);
}

struct Typed {
    std::string preedit;
    std::string committed;   // 整个过程中累计上屏的文本
    std::size_t lastCandCount = 0;
    std::string firstCand;
    bool committedMidway = false;
};

Typed typeCode(Session &s, const std::string &code) {
    Typed t;
    for (char ch : code) {
        SessionOutcome o = s.processKey(std::toupper(static_cast<unsigned char>(ch)), false, false, false);
        if (!o.commitText.empty()) {
            t.committed += o.commitText;
            t.committedMidway = true;
        }
        t.preedit = o.preedit;
        t.lastCandCount = o.cands.size();
        t.firstCand = o.cands.empty() ? std::string() : o.cands[0];
    }
    return t;
}

} // namespace

int main() {
    // 记下当前方案，测完还原（cycleMode 会把选择写回配置）
    std::string original;
    {
        Session probe;
        original = probe.engineName();
    }

    // 1) 五笔：满 4 码唯一 → 自动上屏（原有行为不能丢）
    {
        Session s;
        switchTo(s, "wbzx");
        s.reset();
        // aaad 是 4 码唯一（工期）；wqvb 有「你好/您好」两个候选，不该自动上屏
        Typed t = typeCode(s, "aaad");
        assert(t.committedMidway);
        assert(t.committed == "工期");
        assert(t.preedit.empty());
    }

    // 2) 五笔拼音：纯五笔 4 码仍自动上屏
    {
        Session s;
        switchTo(s, "wbpy");
        s.reset();
        Typed t = typeCode(s, "aaad");
        assert(t.committedMidway);
        assert(t.committed == "工期");
    }

    // 3) 五笔拼音：长拼音必须打完，中途不能被自动上屏截断
    {
        Session s;
        switchTo(s, "wbpy");
        s.reset();
        Typed t = typeCode(s, "woxihuanni");
        assert(!t.committedMidway);
        assert(t.preedit == "woxihuanni");
        assert(t.lastCandCount >= 1);
        assert(t.firstCand == "我喜欢你");
    }

    // 4) 拼音：同样不截断，且能给出整句
    {
        Session s;
        switchTo(s, "py");
        s.reset();
        Typed t = typeCode(s, "jintiantianqizhenhao");
        assert(!t.committedMidway);
        assert(t.preedit == "jintiantianqizhenhao");
        assert(t.firstCand == "今天天气真好");
    }

    // 5) 拼音：空格确认整句
    {
        Session s;
        switchTo(s, "py");
        s.reset();
        typeCode(s, "woxihuannima");
        SessionOutcome o = s.processKey(0x20 /*VK_SPACE*/, false, false, false);
        assert(o.commitText == "我喜欢你吗");
        assert(o.preedit.empty());
    }

    // 还原用户原本的输入方案
    {
        Session restore;
        for (int i = 0; i < 5 && restore.engineName() != original; ++i) restore.cycleMode();
        assert(restore.engineName() == original);
    }

    // 6) 组字中、当前没有候选时按数字：不能把数字打进文档
    {
        Session s;
        switchTo(s, "wbpy");
        s.reset();
        typeCode(s, "woxih");            // 此时无候选
        SessionOutcome o = s.processKey('1', false, false, false);
        assert(o.handled);
        assert(o.commitText.empty());
        assert(o.preedit == "woxih");
    }

    std::puts("autocommit test: PASS");
    return 0;
}
