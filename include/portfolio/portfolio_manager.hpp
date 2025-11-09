#pragma once

#include "portfolio/position_state.hpp"
#include "portfolio/risk_limits.hpp"

#include <array>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <bitset>
#include <optional>
#include <stdexcept>

namespace engine::portfolio
{

    /**
     * @brief Aggregate portfolio level metrics.
     *
     * Stores summary statistics for the entire portfolio such as
     * PnL, exposure, number of positions, and total trades.
     */
    struct PortfolioMetric
    {
        double total_pnl_;      ///< Total P&L.
        double unrealized_pnl_; ///< Unrealized P&L across all open positions.
        double realized_pnl_;   ///< Realized P&L accumulated from closed trades.
        double gross_exposure_; ///< Sum of absolute notional exposure across all symbols.
        double net_exposure_;   ///< Net notional exposure (long minus short).
        int32_t num_positions_; ///< Number of symbols with a non-zero position.
        uint64_t total_trades_; ///< Total number of fills processed.

        /**
         * @brief Construct a PortfolioMetric with all values initialized to zero.
         */
        PortfolioMetric()
            : total_pnl_(0.0), unrealized_pnl_(0.0), realized_pnl_(0.0),
              gross_exposure_(0.0), net_exposure_(0.0), num_positions_(0), total_trades_(0) {}
    };

    /**
     * @brief Manages positions, cash, P&L and basic risk checks for a fixed set of symbols.
     *
     * @tparam EventBus Type providing an @c emit_order(...) interface.
     * @tparam MAX_SYMBOLS Maximum number of symbols tracked by this portfolio.
     */
    template <typename EventBus, size_t MAX_SYMBOLS = 1024>
    class PortfolioManager
    {
    public:
        /**
         * @brief Construct a PortfolioManager with an event bus and initial capital.
         *
         * @param bus Event bus used to publish orders.
         * @param initial_capital Initial cash balance for the portfolio.
         */
        explicit PortfolioManager(EventBus &bus, double initial_capital)
            : event_bus_(bus), cash_(initial_capital), initial_capital_(initial_capital),
              total_realized_pnl_(0), order_count_(0), fill_count_(0), reject_count_(0), next_order_id(1)
        {
            for (auto &symbol : symbol_data_)
            {
                symbol = SymbolData();
            }
        }

        /**
         * @brief Handle a trading signal and attempt to create an order.
         *
         * @param symbol_id Integer ID of the symbol.
         * @param quantity Signed order quantity (positive for buy, negative for sell).
         * @param price Limit price for the order.
         * @param timestamp_ns Timestamp of the signal in nanoseconds.
         *
         * @throws std::out_of_range If @p symbol_id is outside the valid range.
         * @throws std::invalid_argument If @p price is invalid or @p quantity is zero.
         */
        void on_signal(const uint32_t symbol_id, const int32_t quantity, const double price, uint64_t timestamp_ns)
        {
            // Bounds check - programming error
            if (!is_valid_symbol(symbol_id)) [[unlikely]]
            {
                throw std::out_of_range("Invalid symbol_id: " + std::to_string(symbol_id));
            }

            // Input validation - bad data
            if (price <= 0.0 || !std::isfinite(price)) [[unlikely]]
            {
                throw std::invalid_argument("Invalid price: " + std::to_string(price));
            }

            if (quantity == 0) [[unlikely]]
            {
                throw std::invalid_argument("Quantity cannot be zero");
            }

            // Risk Check
            if (!can_execute(symbol_id, quantity, price))
            {
                ++reject_count_;
                return;
            }

            // Update pending quantity
            symbol_data_[symbol_id].pos.pending_quantity_ += quantity;

            // Emit
            uint64_t order_id = generate_order_id();
            event_bus_.emit_order(order_id, symbol_id, quantity, price, timestamp_ns);
            ++order_count_;
        }

