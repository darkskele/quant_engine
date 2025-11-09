#pragma once

#include <cstdint>

namespace engine::portfolio
{

    struct alignas(64) RiskLimits
    {
        int32_t max_positions_;  // Max absolute position
        int32_t max_order_size_; // Max single order size
        double max_notional_;  // Max £ value

        RiskLimits()
            : max_positions_(1000), max_order_size_(100), max_notional_(1e6) {}
    };

} // namespace engine::portfolio
