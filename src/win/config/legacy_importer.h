// SPDX-License-Identifier: MulanPSL-2.0
// Copyright (c) 2026 沈阳蜂蒿文化传媒有限公司 (Charry)
// FreeIME 11 is licensed under Mulan PSL v2.
// You can use this software according to the terms and conditions of the Mulan PSL v2.
// You may obtain a copy of Mulan PSL v2 at: http://license.coscl.org.cn/MulanPSL2
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
// EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
// MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
// See the Mulan PSL v2 for more details.

// FreeIME 11 —— 10.8.9 旧版数据迁移
// 读取极点五笔 10.8.9 的 GBK INI（freeime.ini / default.ini / jidian.ini），
// 转换为 FreeIME 11 的 UTF-8 config.ini 配置键。
#pragma once

#include <string>
#include <vector>

namespace freeime11 {

struct LegacyConfigItem {
    std::string section;
    std::string key;
    std::string value;
};

// 解析旧版 GBK INI 文件为 (section,key,value) 列表。返回 false 表示无法读取。
bool parseLegacyIni(const std::string &gbkPath, std::vector<LegacyConfigItem> &out);

// 将解析出的旧配置映射到新配置并保存。返回被识别的键数量。
int applyLegacyConfig(const std::vector<LegacyConfigItem> &items);

// 迁移入口：迁移旧版配置目录（含 10.8.9 安装目录或用户目录）到新数据目录。
// 返回迁移的键数；返回 -1 表示源不存在。
int migrateLegacyData(const std::string &legacyDir);

} // namespace freeime11
