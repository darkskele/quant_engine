#pragma once

#include "orders/order_state.hpp"
#include "orders/order_queue.hpp"
#include "events/event.hpp"
#include "events/event_queue.hpp"

#include <unordered_map>

namespace engine
{

    /**
     * @brief CRPT base for execution for engines.
     */
    template <typename Derived>
    class execution_engine_base
    {
    public:
        /**
         * @brief Executes order and returns fill event to event queue.
         *
         * @param order Order event from strategy.
         * @param queue Event queue.
         */
        void on_order(const events::order_event &order,
                      events::event_queue &queue)
        {
            // Dispatch to derived class
            derived()->on_order(order, queue);
        }

        /**
         * @brief Lookup an order's state by its ID.
         *
         * @param order_id Unique order identifier.
         * @return const order_state* Pointer to the order state if found,
         *         or nullptr if no such order exists.
         */
        const engine::orders::order_state *get_order(const std::string &order_id) const noexcept
        {
            auto ord = orders_.get(order_id);
            return ord;
        }

    protected:
        /// @brief Can't instantiate base directly.
        execution_engine_base() = default;

        /**
         * @brief Emits a fill event and updates order state.
         *
         * @param order Order event.
         * @param filled_qty Filled portion of order.
         * @param exec_price Price of execution order.
         * @param queue Queue to add fill to.
         * @param time_stamp Time stamp of order.
         */
        void emit_fill(const events::order_event &order,
                       int64_t filled_qty,
                       double exec_price,
                       events::event_queue &queue,
                       std::chrono::system_clock::time_point time_stamp = std::chrono::system_clock::now())
        {
            // Update order state
            auto st = orders_.get(order.order_id_);
            if (!st)
            {
                // First time we've seen this order
                orders_.emplace({order});
                st = orders_.get(order.order_id_);
            }

            // Update fill progress
            st->filled_qty_ += filled_qty;

            if (st->filled_qty_ > 0)
            {
                st->avg_fill_price_ =
                    ((st->avg_fill_price_ * static_cast<double>(st->filled_qty_ - filled_qty)) + (exec_price * static_cast<double>(filled_qty))) / static_cast<double>(st->filled_qty_);
            }
            else
            {
                st->avg_fill_price_ = 0.0; // guard for zero division
            }

            if (st->filled_qty_ >= st->order_.quantity_)
            {
                orders_.inactive(st->order_.order_id_);
            }

            events::fill_event fill{
                order.symbol_,
                order.order_id_,
                filled_qty,
                order.quantity_, // total order size
                order.is_buy_,
                exec_price,
                order,
                time_stamp};

            queue.push(std::move(fill));
        }

        /**
         * @brief Emits a cancel event for orders cancelled by IOC or FOK rules.
         *
         * @param order Order to cancel.
         * @param reason Reason for cancel.
         * @param queue Queue to add event to.
         */
        void emit_cancel(const events::order_event &order, const std::string &reason, events::event_queue &queue) noexcept
        {
            // Make order inactive
            orders_.inactive(order.order_id_);

            // Emit cancel
            events::cancel_event cancel{
                order,
                reason};

            queue.push(std::move(cancel));
        }

        orders::order_queue orders_; ///< Order state tracking.

    private:
        /// Internal getter for derived.
        Derived &derived()
        {
            return static_cast<Derived &>(*this);
        }
    };

} // namespace engine
