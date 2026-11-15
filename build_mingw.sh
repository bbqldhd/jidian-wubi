#!/usr/bin/env bash
# SPDX-License-Identifier: MulanPub-2.0
# Copyright (c) 2026 Charry
# Freeime11 is licensed under Mulan PubL v2.
# You can use this software according to the terms and conditions of the Mulan PubL v2.
# You may obtain a copy of Mulan PubL v2 at: http://license.coscl.org.cn/MulanPubL-2.0
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PubL v2 for more details.

#
# FreeIME 11 —— MinGW (Dev-C++) 一键构建脚本
#
# 用法：在 freeime11-win 目录下执行  bash build_mingw.sh
#      JOBS=16 bash build_mingw.sh    # 想要极限速度：拉满逻辑线程
#      JOBS=10 bash build_mingw.sh    # 边编译边干别的：留出余量
#
# 编译方式：每个 .cpp 单独编成 build/obj/*.o，多路并行；没有改动的源文件直接跳过，
# 只重编改动过的，最后再链接。全量构建和增量构建走同一套流程。
#
# 并发数默认取 **物理核心数**（i5-12500H 是 12），不是逻辑线程数（16）：
# 并行数超过物理核只会互抢缓存，而这些 TU 单个编译峰值能吃到几百 MB 内存，
# 16 个一起上在 16G 机器上容易换页，反而更慢。实测 16 线程 57.7s、12 线程 65.9s，
# 差的这 8 秒换来的是编译时机器还能正常用。
set -e

GPP="/c/Program Files (x86)/Dev-Cpp/MinGW64/bin/g++.exe"
WINDRES="/c/Program Files (x86)/Dev-Cpp/MinGW64/bin/windres.exe"
MGW="/c/Program Files (x86)/Dev-Cpp/MinGW64"
export PATH="$MGW/bin:$MGW/lib/gcc/x86_64-w64-mingw32/10.3.0:$PATH"

# 物理核心数（wmic 优先，退到 PowerShell，再退到逻辑核数）
physical_cores() {
    local n
    n=$(wmic cpu get NumberOfCores /value 2>/dev/null | tr -d '\r' | sed -n 's/^NumberOfCores=//p' | head -1)
    if [ -z "$n" ]; then
        n=$(powershell -NoProfile -Command "(Get-CimInstance Win32_Processor).NumberOfCores" 2>/dev/null |
            tr -d '\r' | head -1)
    fi
    case "$n" in
        '' | *[!0-9]*) n=$(nproc 2>/dev/null || echo 8) ;;
    esac
    echo "$n"
}

if [ -z "$JOBS" ]; then
    JOBS=$(physical_cores)
fi
[ "$JOBS" -gt 16 ] && JOBS=16
[ "$JOBS" -lt 1 ] && JOBS=1

mkdir -p build build/obj

# 安装向导用的纯文本许可证（UTF-8 BOM，Inno Setup 才会按 UTF-8 解析中文）
printf '\xef\xbb\xbf' > build/License.txt
sed 's/^[[:space:]]*//' LICENSE >> build/License.txt

COMMON="-std=c++17 -O2 -Isrc/utils -Isrc/engine -Isrc/session -Ithird_party"
CORE_SRCS="src/utils/log.cpp src/utils/settings.cpp src/utils/utils.cpp \
src/engine/wbzx.cpp src/engine/wbpy.cpp src/engine/py.cpp src/engine/en.cpp src/engine/common.cpp \
src/engine/autophrase.cpp src/engine/userdict.cpp src/engine/freqorder.cpp src/engine/gb2312filter.cpp \
src/session/session.cpp"
TSF_SRCS="src/win/tsf/FreeIME11Service.cpp src/win/tsf/dllmain.cpp \
src/win/tsf/CandidateWindow.cpp src/win/tsf/ToolbarWindow.cpp"

# 源文件 → 目标文件（路径里的斜杠换成下划线，避免建子目录）
obj_of() { echo "build/obj/$(echo "$1" | sed 's|/|_|g; s|\.cpp$|.o|')"; }

BUILT=0
SKIPPED=0

# 并行编译一组源文件：compile_group "<编译参数>" 源文件...
compile_group() {
    local flags="$1"
    shift
    local -a pids=()
    local src o st
    for src in "$@"; do
        o=$(obj_of "$src")
        if [ -f "$o" ] && [ "$o" -nt "$src" ]; then
            SKIPPED=$((SKIPPED + 1))
            continue
        fi
        # shellcheck disable=SC2086
        ( "$GPP" $flags -c "$src" -o "$o" ) &
        pids+=($!)
        BUILT=$((BUILT + 1))
        if [ "${#pids[@]}" -ge "$JOBS" ]; then
            st=0
            wait "${pids[0]}" || st=$?
            pids=("${pids[@]:1}")
            if [ "$st" -ne 0 ]; then
                echo "编译失败，见上面的错误输出" >&2
                exit "$st"
            fi
        fi
    done
    for src in "${pids[@]}"; do
        st=0
        wait "$src" || st=$?
        if [ "$st" -ne 0 ]; then
            echo "编译失败，见上面的错误输出" >&2
            exit "$st"
        fi
    done
}

objs_of() {
    local s
    for s in "$@"; do
        obj_of "$s"
    done
}

