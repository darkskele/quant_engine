#pragma once

#include <string>

#include "events/event.hpp"

namespace engine::orders
{

    /**
     * @brief State of an order as it moves through execution.
     *
     * Holds an immutable order_event and mutable fill progress.
     */
    struct order_state
    {
        const engine::events::order_event order_; ///< Immutable client order
        int64_t filled_qty_{0};                   ///< Cumulative filled
        double avg_fill_price_{0.0};              ///< Weighted average fill price

        explicit order_state(const engine::events::order_event &order,
                             int64_t filled = 0,
                             double avg = 0.0)
            : order_(order), filled_qty_(filled), avg_fill_price_(avg)
        {
        }
    };

} // namespace engine::orders