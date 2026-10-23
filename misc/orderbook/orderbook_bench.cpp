// Small performance suite for misc/orderbook/orderbook.hpp.
//
// Build (Homebrew clang; Apple clang 15 lacks C++20 parenthesized aggregate init):
//   /opt/homebrew/opt/llvm/bin/clang++ -std=c++20 -O3 -DNDEBUG -mcpu=native orderbook_bench.cpp -o orderbook_bench
// Run all scenarios, or only those whose name contains a filter:
//   ./orderbook_bench            ./orderbook_bench mixed
//
// Every scenario replays a fixed, pre-generated stream of operations (fixed
// seed), so runs are comparable before and after an optimisation. The checksum
// folds every order id, trade, cancel result and best-price query: if an
// optimisation changes the checksum, it changed behaviour, so run
// orderbook_test as well.

#include "orderbook.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <new>
#include <random>
#include <string>
#include <vector>

// ---------- allocation counting ----------

static std::uint64_t g_allocs = 0;

void* operator new(std::size_t n) {
    ++g_allocs;
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

// ---------- operation stream ----------

enum class Kind : std::uint8_t { AddPassive, AddAggressive, Cancel, Query, Count };
static const char* kind_name[] = {"add passive", "add aggressive", "cancel", "best bid/ask"};

struct Op {
    Kind kind;
    Side side;
    Price price;
    Qty qty;
    OrderId id;   // cancel target
};

struct Gen {
    std::mt19937_64 rng;
    OrderId adds = 0;   // ids are assigned 0, 1, 2, ... by add()
    explicit Gen(std::uint64_t seed) : rng(seed) {}

    std::uint32_t uniform(std::uint32_t lo, std::uint32_t hi) {
        return std::uniform_int_distribution<std::uint32_t>(lo, hi)(rng);
    }
    Op add(Kind k, Side s, Price p, Qty q) { ++adds; return Op{k, s, p, q, 0}; }
    // Mostly recent orders, like a market maker re-quoting.
    Op cancel_recent(std::uint32_t window) {
        OrderId back = adds == 0 ? 0 : uniform(0, std::min<std::uint32_t>(adds - 1, window));
        return Op{Kind::Cancel, Side::Buy, 0, 0, adds == 0 ? 0 : adds - 1 - back};
    }
};

constexpr Price kMid = 1'000'000;

// Passive orders only, spread over ~2000 levels, never crossing.
static std::vector<Op> gen_build(std::size_t n) {
    Gen g(1);
    std::vector<Op> ops;
    ops.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        bool buy = i % 2 == 0;
        Price p = buy ? kMid - 1 - g.uniform(0, 999) : kMid + 1 + g.uniform(0, 999);
        ops.push_back(g.add(Kind::AddPassive, buy ? Side::Buy : Side::Sell, p, g.uniform(1, 100)));
    }
    return ops;
}

// Roughly market-like: quotes near a drifting mid, some takers, many cancels.
static std::vector<Op> gen_mixed(std::size_t n) {
    Gen g(2);
    std::vector<Op> ops;
    ops.reserve(n);
    Price mid = kMid;
    for (std::size_t i = 0; i < n; ++i) {
        if (i % 64 == 0) mid += g.uniform(0, 2) - 1;              // random walk
        std::uint32_t r = g.uniform(1, 100);
        bool buy = g.uniform(0, 1);
        Side s = buy ? Side::Buy : Side::Sell;
        if (r <= 50) {
            Price p = buy ? mid - 1 - g.uniform(0, 19) : mid + 1 + g.uniform(0, 19);
            ops.push_back(g.add(Kind::AddPassive, s, p, g.uniform(1, 100)));
        } else if (r <= 60) {
            Price p = buy ? mid + g.uniform(0, 3) : mid - g.uniform(0, 3);
            ops.push_back(g.add(Kind::AddAggressive, s, p, g.uniform(1, 300)));
        } else if (r <= 95) {
            ops.push_back(g.cancel_recent(200));
        } else {
            ops.push_back(Op{Kind::Query, s, 0, 0, 0});
        }
    }
    return ops;
}

