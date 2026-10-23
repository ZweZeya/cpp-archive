#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <functional>
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
    struct Level;

    struct Order {
        OrderId order_id;
        Side side;
        Price price;
        Qty qty;
        Level* level;
        Order* next;
        Order* prev;

        Order(OrderId order_id, Side side, Price price, Qty qty, Level* level)
            : order_id(order_id), side(side), price(price), qty(qty), level(level), next(nullptr), prev(nullptr) {}
    };

    struct Level {
        Qty total_qty;
        Order* head;
        Order* tail;

        Level() : total_qty(0), head(nullptr), tail(nullptr) {}

        Order* front() const noexcept {
            return head;
        }

        bool empty() const noexcept { 
            return total_qty == 0 && head == nullptr && tail == nullptr; 
        }

        void push_back(Order* order) {
            order->prev = tail;
            if (tail)
                tail->next = order;
            tail = order;

            if (head == nullptr)
                head = order;
        }

        void pop_front() {
            assert(head != nullptr);

            Order* next_head = head->next;
            if (next_head == nullptr) 
                tail = nullptr;
            else 
                next_head->prev = nullptr;

            head->next = nullptr;
            head = next_head;
        }
    };

    OrderId current_order_id;
    std::map<Price, Level> asks;
    std::map<Price, Level, std::greater<Price>> bids;
    std::unordered_map<OrderId, Order> orders;

    template <typename Book>
    void removeFromBook(Book& book, Order* order) {
        Level* level = order->level;
        Order* next_order = order->next;
        Order* prev_order = order->prev;

        if (next_order) 
            next_order->prev = prev_order;
        if (prev_order) 
            prev_order->next = next_order;

        if (order == level->head)
            level->head = next_order;
        if (order == level->tail)
            level->tail = prev_order;

        level->total_qty -= order->qty;
        if (level->empty())
            book.erase(order->price);
    }

    template <typename Book>
    Qty match(Book& book, OrderId order_id, Side side, Price price, Qty qty, Trades& trades) {
        Qty remaining_qty = qty;
        while (remaining_qty > 0 && !book.empty() && !book.key_comp()(price, book.begin()->first)) {
            Level& level = book.begin()->second;
            while (remaining_qty > 0 && !level.empty()) {
                Order* matched_order = level.front();
                assert(matched_order != nullptr);

                Qty traded_qty = std::min(remaining_qty, matched_order->qty);
                remaining_qty -= traded_qty;
                matched_order->qty -= traded_qty;
                level.total_qty -= traded_qty;

                trades.emplace_back(order_id, matched_order->order_id, side, book.begin()->first, traded_qty);

                if (matched_order->qty == 0) {
                    level.pop_front();
                    orders.erase(matched_order->order_id);
                }
            }
            if (level.empty())
                book.erase(book.begin());
        }
        return remaining_qty;
    }

    void record(OrderId order_id, Side side, Price price, Qty qty) {
        Level* level = side == Side::Buy ? &bids[price] : &asks[price];
        level->total_qty += qty;
        auto result = orders.try_emplace(order_id, order_id, side, price, qty, level);
        level->push_back(&(result.first->second));
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
            removeFromBook(bids, &order);
        else 
            removeFromBook(asks, &order);
        orders.erase(order_itr);

        return true;
    }
};