echo "[1/5] 并行编译（-j$JOBS）：核心 $(( $(echo $CORE_SRCS | wc -w) )) 个、TSF $(( $(echo $TSF_SRCS | wc -w) )) 个..."
# shellcheck disable=SC2086
compile_group "$COMMON" $CORE_SRCS
# shellcheck disable=SC2086
compile_group "$COMMON -DUNICODE -D_UNICODE -Isrc/win/tsf" $TSF_SRCS

echo "[2/5] 编译版本资源..."
"$WINDRES" -O coff --preprocessor="gcc -E -xc-header -DRC_INVOKED" installer/version.rc -o build/version.o
# 设置程序的清单（Common Controls 6 视觉样式 + Per-Monitor V2 DPI）
"$WINDRES" -O coff --preprocessor="gcc -E -xc-header -DRC_INVOKED" \
  src/win/settings_app/settings.rc -o build/obj/settings_res.o

# 链接期公共参数
LINK_CORE="$(objs_of $(echo $CORE_SRCS))"
LINK_TSF="$(objs_of $(echo $TSF_SRCS))"

echo "[3/5] 链接 TSF 输入法 DLL..."
# shellcheck disable=SC2086
"$GPP" -shared -static $LINK_CORE $LINK_TSF build/version.o \
  -liconv -lole32 -ladvapi32 -luuid -lshell32 -lgdi32 -lgdiplus \
  src/win/tsf/FreeIME11.def -o build/FreeIME11.dll

echo "[4/5] 编译并运行自测..."
# 测试源文件也走并行
TEST_SRCS="tests/session_test.cpp tests/autocommit_test.cpp tests/pinyin_sentence_test.cpp \
tests/pinyin_fuzzy_test.cpp tests/punc_test.cpp tests/keygate_test.cpp tests/settings_test.cpp"
# shellcheck disable=SC2086
compile_group "$COMMON" $TEST_SRCS
compile_group "$COMMON -DUNICODE -D_UNICODE -Isrc/win/tsf" tests/candidate_window_test.cpp
compile_group "$COMMON -DUNICODE -D_UNICODE -Isrc/win/tsf" tests/toolbar_window_test.cpp

link_test() {   # link_test 名称 额外对象/库
    local name="$1"
    shift
    # shellcheck disable=SC2086
    "$GPP" $LINK_CORE "$@" -liconv -o "build/$name.exe"
}

link_test session_test           "$(obj_of tests/session_test.cpp)"
link_test autocommit_test        "$(obj_of tests/autocommit_test.cpp)"
link_test punc_test              "$(obj_of tests/punc_test.cpp)"
link_test keygate_test           "$(obj_of tests/keygate_test.cpp)"
link_test pinyin_sentence_test   "$(obj_of tests/pinyin_sentence_test.cpp)"
link_test pinyin_fuzzy_test      "$(obj_of tests/pinyin_fuzzy_test.cpp)"
# settings_test 要用独立的临时数据目录，不能碰用户真实配置
link_test settings_test          "$(obj_of tests/settings_test.cpp)"
"$GPP" "$(obj_of tests/candidate_window_test.cpp)" "$(obj_of src/win/tsf/CandidateWindow.cpp)" \
  -lgdiplus -lgdi32 -luser32 -static -o build/candidate_window_test.exe
"$GPP" "$(obj_of tests/toolbar_window_test.cpp)" "$(obj_of src/win/tsf/ToolbarWindow.cpp)" \
  -lgdiplus -lgdi32 -luser32 -ladvapi32 -static -o build/toolbar_window_test.exe

for t in session_test autocommit_test pinyin_sentence_test pinyin_fuzzy_test punc_test keygate_test settings_test; do
    "./build/$t.exe"
done

echo "[5/5] 编译工具程序..."
compile_group "$COMMON -DUNICODE -D_UNICODE -municode -Isrc/win/config" \
  src/win/config/legacy_importer.cpp src/win/migrate/main.cpp
compile_group "$COMMON -DUNICODE -D_UNICODE -municode" src/win/settings_app/main.cpp
compile_group "$COMMON -DUNICODE -D_UNICODE -municode" src/win/uninstall/main.cpp
compile_group "-std=c++17 -O2 -DUNICODE -D_UNICODE -municode" src/win/addlang/main.cpp

# shellcheck disable=SC2086
"$GPP" $LINK_CORE -municode "$(obj_of src/win/config/legacy_importer.cpp)" "$(obj_of src/win/migrate/main.cpp)" \
  -liconv -static -o build/freeime11-migrate.exe
# shellcheck disable=SC2086
"$GPP" $LINK_CORE -municode "$(obj_of src/win/settings_app/main.cpp)" build/obj/settings_res.o \
  -liconv -lshell32 -luser32 -lcomctl32 -lgdi32 -static -o build/freeime11-settings.exe
# shellcheck disable=SC2086
"$GPP" -municode "$(obj_of src/win/uninstall/main.cpp)" -lshell32 -luser32 -static -o build/freeime11-uninstall.exe
# shellcheck disable=SC2086
"$GPP" -municode "$(obj_of src/win/addlang/main.cpp)" -lole32 -luuid -ladvapi32 -static -o build/freeime11-addlang.exe

echo ""
echo "构建完成（编译 $BUILT 个，跳过 $SKIPPED 个未改动）："
ls -la build/*.exe build/*.dll 2>/dev/null