// Mostly cancels of recently added orders.
static std::vector<Op> gen_cancel_heavy(std::size_t n) {
    Gen g(3);
    std::vector<Op> ops;
    ops.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        if (i < 10'000 || g.uniform(1, 100) <= 30) {
            bool buy = g.uniform(0, 1);
            Price p = buy ? kMid - 1 - g.uniform(0, 49) : kMid + 1 + g.uniform(0, 49);
            ops.push_back(g.add(Kind::AddPassive, buy ? Side::Buy : Side::Sell, p, g.uniform(1, 100)));
        } else {
            ops.push_back(g.cancel_recent(5'000));
        }
    }
    return ops;
}

// Build 32 ask levels x 4 orders, then one buy that sweeps all of them.
static std::vector<Op> gen_sweep(std::size_t n) {
    Gen g(4);
    std::vector<Op> ops;
    ops.reserve(n);
    constexpr Price kLevels = 32, kPerLevel = 4;
    constexpr Qty kQty = 10;
    while (ops.size() + kLevels * kPerLevel + 1 <= n) {
        for (Price l = 0; l < kLevels; ++l)
            for (Price k = 0; k < kPerLevel; ++k)
                ops.push_back(g.add(Kind::AddPassive, Side::Sell, kMid + 1 + l, kQty));
        ops.push_back(g.add(Kind::AddAggressive, Side::Buy, kMid + kLevels, kLevels * kPerLevel * kQty));
    }
    return ops;
}

// ---------- running ----------

static inline void fold(std::uint64_t& h, std::uint64_t x) { h = (h ^ x) * 0x100000001b3ULL; }

static inline void apply(OrderBook& book, const Op& op, std::uint64_t& h) {
    switch (op.kind) {
    case Kind::AddPassive:
    case Kind::AddAggressive: {
        OrderResult r = book.add(op.side, op.price, op.qty);
        fold(h, r.order_id);
        for (const Trade& t : r.trades) {
            fold(h, t.maker_order_id);
            fold(h, t.price);
            fold(h, t.qty);
        }
        break;
    }
    case Kind::Cancel:
        fold(h, book.cancel(op.id));
        break;
    case Kind::Query: {
        auto b = op.side == Side::Buy ? book.getBestBid() : book.getBestAsk();
        fold(h, b ? (std::uint64_t{b->price} << 32 | b->qty) : ~0ULL);
        break;
    }
    case Kind::Count:
        break;
    }
}

using Clock = std::chrono::steady_clock;

static double percentile(std::vector<std::uint32_t>& v, double p) {
    if (v.empty()) return 0;
    std::size_t i = std::min(v.size() - 1, static_cast<std::size_t>(p * v.size()));
    std::nth_element(v.begin(), v.begin() + static_cast<long>(i), v.end());
    return v[i];
}

static void run(const char* name, const std::vector<Op>& ops, int reps) {
    // Throughput: whole stream, no per-op timing. Book destruction is not timed.
    std::vector<double> ns_per_op;
    std::uint64_t checksum = 0, allocs = 0;
    bool stable = true;
    for (int r = 0; r < reps; ++r) {
        auto book = std::make_unique<OrderBook>();
        std::uint64_t h = 0xcbf29ce484222325ULL;
        std::uint64_t a0 = g_allocs;
        auto t0 = Clock::now();
        for (const Op& op : ops) apply(*book, op, h);
        auto t1 = Clock::now();
        allocs = g_allocs - a0;
        ns_per_op.push_back(std::chrono::duration<double, std::nano>(t1 - t0).count() / ops.size());
        if (r == 0) checksum = h; else stable &= h == checksum;
    }
    std::sort(ns_per_op.begin(), ns_per_op.end());

    std::printf("\n== %s  (%zu ops, %d reps)\n", name, ops.size(), reps);
    std::printf("   throughput  best %.1f ns/op (%.2f Mops/s), median %.1f ns/op\n",
                ns_per_op.front(), 1e3 / ns_per_op.front(), ns_per_op[ns_per_op.size() / 2]);
    std::printf("   allocations %.2f per op\n", static_cast<double>(allocs) / ops.size());
    std::printf("   checksum    %016llx%s\n", static_cast<unsigned long long>(checksum),
                stable ? "" : "  (NOT stable across reps!)");

    // Latency: one more run, timing each op individually, split by kind.
    std::vector<std::uint32_t> lat[static_cast<int>(Kind::Count)];
    {
        auto book = std::make_unique<OrderBook>();
        std::uint64_t h = 0;
        for (const Op& op : ops) {
            auto t0 = Clock::now();
            apply(*book, op, h);
            auto t1 = Clock::now();
            lat[static_cast<int>(op.kind)].push_back(
                static_cast<std::uint32_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count()));
        }
    }
    std::printf("   latency ns  %-15s %8s %8s %8s %8s %9s\n", "", "count", "p50", "p99", "p99.9", "max");
    for (int k = 0; k < static_cast<int>(Kind::Count); ++k) {
        auto& v = lat[k];
        if (v.empty()) continue;
        std::uint32_t mx = *std::max_element(v.begin(), v.end());
        std::printf("               %-15s %8zu %8.0f %8.0f %8.0f %9u\n", kind_name[k], v.size(),
                    percentile(v, 0.50), percentile(v, 0.99), percentile(v, 0.999), mx);
    }
}

int main(int argc, char** argv) {
    std::string filter = argc > 1 ? argv[1] : "";
    constexpr std::size_t kOps = 1'000'000;
    constexpr int kReps = 5;

    // Clock::now() itself costs time and on Apple Silicon ticks every ~42 ns,
    // so per-op latencies below that are not meaningful; compare tails (p99+).
    {
        auto t0 = Clock::now();
        for (int i = 0; i < 1'000'000; ++i) (void)Clock::now();
        auto t1 = Clock::now();
        std::printf("timer overhead ~%.1f ns per now() call\n",
                    std::chrono::duration<double, std::nano>(t1 - t0).count() / 1e6);
    }

    struct Scenario { const char* name; std::vector<Op> (*gen)(std::size_t); };
    const Scenario scenarios[] = {
        {"build (passive adds)", gen_build},
        {"mixed (add/take/cancel/query)", gen_mixed},
        {"cancel-heavy", gen_cancel_heavy},
        {"sweep (32 levels per taker)", gen_sweep},
    };
    for (const Scenario& s : scenarios)
        if (filter.empty() || std::string(s.name).find(filter) != std::string::npos)
            run(s.name, s.gen(kOps), kReps);
}
