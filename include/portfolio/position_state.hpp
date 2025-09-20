#pragma once

#include <cstdint>

namespace portfolio
{

    /**
     * @brief Represents the state of a trading position for a single instrument.
     *
     * Tracks the current average entry price, cumulative realized profit and loss,
     * and net quantity (long or short) of the position. Intended to be stored in a
     * map keyed by instrument symbol.
     */
    struct position_state
    {
        double avg_price;    ///< Weighted average entry price of the open position.
        double realized_pnl; ///< Realized profit or loss accumulated from closed trades.
        int64_t quantity;    ///< Net quantity: positive for long, negative for short, zero if flat.
    };

} // namespace portfolio