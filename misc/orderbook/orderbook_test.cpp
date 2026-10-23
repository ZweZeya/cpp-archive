// Small correctness suite for misc/orderbook/orderbook.hpp.
// Build (Apple clang 15 lacks C++20 parenthesized aggregate init; use Homebrew clang):
//   /opt/homebrew/opt/llvm/bin/clang++ -std=c++20 -O1 -g -fsanitize=address,undefined orderbook_test.cpp -o orderbook_test && ./orderbook_test
//
// Part 1: hand-written scenarios that pin down the expected semantics.
// Part 2: randomized comparison against a naive reference book.

#include "orderbook.hpp"

#include <cstdio>
#include <optional>
#include <random>
#include <string>
#include <vector>

// ---------- tiny harness ----------

static int failures = 0;
static int checks = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        ++checks;                                                          \
        if (!(cond)) {                                                     \
            ++failures;                                                    \
            std::printf("  FAIL %s:%d: %s\n", __func__, __LINE__, #cond);  \
        }                                                                  \
    } while (0)

static bool same(const Trade& a, const Trade& b) {
    return a.taker_order_id == b.taker_order_id && a.maker_order_id == b.maker_order_id &&
           a.taker_side == b.taker_side && a.price == b.price && a.qty == b.qty;
}
static bool same(const Trades& a, const Trades& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (!same(a[i], b[i])) return false;
    return true;
}
static bool same(const std::optional<PriceLevel>& a, const std::optional<PriceLevel>& b) {
    if (a.has_value() != b.has_value()) return false;
    return !a || (a->price == b->price && a->qty == b->qty);
}
static std::optional<PriceLevel> lvl(Price p, Qty q) { return PriceLevel{p, q}; }
static const std::optional<PriceLevel> none = std::nullopt;
static Trade T(OrderId taker, OrderId maker, Side s, Price p, Qty q) { return Trade{taker, maker, s, p, q}; }

// ---------- part 1: scenarios ----------

static void empty_book() {
    OrderBook b;
    CHECK(same(b.getBestBid(), none));
    CHECK(same(b.getBestAsk(), none));
    CHECK(!b.cancel(0));
    CHECK(!b.cancel(12345));
}

static void ids_are_sequential_from_zero() {
    OrderBook b;
    CHECK(b.add(Side::Buy, 100, 1).order_id == 0);
    CHECK(b.add(Side::Sell, 200, 1).order_id == 1);
    CHECK(b.add(Side::Buy, 200, 1).order_id == 2);   // crosses, still gets an id
}

static void resting_orders_aggregate_by_level() {
    OrderBook b;
    CHECK(b.add(Side::Buy, 100, 10).trades.empty());
    CHECK(b.add(Side::Buy, 100, 5).trades.empty());
    CHECK(b.add(Side::Buy, 99, 7).trades.empty());
    CHECK(b.add(Side::Sell, 105, 3).trades.empty());
    CHECK(b.add(Side::Sell, 104, 4).trades.empty());
    CHECK(same(b.getBestBid(), lvl(100, 15)));
    CHECK(same(b.getBestAsk(), lvl(104, 4)));
}

static void exact_cross_fills_both() {
    OrderBook b;
    b.add(Side::Sell, 100, 10);                       // id 0
    auto r = b.add(Side::Buy, 100, 10);               // id 1
    CHECK(same(r.trades, {T(1, 0, Side::Buy, 100, 10)}));
    CHECK(same(b.getBestBid(), none));
    CHECK(same(b.getBestAsk(), none));
    CHECK(!b.cancel(0));                              // fully filled: gone
    CHECK(!b.cancel(1));                              // never rested
}

static void trade_happens_at_maker_price() {
    OrderBook b;
    b.add(Side::Sell, 100, 5);                        // id 0
    auto r = b.add(Side::Buy, 105, 5);                // id 1, willing to pay more
    CHECK(same(r.trades, {T(1, 0, Side::Buy, 100, 5)}));

    b.add(Side::Buy, 90, 5);                          // id 2
    r = b.add(Side::Sell, 80, 5);                     // id 3, willing to sell lower
    CHECK(same(r.trades, {T(3, 2, Side::Sell, 90, 5)}));
}

static void partial_fill_of_maker_keeps_remainder() {
    OrderBook b;
    b.add(Side::Sell, 100, 10);                       // id 0
    auto r = b.add(Side::Buy, 100, 4);                // id 1
    CHECK(same(r.trades, {T(1, 0, Side::Buy, 100, 4)}));
    CHECK(same(b.getBestAsk(), lvl(100, 6)));
    CHECK(same(b.getBestBid(), none));
    CHECK(b.cancel(0));                               // remainder is still cancellable
    CHECK(same(b.getBestAsk(), none));
}

