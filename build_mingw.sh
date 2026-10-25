#!/usr/bin/env bash
# FreeIME 11 —— MinGW (Dev-C++) 一键构建脚本
# 用法：在 freeime11-win 目录下执行  bash build_mingw.sh
set -e

GPP="/c/Program Files (x86)/Dev-Cpp/MinGW64/bin/g++.exe"
WINDRES="/c/Program Files (x86)/Dev-Cpp/MinGW64/bin/windres.exe"
MGW="/c/Program Files (x86)/Dev-Cpp/MinGW64"
export PATH="$MGW/bin:$MGW/lib/gcc/x86_64-w64-mingw32/10.3.0:$PATH"
mkdir -p build

COMMON="-std=c++17 -O2 -Isrc/utils -Isrc/engine -Isrc/session -Ithird_party"
UTILS="src/utils/log.cpp src/utils/settings.cpp src/utils/utils.cpp"
ENGINE="src/engine/wbzx.cpp src/engine/wbpy.cpp src/engine/py.cpp src/engine/en.cpp src/engine/common.cpp src/engine/autophrase.cpp src/engine/userdict.cpp src/engine/freqorder.cpp src/engine/gb2312filter.cpp"

echo "[1/5] 编译会话冒烟测试..."
"$GPP" $COMMON $UTILS $ENGINE src/session/session.cpp tests/session_test.cpp \
  -liconv -o build/session_test.exe

echo "[1b] 编译候选窗自测（显示/命中/悬停/点选）..."
"$GPP" -std=c++17 -O2 -DUNICODE -D_UNICODE -Isrc/win/tsf tests/candidate_window_test.cpp \
  src/win/tsf/CandidateWindow.cpp -lgdiplus -lgdi32 -luser32 -static -o build/candidate_window_test.exe

echo "[2/5] 编译版本资源..."
"$WINDRES" -O coff --preprocessor="gcc -E -xc-header -DRC_INVOKED" installer/version.rc -o build/version.o

echo "[3/5] 编译 TSF 输入法 DLL..."
"$GPP" -shared $COMMON -DUNICODE -D_UNICODE -Isrc/win/tsf \
  $UTILS $ENGINE src/session/session.cpp \
  src/win/tsf/FreeIME11Service.cpp src/win/tsf/dllmain.cpp src/win/tsf/CandidateWindow.cpp build/version.o \
  -liconv -lole32 -ladvapi32 -luuid -lshell32 -lgdi32 -lgdiplus -static \
  src/win/tsf/FreeIME11.def -o build/FreeIME11.dll

echo "[4/5] 编译数据迁移工具..."
"$GPP" $COMMON -DUNICODE -D_UNICODE -municode -Isrc/win/config \
  $UTILS src/win/config/legacy_importer.cpp src/win/migrate/main.cpp \
  -liconv -static -o build/freeime11-migrate.exe

echo "[5/6] 编译设置程序..."
"$GPP" $COMMON -DUNICODE -D_UNICODE -municode \
  $UTILS src/win/settings_app/main.cpp \
  -liconv -lshell32 -luser32 -static -o build/freeime11-settings.exe

echo "[6/7] 编译卸载程序..."
"$GPP" $COMMON -DUNICODE -D_UNICODE -municode \
  src/win/uninstall/main.cpp \
  -lshell32 -luser32 -static -o build/freeime11-uninstall.exe

echo "[7/7] 编译语言列表注册工具..."
"$GPP" -std=c++17 -O2 -DUNICODE -D_UNICODE -municode \
  src/win/addlang/main.cpp \
  -lole32 -luuid -ladvapi32 -static -o build/freeime11-addlang.exe

echo ""
echo "构建完成："
ls -la build/*.exe build/*.dll 2>/dev/null
