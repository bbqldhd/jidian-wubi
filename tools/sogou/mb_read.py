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

"""FreeIME11 .mb 词库读取/写入（格式见 src/engine/common.cpp）"""
import struct

def read_field(d, p):
    (n,) = struct.unpack_from('<I', d, p); p += 4
    return d[p:p+n].decode('utf-8', 'replace'), p + n + 1   # 末尾带 NUL

def read_mb(path):
    d = open(path, 'rb').read()
    p = 0
    fields = []
    for _ in range(7):
        s, p = read_field(d, p)
        fields.append(s)
    name, info, ctime, endkeys, special, codetype, straight = fields
    (inLen,) = struct.unpack_from('<I', d, p); p += 4
    inputcode = d[p:p+inLen].split(b'\x00')[0].decode('utf-8','replace'); p += inLen + 1
    wild = d[p]; p += 1
    hasrule = d[p]; p += 1
    if hasrule:
        for _ in range(3):
            p += 2 + 4*3   # iFlag+iWords + 4 cells × 3 bytes
    (cnt,) = struct.unpack_from('<I', d, p); p += 4
    recs = []
    for _ in range(cnt):
        (cl,) = struct.unpack_from('<I', d, p); p += 4
        code = d[p:p+cl-1].decode('utf-8','replace'); p += cl
        (tl,) = struct.unpack_from('<I', d, p); p += 4
        text = d[p:p+tl-1].decode('utf-8','replace'); p += tl
        p += 1  # type
        recs.append((code, text))
    return dict(name=name, info=info, ctime=ctime, endkeys=endkeys, special=special,
                codetype=codetype, straight=straight, inputcode=inputcode, wild=wild,
                hasrule=hasrule, records=recs)

def write_mb(path, meta, records):
    """records: [(code, text)]，必须按 code 递增"""
    with open(path, 'wb') as f:
        for k in ('name','info','ctime','endkeys','special','codetype','straight'):
            b = meta[k].encode('utf-8')
            f.write(struct.pack('<I', len(b))); f.write(b + b'\x00')
        ic = meta.get('inputcode','abc').encode('utf-8')
        f.write(struct.pack('<I', len(ic)))
        f.write(ic + b'\x00')
        f.write(bytes([meta.get('wild', ord('*'))]))
        f.write(bytes([0]))   # hasrule = 0
        f.write(struct.pack('<I', len(records)))
        for code, text in records:
            cb = code.encode('utf-8'); tb = text.encode('utf-8')
            f.write(struct.pack('<I', len(cb)+1)); f.write(cb + b'\x00')
            f.write(struct.pack('<I', len(tb)+1)); f.write(tb + b'\x00')
            f.write(bytes([0]))

if __name__ == '__main__':
    import sys
    m = read_mb(sys.argv[1])
    print('name=%r info=%r codetype=%r inputcode=%r' % (m['name'], m['info'], m['codetype'], m['inputcode']))
    print('记录数:', len(m['records']))
    for r in m['records'][:8]: print('   ', r)
