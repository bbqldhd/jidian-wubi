// SPDX-License-Identifier: MulanPub-2.0
// Copyright (c) 2026 Charry
// Freeime11 is licensed under Mulan PubL v2.
// You can use this software according to the terms and conditions of the Mulan PubL v2.
// You may obtain a copy of Mulan PubL v2 at: http://license.coscl.org.cn/MulanPubL-2.0
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
// EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
// MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
// See the Mulan PubL v2 for more details.

// 启动耗时基准：只测 Session/引擎构造，不依赖 TSF
#include <chrono>
#include <cstdio>

#include "py.h"
#include "wbzx.h"
#include "session.h"
#include "settings.h"

using Clock = std::chrono::steady_clock;
static long ms(Clock::time_point a, Clock::time_point b) {
    return static_cast<long>(std::chrono::duration_cast<std::chrono::milliseconds>(b - a).count());
}

int main() {
    auto t0 = Clock::now();
    settings::instance().reload();
    auto t1 = Clock::now();
    std::printf("settings.reload : %ld ms\n", ms(t0, t1));

    t0 = Clock::now();
    freewb::PyEngine py;
    t1 = Clock::now();
    std::printf("PyEngine ctor   : %ld ms\n", ms(t0, t1));

    t0 = Clock::now();
    freewb::WbzxEngine wb;
    t1 = Clock::now();
    std::printf("WbzxEngine ctor : %ld ms\n", ms(t0, t1));

    t0 = Clock::now();
    freeime11::Session s;
    t1 = Clock::now();
    std::printf("Session ctor    : %ld ms\n", ms(t0, t1));

    t0 = Clock::now();
    auto out = s.processKey('w', false, false, false);
    (void)out;
    t1 = Clock::now();
    std::printf("first key       : %ld ms\n", ms(t0, t1));
    return 0;
}
