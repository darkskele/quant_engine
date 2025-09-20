#pragma once

#include <string>
#include <memory>
#include <variant>
#include <chrono>

namespace engine::events
{

    /**
     * @brief event representing new market data.
     */
    struct market_event
    {
        std::string symbol_;   ///< Trade symbol.
        double price_;         ///< Trade price at the time of the tick.
        double qty_;           ///< Quantity of the base asset traded.
        int64_t timestamp_ms_; ///< Epoch timestamp of the trade in milliseconds.
        bool is_buyer_match_;  ///< True if the buyer initiated the trade (i.e., aggressive buy).
    };

    /**
     * @brief event representing a trading signal from a strategy.
     */
    struct signal_event
    {
        // Empty for now
    };

    /**
     * @brief event representing an order submitted to the market.
     */
    struct order_event
    {
        std::string symbol_;
        std::string order_id_;
        int64_t quantity_;
        bool is_buy_;
        double price_;
    };

    /**
     * @brief event representing a filled order (execution result).
     */
    struct fill_event
    {
        std::string symbol_;
        std::string order_id_;
        int64_t filled_qty_;
        int64_t order_qty_;
        bool is_buy_;
        double fill_price_;
        std::chrono::system_clock::time_point timestamp;
    };

    /**
     * @brief Unified event type using std::variant.
     */
    using event = std::variant<market_event, signal_event, order_event, fill_event>;

} // namespace engine::events
