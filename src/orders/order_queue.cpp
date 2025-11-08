#include "orders/order_queue.hpp"



namespace engine::orders
{

    void order_queue::emplace(order_state &&state)
    {
        auto &order = state.order_;
        if (order.is_buy_)
        {
            // Delete from index if exists, defensive
            if (auto it = bid_index_.find(order.order_id_); it != bid_index_.end())
            {
                bids_.erase(it->second);
                bid_index_.erase(it);
            }
            // Add back into index and set
            auto iter = bids_.insert(std::move(state));
            bid_index_[order.order_id_] = iter;
        }
        else
        {
            // Delete from index if exists, defensive
            if (auto it = asks_index_.find(order.order_id_); it != asks_index_.end())
            {
                asks_.erase(it->second);
                asks_index_.erase(it);
            }
            // Add back into index and set
            auto iter = asks_.insert(std::move(state));
            asks_index_[order.order_id_] = iter;
        }
    }

    order_state *order_queue::get(const std::string &id) noexcept
    {
        // Get from index.
        if (auto it = bid_index_.find(id); it != bid_index_.end())
        {
            return const_cast<order_state *>(&*it->second);
        }
        if (auto it = asks_index_.find(id); it != asks_index_.end())
        {
            return const_cast<order_state *>(&*it->second);
        }
        // Couldn't be found
        return nullptr;
    }

    const order_state *order_queue::get(const std::string &id) const noexcept
    {
        if (auto it = bid_index_.find(id); it != bid_index_.end())
        {
            return &*it->second;
        }
        if (auto it = asks_index_.find(id); it != asks_index_.end())
        {
            return &*it->second;
        }
        return nullptr;
    }

    void order_queue::inactive(const std::string &id) noexcept
    {
        // Erase from both index and set
        if (auto it = bid_index_.find(id); it != bid_index_.end())
        {
            historical_ledger_.emplace(*it->second);
            bids_.erase(it->second);
            bid_index_.erase(it);
            return;
        }
        if (auto it = asks_index_.find(id); it != asks_index_.end())
        {
            historical_ledger_.emplace(*it->second);
            asks_.erase(it->second);
            asks_index_.erase(it);
        }
    }
} // namespace engine::orders