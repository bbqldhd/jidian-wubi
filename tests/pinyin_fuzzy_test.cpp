// SPDX-License-Identifier: MulanPub-2.0
// Copyright (c) 2026 Charry
// Freeime11 is licensed under Mulan PubL v2.
// You can use this software according to the terms and conditions of the Mulan PubL v2.
// You may obtain a copy of Mulan PubL v2 at: http://license.coscl.org.cn/MulanPubL-2.0
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
// EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
// MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
// See the Mulan PubL v2 for more details.

// 拼音智能纠错（模糊音）回归测试：
//   1) 少打 h：zongguo → 中国（整串没命中词条时才纠错）
//   2) 长串里错一个音节：zonghuarenmingongheguo → 中华人民共和国
//   3) 正确输入不受影响：候选条数与首选都不变
//   4) 关掉开关（pinyinFuzzy=false）就不纠错
//   5) 非法串不会被纠错成乱码
#include "../src/engine/common.h"
#include "../src/engine/py.h"
#include "../src/utils/settings.h"

#include <cassert>
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

void dump(const char *code, const std::vector<std::string> &v) {
    printf("%-26s → ", code);
    for (std::size_t i = 0; i < v.size() && i < 5; ++i) printf("%s%s", i ? " | " : "", v[i].c_str());
    if (v.empty()) printf("(空)");
    printf("  共 %zu 条\n", v.size());
}

} // namespace

int main() {
    const bool oldFuzzy = settings::instance().get_pinyinFuzzy();
    settings::instance().set_pinyinFuzzy(true);

    PyEngine engine;

    // 1) 少打一个 h
    {
        const auto &r = typeAll(engine, "zongguo");
        dump("zongguo", r);
        assert(contains(r, "中国"));
        assert(r[0] == "中国");   // 整串没有词条命中，纠错结果直接顶到第一位
    }

    // 2) 长串里错一个音节
    {
        const auto &r = typeAll(engine, "zonghuarenmingongheguo");
        dump("zonghuarenmingongheguo", r);
        assert(!r.empty());
        assert(r[0] == "中华人民共和国");
    }

    // 3) 正确输入：条数与首选保持原样（不被纠错搅乱）
    {
        const auto &r = typeAll(engine, "zhongguo");
        dump("zhongguo", r);
        assert(!r.empty());
        assert(r[0] == "中国");

        const auto &r2 = typeAll(engine, "nihao");
        dump("nihao", r2);
        assert(r2.size() == 1);
        assert(r2[0] == "你好");
    }

    // 4) 关掉开关：不再纠错
    {
        settings::instance().set_pinyinFuzzy(false);
        PyEngine off;
        const auto &r = typeAll(off, "zongguo");
        dump("zongguo(fuzzy off)", r);
        assert(!contains(r, "中国"));
        settings::instance().set_pinyinFuzzy(true);
    }

    // 5) 非法串保持不产候选
    {
        const auto &r = typeAll(engine, "abc");
        assert(r.empty());
    }

    settings::instance().set_pinyinFuzzy(oldFuzzy);
    std::puts("pinyin fuzzy test: PASS");
    return 0;
}
