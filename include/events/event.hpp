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
     * @brief Event representing an order submitted to the market.
     *
     * Immutable: once created, these fields never change.
     */
    struct order_event final
    {
        const std::string symbol_;                              ///< Symbol being traded
        const std::string order_id_;                            ///< Unique order identifier
        const int64_t quantity_;                                ///< Total requested quantity
        const bool is_buy_;                                     ///< Buy = true, Sell = false
        const double price_;                                    ///< Limit/stop price (ignored for pure Market)
        const order_type type_;                                 ///< Market, Limit, Stop, StopLimit
        const order_flags flags_;                               ///< Execution modifiers (IOC, FOK, GTC, etc.)
        const std::chrono::system_clock::time_point timestamp_; ///< Time order was placed
        const market_event trigger_;                            ///< Market event that spawned the order (traceability)

        /// @brief Construct an immutable order event.
        order_event(std::string symbol,
                    std::string order_id,
                    int64_t quantity,
                    bool is_buy,
                    double price,
                    order_type type,
                    order_flags flags,
                    std::chrono::system_clock::time_point ts = std::chrono::system_clock::now(),
                    market_event trigger = {})
            : symbol_(std::move(symbol)),
              order_id_(std::move(order_id)),
              quantity_(quantity),
              is_buy_(is_buy),
              price_(price),
              type_(type),
              flags_(flags),
              timestamp_(ts),
              trigger_(std::move(trigger))
        {
        }
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
        order_event originating_order_;
        std::chrono::system_clock::time_point timestamp{std::chrono::system_clock::now()};
    };

    /**
     * @brief Event representing a cancelled order.
     */
    struct cancel_event
    {
        order_event originating_order_;
        std::string reason_;
        std::chrono::system_clock::time_point timestamp{std::chrono::system_clock::now()};
    };

    /**
     * @brief Unified event type using std::variant.
     */
    using event = std::variant<market_event, signal_event, order_event, fill_event, cancel_event>;

} // namespace engine::events
