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

"""解出搜狗音节 ID 表：加权最长非降子序列定锚点 + 区间插值 + 回验"""
import sys, os, json, time, collections
import fast_scel, mb_read
from syllables import SYLS, splits

def build_votes(mbpath, celldir):
    m = mb_read.read_mb(mbpath)
    text2codes = collections.defaultdict(list)
    for code, text in m['records']:
        text2codes[text].append(code)
    recs = []
    for fn in sorted(f for f in os.listdir(celldir) if f.endswith('.scel')):
        recs.extend(fast_scel.decode_file(os.path.join(celldir, fn)))
    votes = collections.defaultdict(collections.Counter)
    for ids, word, freq in recs:
        if len(word) != len(ids) or not ids:
            continue
        for code in text2codes.get(word, ()):
            sp = splits(code, len(ids))
            if len(sp) == 1:
                for i, sid in enumerate(ids):
                    votes[sid][sp[0][i]] += 1
            break
    return recs, text2codes, votes

def main(mbpath, celldir):
    t0 = time.time()
    recs, text2codes, votes = build_votes(mbpath, celldir)
    order = {s: i for i, s in enumerate(SYLS)}
    ids = sorted(votes)
    obs = [(order[votes[i].most_common(1)[0][0]], votes[i].most_common(1)[0][1]) for i in ids]
    print('scel 记录 %d，投票 ID %d 个（%04x..%04x）  (%.1fs)'
          % (len(recs), len(ids), ids[0], ids[-1], time.time() - t0))

    n = len(ids)
    dp = [0.0] * n
    pre = [-1] * n
    for a in range(n):
        dp[a] = obs[a][1]
        for b in range(a):
            if obs[b][0] < obs[a][0] and dp[b] + obs[a][1] > dp[a]:
                dp[a] = dp[b] + obs[a][1]
                pre[a] = b
    best = max(range(n), key=lambda k: dp[k])
    anchors = []
    k = best
    while k != -1:
        anchors.append(k)
        k = pre[k]
    anchors.reverse()
    print('锚点 %d 个（覆盖 %d/%d 个 ID 的票数），总权重 %.0f'
          % (len(anchors), sum(obs[a][1] for a in anchors), sum(o[1] for o in obs), sum(o[1] for o in obs)))

    table = {}
    for a in anchors:
        table[ids[a]] = SYLS[obs[a][0]]
    # 区间插值：相邻锚点之间，若 ID 间距 == 音节间距则逐一填入
    filled = 0
    for x in range(len(anchors) - 1):
        a, b = anchors[x], anchors[x + 1]
        gap_ids = ids[b] - ids[a] - 1
        gap_syl = obs[b][0] - obs[a][0] - 1
        if gap_ids == gap_syl and gap_ids >= 0:
            for t in range(1, gap_ids + 1):
                table[ids[a] + t] = SYLS[obs[a][0] + t]
                filled += 1
        elif gap_ids > 0:
            # 用区间内最好票数补位，仍保证严格递增
            lo, hi = obs[a][0], obs[b][0]
            for t in range(1, gap_ids + 1):
                sid = ids[a] + t
                c = votes.get(sid)
                if not c:
                    continue
                for syl, cnt in c.most_common():
                    j = order[syl]
                    if lo < j < hi and all(order[v] != j for v in table.values()):
                        table[sid] = syl
                        filled += 1
                        break
    # 首尾锚点之外：按恒定偏移外推
    first, last = anchors[0], anchors[-1]
    off = ids[first] - obs[first][0]
    for sid in ids[:first]:
        j = sid - off
        if 0 <= j < len(SYLS):
            cand = votes[sid].most_common(1)[0][0]
            table[sid] = cand if order[cand] == j else SYLS[j]
    off2 = ids[last] - obs[last][0]
    for sid in ids[last + 1:]:
        j = sid - off2
        if 0 <= j < len(SYLS):
            cand = votes[sid].most_common(1)[0][0]
            table[sid] = cand if order[cand] == j else SYLS[j]
    print('插值补入 %d 个 ID，最终映射 %d 个  (%.1fs)' % (filled, len(table), time.time() - t0))

    seq = sorted(table.items())
    bad = [(seq[k-1], seq[k]) for k in range(1, len(seq)) if order[seq[k][1]] <= order[seq[k-1][1]]]
    print('不合单调的相邻对: %d %s' % (len(bad), ['%04x(%s)->%04x(%s)' % (a[0], a[1], b[0], b[1]) for a, b in bad[:5]]))
    diff = [(i, votes[i].most_common(1)[0][0], table[i]) for i in ids if i in table and table[i] != votes[i].most_common(1)[0][0]]
    print('与多数票不同的 ID: %d %s' % (len(diff), [('%04x' % i, m, t, votes[i][m], votes[i][t]) for i, m, t in diff[:6]]))

    # 回验
    ok = badc = 0
    samples = []
    for ids_, word, freq in recs:
        codes = text2codes.get(word)
        if not codes:
            continue
        out = []
        for sid in ids_:
            s = table.get(sid)
            if s is None:
                out = None; break
            out.append(s)
        if out is None:
            continue
        code = ''.join(out)
        if code in codes:
            ok += 1
        else:
            badc += 1
            if len(samples) < 8:
                samples.append((word, code, codes[:3]))
    print('回验（词库已有词）: 命中 %d / 不一致 %d → %.3f%%'
          % (ok, badc, 100.0 * ok / max(1, ok + badc)))
    for w, c, alt in samples:
        print('   %s -> %s  (应为 %s)' % (w, c, alt))

    json.dump({('%04x' % k): v for k, v in seq}, open('syl_map.json', 'w'), ensure_ascii=False, indent=0)
    print('已写 syl_map.json (%d 项)  (%.1fs)' % (len(table), time.time() - t0))

if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
