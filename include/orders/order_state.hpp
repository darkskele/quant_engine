#pragma once

#include <string>

#include "events/event.hpp"

namespace engine::orders
{

    /**
     * @brief Order state for execution.
     */
    struct order_state
    {
        events::order_event order_; ///< Original order.
        int64_t filled_qty_;        ///< Cumulative filled.
        double avg_fill_price_;     ///< Weighted avg fill price.
        bool is_active_;            ///< Still working or fully closed.
    };

} // namespace engine::orders