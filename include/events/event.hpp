#pragma once

#include <string>
#include <memory>
#include <variant>
#include <chrono>

namespace engine::events
{
    /**
     * @brief Enum representing order types.
     */
    enum class order_type
    {
        Market,     ///< Execute immediately at best available price
        Limit,      ///< Post to order book; executes at or better than limit price
        StopMarket, ///< Triggered stop; executes as market order
        StopLimit   ///< Triggered stop; executes as limit order
    };

    /**
     * @brief Enum representing order flags.
     */
    enum order_flags : uint8_t
    {
        None = 0,
        IOC = 1 << 0,       ///< Immediate-Or-Cancel
        FOK = 1 << 1,       ///< Fill-Or-Kill
        PostOnly = 1 << 2,  ///< Must post to book (maker only)
        ReduceOnly = 1 << 3 ///< Must reduce position, not increase
    };

    /**
     * @brief Inlined or operator for order flags.
     */
    inline order_flags operator|(order_flags a, order_flags b)
    {
        return static_cast<order_flags>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
    }

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
        std::string symbol_;                                                                ///< Symbol being traded
        std::string order_id_;                                                              ///< Unique order identifier
        int64_t quantity_;                                                                  ///< Order quantity (positive int)
        bool is_buy_;                                                                       ///< Buy = true, Sell = false
        double price_;                                                                      ///< Limit/stop price (ignored for pure Market)
        order_type type_{order_type::Market};                                               ///< Market, Limit, Stop
        events::order_flags flags_{order_flags::None};                                      ///< Execution modifiers
        std::chrono::system_clock::time_point timestamp_{std::chrono::system_clock::now()}; ///< Time order was placed
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
        std::chrono::system_clock::time_point timestamp{std::chrono::system_clock::now()};
        order_event originating_order_{};
    };

    /**
     * @brief Unified event type using std::variant.
     */
    using event = std::variant<market_event, signal_event, order_event, fill_event>;

} // namespace engine::events