static void unfilled_taker_remainder_rests_at_its_limit() {
    OrderBook b;
    b.add(Side::Sell, 100, 4);                        // id 0
    auto r = b.add(Side::Buy, 101, 10);               // id 1
    CHECK(same(r.trades, {T(1, 0, Side::Buy, 100, 4)}));
    CHECK(same(b.getBestBid(), lvl(101, 6)));
    CHECK(same(b.getBestAsk(), none));
    CHECK(b.cancel(1));
    CHECK(same(b.getBestBid(), none));
}

static void fifo_within_a_level() {
    OrderBook b;
    b.add(Side::Sell, 100, 3);                        // id 0
    b.add(Side::Sell, 100, 3);                        // id 1
    b.add(Side::Sell, 100, 3);                        // id 2
    auto r = b.add(Side::Buy, 100, 5);                // id 3
    CHECK(same(r.trades, {T(3, 0, Side::Buy, 100, 3), T(3, 1, Side::Buy, 100, 2)}));
    CHECK(same(b.getBestAsk(), lvl(100, 4)));
    r = b.add(Side::Buy, 100, 2);                     // id 4: partially filled id 1 keeps its place
    CHECK(same(r.trades, {T(4, 1, Side::Buy, 100, 1), T(4, 2, Side::Buy, 100, 1)}));
}

static void buy_sweeps_levels_in_price_order() {
    OrderBook b;
    b.add(Side::Sell, 101, 2);                        // id 0
    b.add(Side::Sell, 100, 2);                        // id 1
    b.add(Side::Sell, 102, 2);                        // id 2
    auto r = b.add(Side::Buy, 102, 5);                // id 3
    CHECK(same(r.trades, {T(3, 1, Side::Buy, 100, 2), T(3, 0, Side::Buy, 101, 2),
                          T(3, 2, Side::Buy, 102, 1)}));
    CHECK(same(b.getBestAsk(), lvl(102, 1)));
}

static void sell_sweeps_levels_in_price_order() {
    OrderBook b;
    b.add(Side::Buy, 99, 2);                          // id 0
    b.add(Side::Buy, 101, 2);                         // id 1
    b.add(Side::Buy, 100, 2);                         // id 2
    auto r = b.add(Side::Sell, 99, 5);                // id 3
    CHECK(same(r.trades, {T(3, 1, Side::Sell, 101, 2), T(3, 2, Side::Sell, 100, 2),
                          T(3, 0, Side::Sell, 99, 1)}));
    CHECK(same(b.getBestBid(), lvl(99, 1)));
}

static void limit_price_is_respected() {
    OrderBook b;
    b.add(Side::Sell, 100, 1);                        // id 0
    b.add(Side::Sell, 105, 1);                        // id 1
    auto r = b.add(Side::Buy, 103, 5);                // id 2
    CHECK(same(r.trades, {T(2, 0, Side::Buy, 100, 1)}));
    CHECK(same(b.getBestBid(), lvl(103, 4)));
    CHECK(same(b.getBestAsk(), lvl(105, 1)));
}

static void sweep_entire_side_then_rest() {
    OrderBook b;
    for (Price p = 100; p < 110; ++p) b.add(Side::Sell, p, 1);   // ids 0..9
    auto r = b.add(Side::Buy, 200, 15);                           // id 10
    CHECK(r.trades.size() == 10);
    CHECK(same(b.getBestAsk(), none));
    CHECK(same(b.getBestBid(), lvl(200, 5)));
}

static void cancel_rules() {
    OrderBook b;
    b.add(Side::Buy, 100, 5);                         // id 0
    CHECK(!b.cancel(99));                             // unknown id
    CHECK(b.cancel(0));
    CHECK(!b.cancel(0));                              // second cancel fails
    CHECK(same(b.getBestBid(), none));
}

static void cancel_middle_keeps_fifo_of_others() {
    OrderBook b;
    b.add(Side::Sell, 100, 1);                        // id 0
    b.add(Side::Sell, 100, 1);                        // id 1
    b.add(Side::Sell, 100, 1);                        // id 2
    CHECK(b.cancel(1));
    CHECK(same(b.getBestAsk(), lvl(100, 2)));
    auto r = b.add(Side::Buy, 100, 2);                // id 3
    CHECK(same(r.trades, {T(3, 0, Side::Buy, 100, 1), T(3, 2, Side::Buy, 100, 1)}));
}

