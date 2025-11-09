#pragma once

#include <cstdint>

namespace engine::portfolio
{

    struct alignas(64) PositionState
    {
        int32_t quantity_;         // Current position
        int32_t pending_quantity_; // Orders in flight
        double average_cost_;      // VWAP entry price
        double realized_pnl_;      // Closed P&L
        double last_price_;        // For mark to market

        PositionState()
            : quantity_(0), pending_quantity_(0), average_cost_(0.0), realized_pnl_(0.0), last_price_(0.0) {}
    };

} // namespace engine::portfolio
