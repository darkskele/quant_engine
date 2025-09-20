#pragma once

#include <optional>

#include "events/event.hpp"
#include "events/event_queue.hpp"
#include "portfolio/portfolio_manager.hpp"

namespace engine
{

    /**
     * @brief CRTP base class for backtest/live engines
     *
     * Provides the generic loop and dispatch logic.
     *
     * @tparam Derived Concrete engine type.
     * @tparam Streamer Market data source.
     * @tparam Strategy Strategy type (implements on_market/on_signal).
     * @tparam ExecHandler Execution handler type.
     */
    template <typename Derived, typename Streamer, typename Strategy, typename ExecHandler>
    class engine_base
    {
    public:
        /**
         * @brief Construct a new engine_base.
         * @param streamer Market data streamer.
         * @param strategy Implemented trading strategy.
         * @param portfolio_manager Portfolio manager.
         * @param exec_handler Execution engine to respond to strategy signals.
         */
        engine_base(Streamer &&streamer,
                    Strategy &&strategy,
                    portfolio::portfolio_manager &&portfolio_manager,
                    ExecHandler &&exec_handler)
            : streamer_(std::move(streamer)),
              strategy_(std::move(strategy)),
              portfolio_manager_(std::move(portfolio_manager)),
              exec_handler_(std::move(exec_handler))
        {
        }

        /**
         * @brief Main engine loop.
         */
        void run()
        {
            auto &self = derived();

            // Check engine is running
            while (!self.should_stop())
            {
                // Poll streamer for next market event
                auto ev = poll_streamer();
                if (!ev.has_value())
                {
                    // Decides to continue
                    if (!self.handle_no_event())
                    {
                        break;
                    }
                }
                else
                {
                    handle_event(*ev);
                }

                // Drain queue
                while (!queue_.empty())
                {
                    auto sub_ev = queue_.pop();
                    handle_event(sub_ev);
                }
            }
        }

        /**
         * @brief Getter for const portfolio manager.
         */
        const portfolio::portfolio_manager &portfolio_manager() const noexcept
        {
            return portfolio_manager_;
        }

        /**
         * @brief Getter for portfolio manager.
         */
        portfolio::portfolio_manager &portfolio_manager() noexcept
        {
            return portfolio_manager_;
        }

    protected:
        /// Poll streamer and wrap into a market_event
        std::optional<events::event> poll_streamer()
        {
            // Check streamer for data
            if (auto tick = streamer_.next())
            {
                return events::market_event{*tick};
            }
            return std::nullopt;
        }

        /// Dispatch event to the correct component
        void handle_event(events::event &ev)
        {
            // Get variant type and handle
            std::visit([&](auto &e)
                       {
                using T = std::decay_t<decltype(e)>;

                if constexpr(std::is_same_v<T, events::market_event>)
                {
                    strategy_.on_market(e, queue_);
                    portfolio_manager_.on_market(e.tick_.symbol, e.tick_.price);
                }
                else if constexpr(std::is_same_v<T, events::signal_event>)
                {
                    strategy_.on_signal(e, queue_);
                }
                else if constexpr(std::is_same_v<T, events::order_event>)
                {
                    exec_handler_.on_order(e, queue_);
                }
                else if constexpr(std::is_same_v<T, events::fill_event>)
                {
                    portfolio_manager_.on_fill(e);
                } });
        }

    private:
        /// Internal getter for derived.
        Derived &derived()
        {
            return static_cast<Derived &>(*this);
        }
        Streamer streamer_;                              ///< Streamer object for market access.
        Strategy strategy_;                              ///< Trading strategy implementation.
        portfolio::portfolio_manager portfolio_manager_; ///< Portfolio manager.
        ExecHandler exec_handler_;                       ///< Execution handler.
        events::event_queue queue_;                      ///< Event queue.
    };

} // namespace engine