static void cancel_last_order_removes_level() {
    OrderBook b;
    b.add(Side::Buy, 101, 1);                         // id 0
    b.add(Side::Buy, 100, 1);                         // id 1
    CHECK(same(b.getBestBid(), lvl(101, 1)));
    CHECK(b.cancel(0));
    CHECK(same(b.getBestBid(), lvl(100, 1)));         // best moves to the next level
}

static void level_is_recreated_cleanly() {
    OrderBook b;
    b.add(Side::Sell, 100, 3);                        // id 0
    b.add(Side::Buy, 100, 3);                         // id 1: level 100 emptied by a match
    b.add(Side::Sell, 100, 2);                        // id 2: level recreated
    CHECK(same(b.getBestAsk(), lvl(100, 2)));
    CHECK(b.cancel(2));                               // level emptied by a cancel
    b.add(Side::Sell, 100, 7);                        // id 3: recreated again
    CHECK(same(b.getBestAsk(), lvl(100, 7)));
    auto r = b.add(Side::Buy, 100, 7);
    CHECK(same(r.trades, {T(4, 3, Side::Buy, 100, 7)}));
}

static void zero_qty_order_has_no_effect() {
    OrderBook b;
    b.add(Side::Sell, 100, 5);                        // id 0
    auto r = b.add(Side::Buy, 100, 0);                // id 1
    CHECK(r.order_id == 1);
    CHECK(r.trades.empty());
    CHECK(same(b.getBestAsk(), lvl(100, 5)));
    CHECK(same(b.getBestBid(), none));
    CHECK(!b.cancel(1));
}

static void extreme_prices() {
    OrderBook b;
    b.add(Side::Buy, 0, 1);                           // id 0
    b.add(Side::Sell, UINT32_MAX, 1);                 // id 1
    CHECK(same(b.getBestBid(), lvl(0, 1)));
    CHECK(same(b.getBestAsk(), lvl(UINT32_MAX, 1)));
    auto r = b.add(Side::Sell, 0, 1);                 // id 2: hits the bid at 0
    CHECK(same(r.trades, {T(2, 0, Side::Sell, 0, 1)}));
    r = b.add(Side::Buy, UINT32_MAX, 1);              // id 3: lifts the ask at max
    CHECK(same(r.trades, {T(3, 1, Side::Buy, UINT32_MAX, 1)}));
}

// ---------- part 2: randomized comparison against a naive book ----------

// Every resting order in one vector in arrival order; linear scans everywhere.
// Slow on purpose, so it is easy to trust.
class ReferenceBook {
    struct Resting { OrderId id; Side side; Price price; Qty qty; };
    OrderId next_id_ = 0;
    std::vector<Resting> resting_;

    std::optional<std::size_t> best_maker(Side taker, Price limit) const {
        std::optional<std::size_t> best;
        for (std::size_t i = 0; i < resting_.size(); ++i) {
            const Resting& r = resting_[i];
            if (r.side == taker) continue;
            bool crosses = taker == Side::Buy ? r.price <= limit : r.price >= limit;
            if (!crosses) continue;
            if (!best || (taker == Side::Buy ? r.price < resting_[*best].price
                                             : r.price > resting_[*best].price))
                best = i;                             // ties keep the earlier order
        }
        return best;
    }

    std::optional<PriceLevel> best_level(Side side) const {
        std::optional<Price> best;
        for (const Resting& r : resting_)
            if (r.side == side && (!best || (side == Side::Buy ? r.price > *best : r.price < *best)))
                best = r.price;
        if (!best) return std::nullopt;
        Qty total = 0;
        for (const Resting& r : resting_)
            if (r.side == side && r.price == *best) total += r.qty;
        return PriceLevel{*best, total};
    }

public:
    OrderResult add(Side side, Price price, Qty qty) {
        OrderId id = next_id_++;
        Trades trades;
        Qty remaining = qty;
        while (remaining > 0) {
            auto i = best_maker(side, price);
            if (!i) break;
            Resting& m = resting_[*i];
            Qty q = remaining < m.qty ? remaining : m.qty;
            trades.push_back(Trade{id, m.id, side, m.price, q});
            remaining -= q;
            m.qty -= q;
            if (m.qty == 0) resting_.erase(resting_.begin() + static_cast<long>(*i));
        }
        if (remaining > 0) resting_.push_back(Resting{id, side, price, remaining});
        return OrderResult{id, std::move(trades)};
    }

    bool cancel(OrderId id) {
        for (auto it = resting_.begin(); it != resting_.end(); ++it)
            if (it->id == id) { resting_.erase(it); return true; }
        return false;
    }

    std::optional<PriceLevel> getBestBid() const { return best_level(Side::Buy); }
    std::optional<PriceLevel> getBestAsk() const { return best_level(Side::Sell); }
};

