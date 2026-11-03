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

# 搜狗词库 → 拼音词库 转换工具

把搜狗 `.scel` 细胞词库解析出来，转成极点五笔 11 的 `pinyin.mb` 词库格式。

## 用法

```bash
# 1) 抓词库（也可手动把 .scel 放进一个目录）
python fetch_cells.py 1 2 3 15097 15117 15203

# 2) 解音节表：需要现有 pinyin.mb 作为对齐语料
python solve_sylmap.py ../../../upstream-freewb/data/mb/default/pinyin.mb ./scel

# 3) 合并进词库
python merge_final.py ../../../upstream-freewb/data/mb/default/pinyin.mb ./scel pinyin.new.mb
```

## .scel 格式（逆向结果）

```
记录区 = 重复 N 次的：
  u16  flag = 1
  u16  codeLen
  code 字节        音节 ID 序列（u16 小端，一个音节一个 ID）
  u16  wordLen
  word 字节        词条，UTF-16LE
  u16  extraLen
  extra 字节       前 2 字节为词频
```

几点容易踩的坑：

- **音节 ID 不是拼音字符串**，而是「按拼音字母序排的音节表」里的下标。搜狗那张表比常见的
  411 音节表多两个（`ng`、`fiao` 这类），所以不能直接按下标查，必须对齐。
- 记录区在文件里**分成多组**，组与组之间有索引；用一个「单趟扫描 + 失败就跳到下一个
  `01 00`」的解析器最稳（`fast_scel.py`）。
- 判断记录是否合法要校验词条是**合法 UTF-16 中文**，否则会把索引区误当记录。

## 音节表怎么解出来的

`code` 里的 ID 必须满足「ID 递增 → 音节字母序递增」。于是：

1. 用词库中「字数 == 音节数」的词条，配合 `pinyin.mb` 里同词的拼音码（切成同样多的音节）
   给每个 ID 投票；
2. 只保留切分唯一的投票，按票数加权做**最长非降子序列**定锚点；
3. 相邻锚点之间若 ID 间距与音节间距相等，逐一插值。

结果写在 `syl_map.json`（407 项，严格单调）。用它解码 20 万条记录，与现有词库中已有词的
读音一致率 **99.71%**，剩下 0.29% 是多音字（`攒三聚五` 的「攒」、「大模大样」的「模」），
搜狗给的读音反而更贴合词义。

## 词条取舍

- 只保留**全为汉字**且**字数 == 音节数**的词条 —— 正好匹配引擎整句切分的约束；
- 编码长度 ≤ 32（与 `kMaxWordCodeLen` 一致）；
- 默认跳过 `c4/netnew`（网络新词）词库：里面全是人名、地名、公司名，混进候选是纯噪声。

## 版权提醒

`.scel` 词库由搜狗以「供搜狗输入法使用」的形式分发，其内容版权归属原始贡献者。
把转换后的词条并入本项目再对外分发，属于**授权不明确**的用法；
自用没问题，公开发布前请自行评估。
