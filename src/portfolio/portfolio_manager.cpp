#include "portfolio/portfolio_manager.hpp"

#include <algorithm>

namespace engine::portfolio
{

    portfolio_manager::portfolio_manager(double starting_cash, double commission_rate, double slippage_rate)
        : cash_(starting_cash), realized_pnl_(0), commission_rate_(commission_rate), slippage_rate_(slippage_rate) {}

    void portfolio_manager::on_fill(const engine::events::fill_event &fill) noexcept
    {
        // Get new quantity
        auto &pos = positions_[fill.symbol_];
        int64_t signed_qty = fill.is_buy_ ? fill.filled_qty_ : -fill.filled_qty_;

        // Adjust for slippage
        double effective_price = fill.fill_price_;
        if (slippage_rate_ > 0.0)
        {
            effective_price *= (fill.is_buy_ ? (1.0 + slippage_rate_) : (1.0 - slippage_rate_));
        }

        // Commission
        double trade_value = effective_price * fill.filled_qty_;
        double commission = trade_value * commission_rate_;
        cash_ -= commission; // commission always reduces cash

        // Cash for trade adjustment
        if (fill.is_buy_)
        {
            cash_ -= trade_value;
        }
        else
        {
            cash_ += trade_value;
        }

        // Same side position (avg in)
        if ((pos.quantity >= 0 && signed_qty > 0) || // long + buy more
            (pos.quantity <= 0 && signed_qty < 0))   // short + sell more
        {
            double old_cost = pos.avg_price * std::abs(pos.quantity);
            double new_cost = effective_price * std::abs(signed_qty);
            pos.quantity += signed_qty;
            pos.avg_price = (old_cost + new_cost) / std::abs(pos.quantity);
        }
        // Closing or flipping
        else
        {
            int closing_qty = std::min(std::abs(pos.quantity), std::abs(signed_qty));
            double pnl = closing_qty * (effective_price - pos.avg_price) * (pos.quantity > 0 ? 1 : -1);
            realized_pnl_ += pnl;

            int old_qty = pos.quantity;
            pos.quantity += signed_qty;

            // If flipped sides, reset cost basis to trade price
            if (pos.quantity == 0)
            {
                pos.avg_price = 0; // flat
            }
            else if ((old_qty > 0 && pos.quantity < 0) || (old_qty < 0 && pos.quantity > 0))
            {
                // flipped sides
                pos.avg_price = effective_price;
            }
        }

        // Log fill
        trade_log_.push_back(fill);
    }

    void portfolio_manager::on_market(const std::string &symbol, double price) noexcept
    {
        // Track market price
        market_prices_[symbol] = price;
    }

    double portfolio_manager::unrealized_pnl() const noexcept
    {
        double total = 0.0;
        for (const auto &[symbol, pos] : positions_)
        {
            // Update total pnl from market prices
            auto it = market_prices_.find(symbol);
            if (it != market_prices_.end())
            {
                double mkt_price = it->second;
                total += pos.quantity * (mkt_price - pos.avg_price);
            }
        }
        return total;
    }

    double portfolio_manager::realized_pnl() const noexcept
    {
        return realized_pnl_;
    }

    double portfolio_manager::total_equity() const noexcept
    {
        double value = cash_;
        for (const auto [symbol, pos] : positions_)
        {
            auto it = market_prices_.find(symbol);
            if (it != market_prices_.end())
            {
                value += pos.quantity * it->second;
            }
        }
        return value;
    }

    double portfolio_manager::cash_balance() const noexcept
    {
        return cash_;
    }

    const position_state &portfolio_manager::position(const std::string &symbol) const noexcept
    {
        static position_state empty{};
        auto it = positions_.find(symbol);
        return it != positions_.end() ? it->second : empty;
    }

    const std::vector<engine::events::fill_event> &portfolio_manager::trade_log() const noexcept
    {
        return trade_log_;
    }

} // namespace engine::portfolio
