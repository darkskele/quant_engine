#pragma once

#include "orders/order_state.hpp"
#include "streamer/buffers/revolving_ring_buffer.hpp"

#include <set>
#include <unordered_map>
#include <queue>
#include <deque>

namespace engine::orders
{
    /**
     * @brief Custom queue for orders. Uses custom comparators for bids and asks and a back index
     * to maintain O(1) insert and erase.
     */
    class order_queue
    {
    private:
        /**
         * @brief Comparator for buy orders. Orders by higher price and timestamp as tiebreaker.
         */
        struct bid_cmp
        {
            bool operator()(const order_state &lhs, const order_state &rhs) const noexcept
            {
                // Compare by order
                const auto &o1 = lhs.order_;
                const auto &o2 = rhs.order_;
                if (o1.price_ != o2.price_)
                {
                    return o1.price_ > o2.price_;
                }
                // Tie break on timestamp
                return o1.timestamp_ < o2.timestamp_; // earliest first
            }
        };

        /**
         * @brief Comparator for ask orders. Orders by lower price and timestamp as tiebreaker.
         */
        struct ask_cmp
        {
            bool operator()(const order_state &lhs, const order_state &rhs) const noexcept
            {
                // Compare by order
                const auto &o1 = lhs.order_;
                const auto &o2 = rhs.order_;
                if (o1.price_ != o2.price_)
                {
                    return o1.price_ < o2.price_;
                }
                // Tie break on timestamp
                return o1.timestamp_ < o2.timestamp_; // earliest first
            }
        };

        /// @brief Ordered set aliases for different book types.
        using bid_container = std::multiset<order_state, bid_cmp>;
        using ask_container = std::multiset<order_state, ask_cmp>;
        /// @brief Aliases for iterators.
        using bid_iterator = bid_container::iterator;
        using ask_iterator = ask_container::iterator;
        /// @brief Alias for historical ledger.
        using historical_container = btc_stream::streamer::buffers::revolving_recency_buffer<order_state>;

    public:
        /// @brief Default destructor, resources are already RAII compliant.
        order_queue() = default;
        ~order_queue() = default;
        /// @brief No copies, but allow moves.
        order_queue(const order_queue &) = delete;
        order_queue &operator=(const order_queue &) = delete;
        order_queue(order_queue &&) = default;
        order_queue &operator=(order_queue &&) = default;

        /// @brief Getters.
        const bid_container &bids() noexcept { return bids_; }
        const ask_container &asks() noexcept { return asks_; }
        size_t size() const noexcept { return bids_.size() + asks_.size(); }
        bool empty() const noexcept { return bids_.empty() && asks_.empty(); }
        const order_state &best_bid() const noexcept { return *bids_.begin(); };
        const order_state &best_ask() const noexcept { return *asks_.begin(); };
        const historical_container &ledger() const noexcept { return historical_ledger_; }

        /**
         * @brief Emplaces or replaces new order state.
         * @param state Order state.
         */
        void emplace(order_state &&state);

        /**
         * @brief Convenience overload to construct from event.
         * @param ev Order event.
         */
        void emplace(const events::order_event &ev) noexcept { return emplace(order_state{ev}); }

        /**
         * @brief Gets order by ID. Uses const cast to remove constness, ordering remains in tact as
         * member constness of ordering keys are enforced in struct definition.
         * @param id Order id to get.
         * @return Pointer to constant order state, or nullptr.
         */
        order_state *get(const std::string &id) noexcept;

        /**
         * @brief Const getter.
         * @param id Order id to get.
         * @return Pointer to constant order state, or nullptr.
         */
        const order_state *get(const std::string &id) const noexcept;

        /**
         * @brief Inactivates and deletes order state from queue.
         * @param id Order id to remove.
         */
        void inactive(const std::string &id) noexcept;

        /**
         * @brief Iteration ergonomic helper for processing both bids and asks. Iteration
         * is pruned by callable success.
         * 
         * @tparam Fn Callable type, must except order state and return bool.
         * 
         * @param fn Callable.
         */
        template <typename Fn>
        void for_each_pruned(Fn&& fn) {
            for (auto& b : bids_)
            {
                if(!fn(b)) break;
            }
            for (auto& a : asks_)
            {
                if(!fn(a)) break;
            }
        }

    private:
        std::unordered_map<std::string, bid_iterator> bid_index_;  // Bids back index
        std::unordered_map<std::string, ask_iterator> asks_index_; // Asks back index
        bid_container bids_;                                       // Ordered Bids set
        ask_container asks_;                                       // Ordered Asks set
        historical_container historical_ledger_;                   // Historical ledgers
    };

} // namespace engine::orders
