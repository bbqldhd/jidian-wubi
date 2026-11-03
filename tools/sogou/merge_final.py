# -*- coding: utf-8 -*-
# SPDX-License-Identifier: MulanPSL-2.0
# Copyright (c) 2026 Charry
# Freeime11 is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at: http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

"""把搜狗 .scel 词库并入 FreeIME11 拼音词库（pinyin.mb）

规则：
- 只用「字数 == 音节数」且全为汉字的词条（正好匹配引擎整句切分的约束）
- 编码长度 <= 32（与引擎 kMaxWordCodeLen 一致）
- 同编码内：原有词条保持原顺序在前，新词按搜狗词频降序追加
"""
import sys, os, json, time, collections
import fast_scel, mb_read

MAX_CODE = 32

def is_cjk_word(w):
    if not w:
        return False
    for c in w:
        o = ord(c)
        if not (0x4E00 <= o <= 0x9FFF or 0x3400 <= o <= 0x4DBF or 0x20000 <= o <= 0x2FFFF):
            return False
    return True

def main(mbpath, celldir, outpath):
    t0 = time.time()
    syl = {int(k, 16): v for k, v in json.load(open('syl_map.json')).items()}
    base = mb_read.read_mb(mbpath)
    base_recs = base['records']
    print('原词库 %d 条  (%.1fs)' % (len(base_recs), time.time() - t0))

    groups = collections.OrderedDict()
    for code, text in base_recs:
        groups.setdefault(code, []).append(text)
    have = set(base_recs)

    # 网络新词词库里全是人名/地名/公司名，混进候选是纯噪声，跳过
    SKIP = {'c4.scel', 'netnew.scel'}
    stat = dict(total=0, dec=0, kept=0, dup=0, tooLong=0, notCjk=0, badLen=0)
    addmap = collections.defaultdict(list)
    for fn in sorted(f for f in os.listdir(celldir) if f.endswith('.scel')):
        if fn in SKIP:
            print('  %-16s 跳过（低质量词库）' % fn)
            continue
        recs = fast_scel.decode_file(os.path.join(celldir, fn))
        n_add = 0
        for ids, word, freq in recs:
            stat['total'] += 1
            code = []
            okc = True
            for sid in ids:
                s = syl.get(sid)
                if s is None:
                    okc = False; break
                code.append(s)
            if not okc:
                continue
            stat['dec'] += 1
            if len(word) != len(ids):
                stat['badLen'] += 1; continue
            if not is_cjk_word(word):
                stat['notCjk'] += 1; continue
            c = ''.join(code)
            if len(c) > MAX_CODE:
                stat['tooLong'] += 1; continue
            if (c, word) in have:
                stat['dup'] += 1; continue
            have.add((c, word))
            addmap[c].append((freq, word))
            n_add += 1
        print('  %-16s 新增 %5d  (%.1fs)' % (fn, n_add, time.time() - t0))

    print('统计: 总 %d / 可解码 %d / 保留 %d（重复 %d，超长 %d，非汉字 %d，字数不符 %d）'
          % (stat['total'], stat['dec'], sum(len(v) for v in addmap.values()),
             stat['dup'], stat['tooLong'], stat['notCjk'], stat['badLen']))

    for code, lst in addmap.items():
        lst.sort(key=lambda x: (-x[0], x[1]))
        groups.setdefault(code, []).extend(w for _, w in lst)

    records = []
    for code in sorted(groups):
        for w in groups[code]:
            records.append((code, w))
    print('合并后 %d 条 / %d 个编码  (%.1fs)' % (len(records), len(groups), time.time() - t0))

    meta = dict(base)
    meta['info'] = '极点五笔拼音词库 + 搜狗官方细胞词库（Freeime11）'
    mb_read.write_mb(outpath, meta, records)
    print('已写出 %s  %.2f MB  (%.1fs)'
          % (outpath, os.path.getsize(outpath) / 1048576.0, time.time() - t0))

if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2], sys.argv[3])
