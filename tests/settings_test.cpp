// SPDX-License-Identifier: MulanPub-2.0
// Copyright (c) 2026 Charry
// Freeime11 is licensed under Mulan PubL v2.
// You can use this software according to the terms and conditions of the Mulan PubL v2.
// You may obtain a copy of Mulan PubL v2 at: http://license.coscl.org.cn/MulanPubL-2.0
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
// EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
// MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
// See the Mulan PubL v2 for more details.

// 配置通用读写自测：设置 GUI 依赖 setByKey/getByKey 正确覆盖 Bool/Int/String。
// 用 FREEWB_DATA_DIR 指到临时目录，绝不碰用户真实配置。
#include "../src/utils/settings.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>

int main() {
    const char *tmp = std::getenv("TEMP");
    const std::string dir = std::string(tmp ? tmp : ".") + "/freeime11-settings-test";
    _putenv_s("FREEWB_DATA_DIR", dir.c_str());

    auto &s = settings::instance();

    // 元数据来自单一数据源，供 GUI 分组与默认值展示
    size_t n = 0;
    const settings::ConfigMeta *metas = settings::allConfigMeta(&n);
    assert(n > 60);
    bool sawBool = false, sawInt = false, sawString = false, sawDesc = false;
    for (size_t i = 0; i < n; ++i) {
        assert(metas[i].key && *metas[i].key);
        assert(metas[i].category && *metas[i].category);
        assert(metas[i].defaultValue);
        if (metas[i].type == settings::ValueType::Bool) sawBool = true;
        if (metas[i].type == settings::ValueType::Int) sawInt = true;
        if (metas[i].type == settings::ValueType::String) sawString = true;
        if (metas[i].description && *metas[i].description) sawDesc = true;
    }
    assert(sawBool && sawInt && sawString && sawDesc);

    // Bool：以前 setByKey 只写 stringValue，保存时读 boolValue，等于改不动
    assert(s.setByKey("chinesePunc", "false"));
    assert(s.getByKey("chinesePunc") == "false");
    assert(!s.get_chinesePunc());
    assert(s.setByKey("chinesePunc", "true"));
    assert(s.getByKey("chinesePunc") == "true");
    assert(s.get_chinesePunc());

    // Int：同理，必须写进 intValue
    assert(s.setByKey("candiWordCount", "7"));
    assert(s.getByKey("candiWordCount") == "7");
    assert(s.get_candiWordCount() == 7);
    assert(!s.setByKey("candiWordCount", "not-a-number"));

    // String
    assert(s.setByKey("candiTextFontName", "Microsoft YaHei UI"));
    assert(s.getByKey("candiTextFontName") == "Microsoft YaHei UI");
    assert(s.get_candiTextFontName() == "Microsoft YaHei UI");

    // 未知键拒绝
    assert(!s.setByKey("no_such_option", "x"));
    assert(s.getByKey("no_such_option").empty());

    // 保存后重新载入，三类值都要留得住（证明真的写进了 ini）
    assert(s.save());
    s.reload();
    assert(s.getByKey("chinesePunc") == "true");
    assert(s.getByKey("candiWordCount") == "7");
    assert(s.getByKey("candiTextFontName") == "Microsoft YaHei UI");

    std::puts("settings test: PASS");
    return 0;
}
