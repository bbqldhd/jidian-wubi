#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Inject the unified Mulan PubL v2 (木兰公共许可证第 2 版) notice into every source file.

The licence requires the statement to sit at the very top of each source file,
written in the comment syntax appropriate to that language.  This script is the
C++ / shell / Inno Setup counterpart of the same helper used by the author's
other projects, so all repositories carry byte-identical notices.

Usage:
    python tools/add_license_headers.py            # apply
    python tools/add_license_headers.py --check    # report only, no write
"""

import os
import re
import sys

# Project root = parent directory of this file's directory (tools/)
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

NOTICE = [
    "SPDX-License-Identifier: MulanPub-2.0",
    "Copyright (c) 2026 Charry",
    "Freeime11 is licensed under Mulan PubL v2.",
    "You can use this software according to the terms and conditions of the Mulan PubL v2.",
    "You may obtain a copy of Mulan PubL v2 at: http://license.coscl.org.cn/MulanPubL-2.0",
    'THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,',
    "EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,",
    "MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.",
    "See the Mulan PubL v2 for more details.",
]

# Directories that are vendored / generated and must keep their own headers.
SKIP_DIRS = {
    "build", ".git", ".workbuddy", "__pycache__",
    "third_party",          # spdlog etc. - upstream licences
    "extract", "re", "upstream-freewb",
}
# Vendored sources derived from FreeWB; they keep their original copyright.
SKIP_PATH_PREFIX = (
    os.path.join("src", "engine"),
    os.path.join("src", "utils"),
)

# Extension -> (comment prefix, block wrapper or None)
LINE_COMMENT = {
    ".cpp": "//", ".h": "//", ".hpp": "//", ".c": "//", ".rc": "//",
    ".py": "#", ".sh": "#", ".cmake": "#",
    ".iss": ";", ".def": ";", ".ini": ";",
    ".bat": "REM", ".cmd": "REM",
}
BLOCK_COMMENT = {".md": ("<!--", "-->")}


def line_header(prefix):
    return "".join("{} {}\n".format(prefix, n) for n in NOTICE) + "\n"


def block_header(open_tag, close_tag):
    body = "\n".join("  " + n for n in NOTICE)
    return "{}\n{}\n{}\n\n".format(open_tag, body, close_tag)


def build_header(path):
    ext = os.path.splitext(path)[1].lower()
    name = os.path.basename(path).lower()
    if name == "cmakelists.txt":
        return line_header("#")
    if ext in BLOCK_COMMENT:
        return block_header(*BLOCK_COMMENT[ext])
    if ext in LINE_COMMENT:
        return line_header(LINE_COMMENT[ext])
    return None


def strip_old_header(text):
    """Remove a previously written short SPDX header so it can be upgraded."""
    lines = text.splitlines(keepends=True)
    i = 0
    # keep a shebang / encoding line first
    while i < len(lines) and (lines[i].startswith("#!") or lines[i].startswith("# -*-")):
        i += 1
    if i >= len(lines):
        return text

    # 块注释形式（Markdown 等）：<!-- ... --> 整体删掉
    if lines[i].lstrip().startswith("<!--"):
        j = i
        while j < len(lines) and "-->" not in lines[j]:
            j += 1
        if j < len(lines):
            j += 1  # 连同结束标签一起
        while j < len(lines) and lines[j].strip() == "":
            j += 1
        return "".join(lines[:i] + lines[j:])

    first = lines[i].lstrip()
    if not first.startswith(("//", "#", ";", "REM ")):
        return text
    prefix = first[:2] if first.startswith(("//", "#;")) else first.split(" ")[0]
    if "SPDX-License-Identifier" not in lines[i]:
        return text
    j = i
    while j < len(lines):
        s = lines[j].strip()
        if s.startswith(prefix) and re.search(
            r"SPDX-License-Identifier|Copyright \(c\)|Mulan|木兰"
            r"|THIS SOFTWARE IS PROVIDED|EITHER EXPRESS|MERCHANTABILITY OR FIT", lines[j]
        ):
            j += 1
            continue
        break
    # drop the blank separator line(s) that followed the header
    while j < len(lines) and lines[j].strip() == "":
        j += 1
    return "".join(lines[:i] + lines[j:])


def apply_header(path, check_only):
    with open(path, "r", encoding="utf-8-sig") as f:
        text = f.read()

    header = build_header(path)
    if header is None:
        return ("SKIP", path, "unsupported type")
    if ("SPDX-License-Identifier: MulanPub-2.0" in text
            and "See the Mulan PubL v2 for more details." in text):
        return ("SKIP", path, "already licensed")

    stripped = strip_old_header(text)
    if stripped != text:
        note = "upgraded short SPDX header"
    else:
        note = "{}-line notice".format(len(NOTICE))

    # keep shebang / encoding directives at the very first line
    lines = stripped.splitlines(keepends=True)
    keep = 0
    while keep < len(lines) and (lines[keep].startswith("#!") or lines[keep].startswith("# -*-")):
        keep += 1
    head = "".join(lines[:keep])
    body = "".join(lines[keep:])

    new = head + header + body
    if not check_only:
        with open(path, "w", encoding="utf-8", newline="") as f:
            f.write(new)
    return ("ADD", path, note)


def main():
    check_only = "--check" in sys.argv
    report = []
    for dirpath, dirnames, filenames in os.walk(ROOT):
        dirnames[:] = sorted(d for d in dirnames if d not in SKIP_DIRS)
        rel_dir = os.path.relpath(dirpath, ROOT)
        if rel_dir != "." and any(
            rel_dir == p or rel_dir.startswith(p + os.sep) for p in SKIP_PATH_PREFIX
        ):
            continue
        for fn in filenames:
            path = os.path.join(dirpath, fn)
            rel = os.path.relpath(path, ROOT)
            if rel.replace("\\", "/") in ("LICENSE", "NOTICE"):
                continue
            report.append(apply_header(path, check_only))

    for action, path, note in report:
        if action == "SKIP":
            continue
        print("{:5s}  {:52s}  {}".format(action, os.path.relpath(path, ROOT), note))
    added = sum(1 for r in report if r[0] == "ADD")
    print("\n{} files licensed, {} skipped{}".format(
        added, len(report) - added, " (check only)" if check_only else ""))


if __name__ == "__main__":
    main()
