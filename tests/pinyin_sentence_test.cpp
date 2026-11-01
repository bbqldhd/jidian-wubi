// SPDX-License-Identifier: MulanPSL-2.0
// Copyright (c) 2026 Charry
// Freeime11 is licensed under Mulan PSL v2.
// You can use this software according to the terms and conditions of the Mulan PSL v2.
// You may obtain a copy of Mulan PSL v2 at: http://license.coscl.org.cn/MulanPSL2
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
// EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
// MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
// See the Mulan PSL v2 for more details.

// 拼音整句（智能组句）回归测试：
//   1) 长拼音串能出整句，且不再是 0 候选
//   2) 词库里有整词时首选不变、条数不变（不引入回归）
//   3) 非法拼音串不会被硬切成乱码
//   4) 超长输入的处理耗时可接受
#include "../src/engine/common.h"
#include "../src/engine/py.h"

#include <cassert>
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

using namespace freewb;

namespace {

const std::vector<std::string> &typeAll(PyEngine &e, const std::string &code) {
    e.reset();
    std::string buf;
    for (char c : code) {
        buf.push_back(c);
        e.putKey(buf.c_str());
    }
    return e.getResult().texts;
}

bool contains(const std::vector<std::string> &v, const char *s) {
    for (const auto &x : v) {
        if (x == s) return true;
    }
    return false;
}

std::size_t chars(const std::string &s) { return MbDictionaryTable::utf8CharCount(s); }

} // namespace

int main() {
    PyEngine engine;

    // 1) 整串不在词库里时，按音节组句
    {
        const auto &r = typeAll(engine, "woxihuanni");
        assert(!r.empty());
        assert(r[0] == "我喜欢你");

        const auto &r2 = typeAll(engine, "jintiantianqizhenhao");
        assert(!r2.empty());
        assert(r2[0] == "今天天气真好");
        assert(chars(r2[0]) >= 6);   // 远超词库最长词条 11 字以内的常见长度

        const auto &r3 = typeAll(engine, "woxihuannima");
        assert(!r3.empty());
        assert(r3[0] == "我喜欢你吗");
    }

    // 2) 词库里有整词：首选与条数保持原样，不被整句抢位
    {
        const auto &r = typeAll(engine, "nihao");
        assert(r.size() == 1);
        assert(r[0] == "你好");

        const auto &r2 = typeAll(engine, "zhonghuarenmingongheguo");
        assert(!r2.empty());
        assert(r2[0] == "中华人民共和国");
    }

    // 3) 打字过程中的前缀匹配不受影响（整句只能排在已有的后面）
    {
        const auto &r = typeAll(engine, "niha");
        assert(r.size() >= 1);
        assert(r[0] == "你好");
    }

    // 4) 非法拼音串不产生整句（不能把 abc 硬切成乱码）
    {
        const auto &r = typeAll(engine, "abc");
        assert(r.empty());
        const auto &r2 = typeAll(engine, "zzzzzzzz");
        assert(r2.empty());
    }

    // 5) 超长输入：能出结果，且单次按键不会明显卡顿
    {
        const std::string longCode = "womendedajiazhoujintianqulejintianqulegongyuanwan";  // 48 字母
        const auto start = std::chrono::steady_clock::now();
        const auto &r = typeAll(engine, longCode);
        const auto cost = std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::steady_clock::now() - start)
                              .count();
        printf("48 字母输入：cands=%zu 首条=%s 耗时=%lldms（含逐键重算）\n", r.size(),
               r.empty() ? "(无)" : r[0].c_str(), static_cast<long long>(cost));
        assert(!r.empty());
        assert(chars(r[0]) >= 10);
        assert(cost < 2000);
    }

    std::puts("pinyin sentence test: PASS");
    return 0;
}
