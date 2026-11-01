# SPDX-License-Identifier: MulanPSL-2.0
# Copyright (c) 2026 Charry
# Freeime11 is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at: http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

# FreeIME 11 —— 极点五笔现代 TSF 重制（Windows 11）
# 目标：Windows 10 1809+ / Windows 11 26H2；架构 x64 / ARM64。
cmake_minimum_required(VERSION 3.20)

project(FreeIME11 VERSION 11.1.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

# 引擎源码为上游 FreeWB（MulanPSL-2.0），仅在此兼容 C++17
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

option(FREEWB_BUILD_TSF "构建 Windows TSF 文本服务（需 Windows SDK）" ON)
option(FREEWB_BUILD_TESTS "构建单元测试" OFF)

# ---------------------------------------------------------------------------
# 可移植引擎与工具库（跨平台，可独立编译验证）
# ---------------------------------------------------------------------------
add_library(freeime11_utils STATIC
    src/utils/ConvertUTF.c
    src/utils/key.cpp
    src/utils/log.cpp
    src/utils/settings.cpp
    src/utils/utils.cpp
    src/utils/sound.cpp
)
target_include_directories(freeime11_utils PUBLIC src/utils src/engine)

add_library(freeime11_engine STATIC
    src/engine/wbzx.cpp
    src/engine/wbpy.cpp
    src/engine/py.cpp
    src/engine/en.cpp
    src/engine/common.cpp
    src/engine/autophrase.cpp
    src/engine/userdict.cpp
    src/engine/freqorder.cpp
    src/engine/gb2312filter.cpp
)
target_include_directories(freeime11_engine PUBLIC src/engine src/utils)
target_link_libraries(freeime11_engine PUBLIC freeime11_utils)

# sqlite3（用户词库/词频可选依赖；未启用时使用上游文本回退）
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/utils/sqlite3.c")
    target_sources(freeime11_utils PRIVATE src/utils/sqlite3.c)
    target_compile_definitions(freeime11_utils PUBLIC FREEWB_HAS_SQLITE)
endif()

# 会话层（跨平台）
add_library(freeime11_session STATIC
    src/session/session.cpp
)
target_include_directories(freeime11_session PUBLIC src/session src/engine src/utils)
target_link_libraries(freeime11_session PUBLIC freeime11_engine)

# ---------------------------------------------------------------------------
# Windows TSF 文本服务
# ---------------------------------------------------------------------------
if(WIN32 AND FREEWB_BUILD_TSF)
    add_library(FreeIME11 SHARED
        src/win/tsf/FreeIME11Service.cpp
        src/win/tsf/dllmain.cpp
        src/win/tsf/FreeIME11.def
    )
    target_link_libraries(FreeIME11 PRIVATE freeime11_session advapi32 ole32)
    target_compile_definitions(FreeIME11 PRIVATE UNICODE _UNICODE)
    target_link_options(FreeIME11 PRIVATE /DEF:${CMAKE_CURRENT_SOURCE_DIR}/src/win/tsf/FreeIME11.def)
    set_target_properties(FreeIME11 PROPERTIES OUTPUT_NAME "FreeIME11")

    # 迁移工具（10.8.9 数据迁移）
    add_executable(freeime11-migrate src/win/config/legacy_importer.cpp src/win/migrate/main.cpp)
    target_link_libraries(freeime11-migrate PRIVATE freeime11_session)
    target_compile_definitions(freeime11-migrate PRIVATE UNICODE _UNICODE)

    # 设置程序
    add_executable(freeime11-settings src/win/settings_app/main.cpp)
    target_link_libraries(freeime11-settings PRIVATE freeime11_session)
    target_compile_definitions(freeime11-settings PRIVATE UNICODE _UNICODE)

    # 卸载程序
    add_executable(freeime11-uninstall src/win/uninstall/main.cpp)
    target_link_libraries(freeime11-uninstall PRIVATE shell32 user32)
    target_compile_definitions(freeime11-uninstall PRIVATE UNICODE _UNICODE)
endif()

# ---------------------------------------------------------------------------
# 测试（跨平台，无 TSF 依赖）
# ---------------------------------------------------------------------------
if(FREEWB_BUILD_TESTS)
    enable_testing()
    add_executable(session_test tests/session_test.cpp)
    target_link_libraries(session_test PRIVATE freeime11_session)
    add_test(NAME session_test COMMAND session_test)
endif()

install(TARGETS freeime11_utils freeime11_engine freeime11_session ARCHIVE DESTINATION lib)
