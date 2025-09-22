#pragma once

#include "events/event.hpp"
#include "portfolio/position_state.hpp"

#include <string>
#include <vector>
#include <unordered_map>

namespace engine::portfolio
{

    /**
     * @brief Tracks portfolio state: positions, cash, and PnL.
     */
    class portfolio_manager
    {
    public:
        /**
         * @brief Construct a new portfolio_manager .
         * @param starting_cash Initial cash balance (e.g., 100000 USD).
         * @param commission_rate % of trade notational.
         * @param slippage_rate % price adjustment.
         */
        portfolio_manager(double starting_cash = 100000.0,
                          double commission_rate = 0.0);

        /**
         * @brief Handle a fill_event (executed trade).
         * @param fill The fill to apply to the portfolio.
         */
        void on_fill(const engine::events::fill_event &fill) noexcept;

        /**
         * @brief Handle a MarketEvent (price update).
         * @param symbol The asset symbol (e.g., BTCUSD).
         * @param price The current market price of the asset.
         * @param price The current market quantity of the asset.
         */
        void on_market(const std::string &symbol, double price, double qty) noexcept;

        /**
         * @brief Handles cancel event (cancelled orders).
         * @param cancel Cancel event.
         */
        void on_cancel(const engine::events::cancel_event &cancel) noexcept;

        /**
         * @brief Get current total equity (cash + value of holdings).
         */
        double total_equity() const noexcept;

        /**
         * @brief Get current unrealized PnL.
         */
        double unrealized_pnl() const noexcept;

        /**
         * @brief Get current realized Pnl;
         */
        double realized_pnl() const noexcept;

        /**
         * @brief Get current cash balance.
         */
        double cash_balance() const noexcept;

        /**
         * @brief Gets specified position.
         *
         * @param symbol Position symbol to get.
         * @return Retrieved position.
         */
        const position_state &position(const std::string &symbol) const noexcept;

        /**
         * @brief Gets trade log.
         */
        const std::vector<engine::events::fill_event> &trade_log() const noexcept;

        /**
         * @brief Get last market price.
         *
         * @param symbol Market to check.
         * @return If symbol doesn't exist will return 0.0.
         */
        double last_price(const std::string &symbol) const noexcept;

        /**
         * @brief Get last market quantity.
         *
         * @param symbol Market to check.
         * @return If symbol doesn't exist will return 0.0.
         */
        double last_quantity(const std::string &symbol) const noexcept;

        /**
         * @brief Get cancel count.
         */
        size_t cancel_count() const noexcept {return cancel_count_;};

        /**
         * @brief Get const ref to canceled order ids.
         */
        const std::vector<std::string>& cancelled_order_ids() const noexcept {return cancelled_order_ids_; }

    private:
        double cash_;                                               ///< Available cash balance in the account (after trades and fees).
        double realized_pnl_;                                       ///< Total realized profit and loss across all positions.
        double commission_rate_;                                    ///< Commission fee rate applied to each trade (e.g. 0.001 = 0.1%).
        std::unordered_map<std::string, position_state> positions_; ///< Current open positions keyed by symbol.
        std::unordered_map<std::string, double> market_prices_;     ///< Last known market price per symbol.
        std::unordered_map<std::string, double> market_quantities;  ///< Last known market quantity per symbol.
        std::vector<engine::events::fill_event> trade_log_;         ///< Log of trades across the portfolio.
        std::vector<std::string> cancelled_order_ids_;              ///< Log of cancelled order ids.
        size_t cancel_count_;                                       ///< Count of cancelled orders.
    };

} // namespace engine::portfolio