struct FuzzConfig {
    const char* name;
    Price lo, hi;       // price band: narrow => lots of crossing
    Qty max_qty;
    int cancel_pct;     // percentage of ops that are cancels
    int ops_per_seed;
    int seeds;
};

static std::string describe(Side s, Price p, Qty q) {
    return std::string(s == Side::Buy ? "add Buy " : "add Sell ") + std::to_string(p) + " x" + std::to_string(q);
}

static bool fuzz_one(const FuzzConfig& cfg, unsigned seed) {
    std::mt19937 rng(seed);
    auto pick = [&](int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(rng); };

    OrderBook book;
    ReferenceBook ref;
    OrderId adds = 0;
    std::vector<std::string> log;

    auto fail = [&](int op, const std::string& what) {
        ++failures;
        std::printf("  FAIL fuzz[%s] seed=%u op=%d: %s\n    last ops:\n", cfg.name, seed, op, what.c_str());
        std::size_t from = log.size() > 8 ? log.size() - 8 : 0;
        for (std::size_t i = from; i < log.size(); ++i) std::printf("      %s\n", log[i].c_str());
        return false;
    };

    for (int op = 0; op < cfg.ops_per_seed; ++op) {
        ++checks;
        if (pick(1, 100) <= cfg.cancel_pct && adds > 0) {
            // mostly recent ids, sometimes old/filled/unknown ones
            OrderId id = pick(1, 10) == 1 ? static_cast<OrderId>(pick(0, static_cast<int>(adds) + 5))
                                          : adds - 1 - static_cast<OrderId>(pick(0, std::min<int>(static_cast<int>(adds) - 1, 50)));
            log.push_back("cancel " + std::to_string(id));
            bool got = book.cancel(id), want = ref.cancel(id);
            if (got != want) return fail(op, "cancel(" + std::to_string(id) + ") returned " + (got ? "true" : "false"));
        } else {
            Side s = pick(0, 1) ? Side::Buy : Side::Sell;
            Price p = static_cast<Price>(pick(static_cast<int>(cfg.lo), static_cast<int>(cfg.hi)));
            Qty q = static_cast<Qty>(pick(1, static_cast<int>(cfg.max_qty)));
            log.push_back(describe(s, p, q));
            OrderResult got = book.add(s, p, q);
            OrderResult want = ref.add(s, p, q);
            ++adds;
            if (got.order_id != want.order_id) return fail(op, "order_id mismatch");
            if (!same(got.trades, want.trades))
                return fail(op, "trades mismatch (got " + std::to_string(got.trades.size()) +
                                " trades, want " + std::to_string(want.trades.size()) + ")");
        }
        auto bb = book.getBestBid(), ba = book.getBestAsk();
        if (!same(bb, ref.getBestBid())) return fail(op, "best bid mismatch");
        if (!same(ba, ref.getBestAsk())) return fail(op, "best ask mismatch");
        if (bb && ba && bb->price >= ba->price) return fail(op, "book is crossed");
    }
    return true;
}

static void fuzz(const FuzzConfig& cfg) {
    int passed = 0;
    for (int s = 1; s <= cfg.seeds; ++s)
        if (fuzz_one(cfg, static_cast<unsigned>(s))) ++passed;
    std::printf("  fuzz[%-12s] %d/%d seeds x %d ops passed\n", cfg.name, passed, cfg.seeds, cfg.ops_per_seed);
}

int main() {
    std::printf("scenarios\n");
    empty_book();
    ids_are_sequential_from_zero();
    resting_orders_aggregate_by_level();
    exact_cross_fills_both();
    trade_happens_at_maker_price();
    partial_fill_of_maker_keeps_remainder();
    unfilled_taker_remainder_rests_at_its_limit();
    fifo_within_a_level();
    buy_sweeps_levels_in_price_order();
    sell_sweeps_levels_in_price_order();
    limit_price_is_respected();
    sweep_entire_side_then_rest();
    cancel_rules();
    cancel_middle_keeps_fifo_of_others();
    cancel_last_order_removes_level();
    level_is_recreated_cleanly();
    zero_qty_order_has_no_effect();
    extreme_prices();

    std::printf("randomized vs reference\n");
    fuzz({"tight",        95,  105, 20,  25, 2000, 100});   // heavy crossing
    fuzz({"wide",         50,  150, 50,  25, 2000, 100});   // deeper book
    fuzz({"cancel-heavy", 95,  105, 10,  70, 2000, 100});   // mostly cancels
    fuzz({"big-qty",      98,  102, 1000, 30, 2000, 50});   // multi-level sweeps

    std::printf("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
