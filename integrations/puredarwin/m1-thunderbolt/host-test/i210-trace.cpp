// SPDX-License-Identifier: MIT
#include "pd_i210_trace.h"
#include <atomic>
#include <cassert>
#include <thread>
#include <vector>
int main()
{
    uint32_t budget = 0;
    std::atomic<unsigned> accepted{0};
    std::vector<std::thread> threads;
    for (unsigned n = 0; n < 8; ++n)
        threads.emplace_back([&] {
            for (unsigned i = 0; i < 10000; ++i)
                if (pd_i210_trace_take(&budget)) ++accepted;
        });
    for (auto &thread : threads) thread.join();
    assert(accepted == 8 && budget == 8);
    assert(!pd_i210_trace_take(&budget) && budget == 8);
    budget = UINT32_MAX;
    assert(!pd_i210_trace_take(&budget) && budget == UINT32_MAX);
}
