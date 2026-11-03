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

"""从搜狗词库站抓取 .scel 细胞词库"""
import re, io, os, sys, urllib.parse, urllib.request

UA = {'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64)'}
OUT = 'scel'

def get(url, timeout=60):
    # 查询串里的中文要百分号编码
    if '?' in url:
        base, query = url.split('?', 1)
        parts = []
        for kv in query.split('&'):
            if '=' in kv:
                k, v = kv.split('=', 1)
                parts.append(k + '=' + urllib.parse.quote(v, safe=''))
            else:
                parts.append(kv)
        url = base + '?' + '&'.join(parts)
    req = urllib.request.Request(url, headers=UA)
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return r.read()

def cell_links(detail_id):
    html = get('https://pinyin.sogou.com/dict/detail/index/%d' % detail_id).decode('utf-8', 'replace')
    out = []
    for m in re.finditer(r'href="([^"]*download_cell\.php\?[^"]*)"[^>]*>([^<]{0,60})', html):
        u, name = m.group(1), m.group(2).strip()
        if u.startswith('//'):
            u = 'https:' + u
        elif u.startswith('/'):
            u = 'https://pinyin.sogou.com' + u
        out.append((u, name))
    return out

def main(ids):
    os.makedirs(OUT, exist_ok=True)
    for cid in ids:
        try:
            links = cell_links(cid)
        except Exception as e:
            print('  id=%s 详情页失败: %s' % (cid, e)); continue
        # 取该 id 自身的下载链接
        target = None
        for u, name in links:
            if 'id=%d&' % cid in u:
                target = (u, name); break
        if not target:
            print('  id=%s 详情页无自身下载链接' % cid); continue
        u, name = target
        fn = os.path.join(OUT, 'c%s.scel' % cid)
        try:
            data = get(u)
        except Exception as e:
            print('  id=%s 下载失败: %s' % (cid, e)); continue
        open(fn, 'wb').write(data)
        print('  id=%-7s %-22s %8d bytes -> %s' % (cid, name[:22], len(data), fn))

if __name__ == '__main__':
    main([int(x) for x in sys.argv[1:]])
