#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <functional>
#include <list>
#include <map>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

enum class Side {Buy, Sell};


using OrderId = uint32_t;
using Price = uint32_t;
using Qty = uint32_t;

struct Trade {
    OrderId taker_order_id;
    OrderId maker_order_id;
    Side taker_side;
    Price price;
    Qty qty;
};
using Trades = std::vector<Trade>;

struct OrderResult {
    OrderId order_id;
    Trades trades;
};

struct PriceLevel {
    Price price;
    Qty qty;
};

class OrderBook {
private:
    struct Level {
        Qty total_qty;
        std::list<OrderId> order_ids;
    };

    struct Order {
        OrderId order_id;
        Side side;
        Price price;
        Qty qty;
        Level* level;
        std::list<OrderId>::iterator book_itr;
    };

    OrderId current_order_id;
    std::map<Price, Level> asks;
    std::map<Price, Level, std::greater<Price>> bids;
    std::unordered_map<OrderId, Order> orders;

    template <typename Book>
    void removeFromBook(Book& book, Order& order) {
        Level* level = order.level;
        level->order_ids.erase(order.book_itr);
        level->total_qty -= order.qty;
        if (level->total_qty == 0 && level->order_ids.empty())
            book.erase(order.price);
    }

    template <typename Book>
    Qty match(Book& book, OrderId order_id, Side side, Price price, Qty qty, Trades& trades) {
        Qty remaining_qty = qty;
        while (remaining_qty > 0 && !book.empty() && !book.key_comp()(price, book.begin()->first)) {
            Level& level = book.begin()->second;
            while (remaining_qty > 0 && !level.order_ids.empty()) {
                auto matched_order_itr = orders.find(level.order_ids.front());
                assert(matched_order_itr != orders.end());

                Order& matched_order = matched_order_itr->second;

                Qty traded_qty = std::min(remaining_qty, matched_order.qty);
                remaining_qty -= traded_qty;
                matched_order.qty -= traded_qty;
                level.total_qty -= traded_qty;

                trades.emplace_back(order_id, matched_order.order_id, side, book.begin()->first, traded_qty);

                if (matched_order.qty == 0) {
                    level.order_ids.pop_front();
                    orders.erase(matched_order_itr);
                }
            }
            if (level.total_qty == 0 && level.order_ids.empty()) 
                book.erase(book.begin());
        }
        return remaining_qty;
    }

    void record(OrderId order_id, Side side, Price price, Qty qty) {
        Level* level = side == Side::Buy ? &bids[price] : &asks[price];
        level->total_qty += qty;
        auto& order_ids = level->order_ids;
        auto book_itr = order_ids.insert(order_ids.end(), order_id);
        [[maybe_unused]] const auto& result = orders.try_emplace(order_id, order_id, side, price, qty, level, book_itr);
        assert(result.second);
    }

public:
    OrderBook() noexcept : current_order_id(0) {}

    OrderResult add(Side side, Price price, Qty qty) {
        OrderId order_id = current_order_id++;
        Trades trades;
        Qty remaining_qty = side == Side::Buy
            ? match(asks, order_id, side, price, qty, trades)
            : match(bids, order_id, side, price, qty, trades);
        if (remaining_qty > 0)
            record(order_id, side, price, remaining_qty);
        return OrderResult{order_id, std::move(trades)};
    }


    std::optional<PriceLevel> getBestAsk() const noexcept {
        if (asks.empty()) return std::nullopt;
        auto itr = asks.begin();
        return PriceLevel{itr->first, itr->second.total_qty};
    };

    std::optional<PriceLevel> getBestBid() const noexcept {
        if (bids.empty()) return std::nullopt;
        auto itr = bids.begin();
        return PriceLevel{itr->first, itr->second.total_qty};
    };

    bool cancel(OrderId order_id) {
        std::unordered_map<OrderId, Order>::iterator order_itr;
        if ((order_itr = orders.find(order_id)) == orders.end()) 
            return false;

        Order& order = order_itr->second;
        if (order.side == Side::Buy)
            removeFromBook(bids, order);
        else 
            removeFromBook(asks, order);
        orders.erase(order_itr);

        return true;
    }
};