        /**
         * @brief Handle a fill for a previously emitted order.
         *
         * Updates pending quantity, position state (quantity, average cost, P&L),
         * and cash, then refreshes the active position tracking.
         *
         * @param symbol_id Integer ID of the filled symbol.
         * @param quantity Signed filled quantity (positive for buy, negative for sell).
         * @param price Fill price.
         *
         * @throws std::out_of_range If @p symbol_id is outside the valid range.
         * @throws std::invalid_argument If @p price is invalid or @p quantity is zero.
         */
        void on_fill(const uint32_t symbol_id, const int32_t quantity, const double price)
        {
            // Bounds check - programming error
            if (!is_valid_symbol(symbol_id)) [[unlikely]]
            {
                throw std::out_of_range("Invalid symbol_id: " + std::to_string(symbol_id));
            }

            // Input validation - bad data
            if (price <= 0.0 || !std::isfinite(price)) [[unlikely]]
            {
                throw std::invalid_argument("Invalid price: " + std::to_string(price));
            }

            if (quantity == 0) [[unlikely]]
            {
                throw std::invalid_argument("Quantity cannot be zero");
            }

            PositionState &pos = symbol_data_[symbol_id].pos;

            // Update pending
            pos.pending_quantity_ -= quantity;

            // Update position and PNL
            update_position_on_fill(symbol_id, quantity, price);

            // Update cash (simple notional change; fees handled elsewhere)
            cash_ -= quantity * price;
            ++fill_count_;

            // Update active tracking
            update_active_status(symbol_id);
        }

        /**
         * @brief Handle a market data update for a symbol.
         *
         * Updates the last traded price for the symbol, which is used for
         * unrealized P&L and exposure calculations.
         *
         * @param symbol_id Integer ID of the symbol.
         * @param last Last traded price (must be positive and finite).
         *
         * @throws std::out_of_range If @p symbol_id is outside the valid range.
         * @throws std::invalid_argument If @p last is invalid.
         */
        void on_market_data(const uint32_t symbol_id, const double last)
        {
            // Bounds check - programming error
            if (!is_valid_symbol(symbol_id)) [[unlikely]]
            {
                throw std::out_of_range("Invalid symbol_id: " + std::to_string(symbol_id));
            }

            // Input validation - bad data
            if (last <= 0.0 || !std::isfinite(last)) [[unlikely]]
            {
                throw std::invalid_argument("Invalid market price: " + std::to_string(last));
            }

            symbol_data_[symbol_id].pos.last_price_ = last;
        }

        /**
         * @brief Compute portfolio-level metrics across all active positions.
         *
         * Aggregates realized and unrealized P&L, gross and net exposure,
         * position count, and total trade count based on current symbol state.
         *
         * @return A @c PortfolioMetric struct with the current summary statistics.
         */
        PortfolioMetric compute_metrics() const noexcept
        {
            PortfolioMetric pm;
            pm.total_trades_ = fill_count_;

            // Update metrics from all positions
            for (size_t i = 0; i < MAX_SYMBOLS; ++i)
            {
                // Only check active positions
                if (active_positions_.test(i))
                {
                    const PositionState &pos = symbol_data_[i].pos;
                    if (pos.quantity_ != 0)
                    {
                        ++pm.num_positions_;
                        pm.unrealized_pnl_ += pos.quantity_ * (pos.last_price_ - pos.average_cost_);
                        pm.gross_exposure_ += std::abs(pos.quantity_ * pos.last_price_);
                        pm.net_exposure_ += pos.quantity_ * pos.last_price_;
                    }
                }
            }

            pm.realized_pnl_ = total_realized_pnl_;
            pm.total_pnl_ = pm.realized_pnl_ + pm.unrealized_pnl_;
            return pm;
        }

        /**
         * @brief Get the position state for a given symbol.
         *
         * @param symbol_id Integer ID of the symbol.
         * @return Const reference to the @c PositionState for the symbol.
         *
         * @throws std::out_of_range If @p symbol_id is outside the valid range.
         */
        const PositionState &get_position(const uint32_t symbol_id) const
        {
            // Bounds check - programming error
            if (!is_valid_symbol(symbol_id)) [[unlikely]]
            {
                throw std::out_of_range("Invalid symbol_id: " + std::to_string(symbol_id));
            }

            return symbol_data_[symbol_id].pos;
        }

        /**
         * @brief Compute unrealized P&L for a specific symbol.
         *
         * Uses the current position quantity, average cost and last price
         * to compute the mark-to-market P&L.
         *
         * @param symbol_id Integer ID of the symbol.
         * @return Unrealized P&L for the symbol.
         *
         * @throws std::out_of_range If @p symbol_id is outside the valid range.
         */
        double get_unrealized_pnl(const uint32_t symbol_id) const
        {
            // Bounds check - programming error
            if (!is_valid_symbol(symbol_id)) [[unlikely]]
            {
                throw std::out_of_range("Invalid symbol_id: " + std::to_string(symbol_id));
            }

            const PositionState &pos = symbol_data_[symbol_id].pos;
            return pos.quantity_ * (pos.last_price_ - pos.average_cost_);
        }

