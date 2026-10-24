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

echo "[2/5] 编译版本资源..."
"$WINDRES" -O coff --preprocessor="gcc -E -xc-header -DRC_INVOKED" installer/version.rc -o build/version.o

echo "[3/5] 编译 TSF 输入法 DLL..."
"$GPP" -shared $COMMON -DUNICODE -D_UNICODE -Isrc/win/tsf \
  $UTILS $ENGINE src/session/session.cpp \
  src/win/tsf/FreeIME11Service.cpp src/win/tsf/dllmain.cpp build/version.o \
  -liconv -lole32 -ladvapi32 -luuid -lshell32 -static-libgcc -static-libstdc++ \
  src/win/tsf/FreeIME11.def -o build/FreeIME11.dll

echo "[4/5] 编译数据迁移工具..."
"$GPP" $COMMON -DUNICODE -D_UNICODE -municode -Isrc/win/config \
  $UTILS src/win/config/legacy_importer.cpp src/win/migrate/main.cpp \
  -liconv -static-libgcc -static-libstdc++ -o build/freeime11-migrate.exe

echo "[5/5] 编译设置程序..."
"$GPP" $COMMON -DUNICODE -D_UNICODE -municode \
  $UTILS src/win/settings_app/main.cpp \
  -liconv -lshell32 -luser32 -static-libgcc -static-libstdc++ -o build/freeime11-settings.exe

echo ""
echo "构建完成："
ls -la build/*.exe build/*.dll 2>/dev/null
