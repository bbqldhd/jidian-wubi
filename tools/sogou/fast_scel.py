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

"""搜狗 .scel 快速解析：单趟线性扫描 + 失败后跳到下一个记录标志位

记录 = [u16 flag=1][u16 codeLen][code][u16 wordLen][word UTF-16LE][u16 extraLen][extra]
code 为音节 ID 序列（u16 小端），extra 前 2 字节是词频。
"""
import struct, sys, os, time

def u16(d, p):
    return d[p] | (d[p+1] << 8)

def valid_text(b):
    if len(b) < 2 or len(b) % 2:
        return False
    try:
        s = b.decode('utf-16-le')
    except UnicodeDecodeError:
        return False
    if not s:
        return False
    for c in s:
        o = ord(c)
        if o < 0x80:
            continue
        if 0x4E00 <= o <= 0x9FFF or 0x3400 <= o <= 0x4DBF or 0x20000 <= o <= 0x2FFFF:
            continue
        if 0x2000 <= o <= 0x206F or 0x3000 <= o <= 0x303F or 0xFF00 <= o <= 0xFFEF:
            continue
        return False
    return True

def try_record(d, p, n):
    if p + 2 > n or d[p] != 1 or d[p+1] != 0:
        return None
    q = p + 2
    fields = []
    for idx in range(3):
        if q + 2 > n:
            return None
        ln = u16(d, q); q += 2
        if ln > 4096 or q + ln > n:
            return None
        if idx < 2 and (ln < 2 or ln % 2):
            return None
        if idx == 1 and not valid_text(d[q:q+ln]):
            return None
        fields.append(d[q:q+ln]); q += ln
    return fields, q

def decode_file(path):
    d = open(path, 'rb').read()
    n = len(d)
    recs = []
    p = 0
    while True:
        i = d.find(b'\x01\x00', p)
        if i < 0:
            break
        if i & 1:
            p = i + 1
            continue
        r = try_record(d, i, n)
        if r is None:
            p = i + 1
            continue
        fields, q = r
        recs.append(fields)
        p = q
    out = []
    for cb, wb, eb in recs:
        ids = [u16(cb, i) for i in range(0, len(cb), 2)]
        out.append((ids, wb.decode('utf-16-le'), u16(eb, 0) if len(eb) >= 2 else 0))
    return out

if __name__ == '__main__':
    tot = 0
    for path in sys.argv[1:]:
        t0 = time.time()
        recs = decode_file(path)
        dt = time.time() - t0
        tot += len(recs)
        print('%-26s %8d 条  %.2fs' % (os.path.basename(path), len(recs), dt))
        for ids, w, f in recs[:4]:
            print('     %-22s %-12s %d' % (' '.join('%04x' % i for i in ids), w, f))
    print('合计 %d 条' % tot)