        /**
         * @brief Get the current cash balance.
         *
         * @return Cash available in the portfolio.
         */
        double get_cash() const noexcept
        {
            return cash_;
        }

        /**
         * @brief Compute the total portfolio value (cash + positions).
         *
         * Sums the mark to market value of all active positions and adds cash.
         *
         * @return Total portfolio value.
         */
        double get_total_value() const noexcept
        {
            double positions_value = 0.0;

            // Only iterate over active positions
            for (size_t i = 0; i < MAX_SYMBOLS; ++i)
            {
                if (active_positions_.test(i))
                {
                    const PositionState &pos = symbol_data_[i].pos;
                    positions_value += pos.quantity_ * pos.last_price_;
                }
            }

            return cash_ + positions_value;
        }

        /**
         * @brief Get the total number of orders emitted.
         *
         * @return Count of orders created via @c on_signal().
         */
        uint32_t get_order_count() const noexcept
        {
            return order_count_;
        }

        /**
         * @brief Get the total number of fills processed.
         *
         * @return Count of fills handled via @c on_fill().
         */
        uint32_t get_fill_count() const noexcept
        {
            return fill_count_;
        }

        /**
         * @brief Get the number of rejected orders.
         *
         * @return Count of signals that failed risk checks.
         */
        uint32_t get_reject_count() const noexcept
        {
            return reject_count_;
        }

        /**
         * @brief Set risk limits for a specific symbol.
         *
         * @param symbol_id Integer ID of the symbol.
         * @param limit New @c RiskLimits to apply.
         *
         * @throws std::out_of_range If @p symbol_id is outside the valid range.
         */
        void set_risk_limit(const uint32_t symbol_id, const RiskLimits &limit)
        {
            // Bounds check - programming error
            if (!is_valid_symbol(symbol_id)) [[unlikely]]
            {
                throw std::out_of_range("Invalid symbol_id: " + std::to_string(symbol_id));
            }

            symbol_data_[symbol_id].risk = limit;
        }

        /**
         * @brief Get the risk limits for a specific symbol.
         *
         * @param symbol_id Integer ID of the symbol.
         * @return Const reference to the @c RiskLimits for the symbol.
         *
         * @throws std::out_of_range If @p symbol_id is outside the valid range.
         */
        const RiskLimits &get_risk_limit(const uint32_t symbol_id) const
        {
            // Bounds check - programming error
            if (!is_valid_symbol(symbol_id)) [[unlikely]]
            {
                throw std::out_of_range("Invalid symbol_id: " + std::to_string(symbol_id));
            }

            return symbol_data_[symbol_id].risk;
        }

        /**
         * @brief Check whether there is an active (non zero) position for a symbol.
         *
         * @param symbol_id Integer ID of the symbol.
         * @return true if the symbol has a non zero position, false otherwise.
         */
        bool has_position(const uint32_t symbol_id) const noexcept
        {
            return is_valid_symbol(symbol_id) && active_positions_.test(symbol_id);
        }

    private:
        /**
         * @brief Fast pre-trade risk check for a potential order.
         *
         * Validates order size, resulting position size, notional exposure and
         * cash availability against configured risk limits for the symbol.
         *
         * @param symbol_id Integer ID of the symbol.
         * @param quantity Signed order quantity being requested.
         * @param price Limit price of the order.
         * @return true if the order passes all risk checks, false otherwise.
         */
        bool can_execute(const uint32_t symbol_id, const int32_t quantity, const double price) const noexcept
        {
            // Fast risk check
            const SymbolData &sym = symbol_data_[symbol_id];
            const PositionState &pos = sym.pos;
            const RiskLimits &risk = sym.risk;

            // Compute values upfront
            int32_t abs_quantity = std::abs(quantity);
            int32_t new_position = pos.quantity_ + pos.pending_quantity_ + quantity;
            int32_t abs_new_position = std::abs(new_position);
            double cost = quantity * price;

            // Branchless comparisons
            bool order_size_ok = abs_quantity <= risk.max_order_size_;
            bool position_ok = abs_new_position <= risk.max_positions_;
            bool notional_ok = (abs_new_position * price) <= risk.max_notional_;
            bool cash_ok = (quantity <= 0) | (cost <= cash_);

            return order_size_ok & position_ok & notional_ok & cash_ok;
        }

        /**
         * @brief Update position state in response to a fill.
         *
         * Adjusts position quantity, average cost and realized P&L based on
         * the signed fill quantity. Handles adding, partial closing and
         * full reversal of positions.
         *
         * @param symbol_id Integer ID of the symbol being updated.
         * @param quantity Signed filled quantity.
         * @param price Fill price.
         */
        void update_position_on_fill(const uint32_t symbol_id, const int32_t quantity, const double price) noexcept
        {
            // symbol_id already validated by caller
            PositionState &pos = symbol_data_[symbol_id].pos;

            // Store original quantity to detect reversals
            const int32_t old_qty = pos.quantity_;
            const int32_t new_qty = old_qty + quantity;

            // Check direction
            const bool is_closing = (old_qty ^ quantity) < 0;

            // Closing path calculations
            const int32_t closed_qty = std::min(std::abs(quantity), std::abs(old_qty));
            const int32_t sign = (old_qty > 0) ? 1 : -1;
            const double pnl_per_share = sign * (price - pos.average_cost_);
            const double realized_pnl_delta = closed_qty * pnl_per_share * is_closing;

            // Adding path calculations
            const double new_vwap = (new_qty != 0) ? (old_qty * pos.average_cost_ + quantity * price) / new_qty : pos.average_cost_;

            // Position reversal check
            const bool reversed = ((old_qty ^ new_qty) < 0) && (new_qty != 0);

            // Apply updates
            pos.realized_pnl_ += realized_pnl_delta;
            total_realized_pnl_ += realized_pnl_delta;
            pos.average_cost_ = reversed ? price : (is_closing ? pos.average_cost_ : new_vwap);
            pos.quantity_ = new_qty;
        }

        /**
         * @brief Generate a new unique order ID.
         *
         * Uses an atomic counter to produce monotonically increasing IDs.
         *
         * @return Newly generated order ID.
         */
        uint64_t generate_order_id() noexcept
        {
            return next_order_id.fetch_add(1, std::memory_order_relaxed);
        }

        /**
         * @brief Check whether a symbol ID falls within the valid range.
         *
         * @param symbol_id Integer ID to check.
         * @return true if @p symbol_id < MAX_SYMBOLS, false otherwise.
         */
        bool is_valid_symbol(const uint32_t symbol_id) const noexcept
        {
            return symbol_id < MAX_SYMBOLS;
        }

        /**
         * @brief Update the active-position bitset for a given symbol.
         *
         * Sets or clears the bit for @p symbol_id depending on whether
         * the position quantity is non-zero.
         *
         * @param symbol_id Integer ID of the symbol to update.
         */
        void update_active_status(const uint32_t symbol_id) noexcept
        {
            // symbol_id already validated by caller
            if (symbol_data_[symbol_id].pos.quantity_ != 0)
            {
                active_positions_.set(symbol_id);
            }
            else
            {
                active_positions_.reset(symbol_id);
            }
        }

        /**
         * @brief Per-symbol state combining position and risk configuration.
         */
        struct SymbolData
        {
            PositionState pos; ///< Position state (quantity, P&L, prices).
            RiskLimits risk;   ///< Risk limits for this symbol.

            SymbolData() : pos(), risk() {}
        };

        EventBus &event_bus_;                                         ///< Event bus used to emit orders.
        alignas(64) std::array<SymbolData, MAX_SYMBOLS> symbol_data_; ///< Per symbol position and risk .
        std::bitset<MAX_SYMBOLS> active_positions_;                   ///< Bitset indicating which symbols currently have a non zero position..
        double cash_;                                                 ///< Current cash balance.
        double initial_capital_;                                      ///< Initial capital used to seed the portfolio.
        double total_realized_pnl_;                                   ///< Accumulated realized PNL.
        uint64_t order_count_;                                        ///< Number of orders emitted.
        uint64_t fill_count_;                                         ///< Number of fills processed.
        uint64_t reject_count_;                                       ///< Number of orders rejected by risk checks.
        std::atomic<uint64_t> next_order_id;                          ///< Atomic unique order ID.
    };

} // namespace engine::portfolio
