#pragma once

#include <optional>
#include <thread>

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
            size_t tick_count = 0;

            // Check engine is running
            while (!self.should_stop())
            {
                auto loop_start = std::chrono::high_resolution_clock::now();

                try
                {
                    // Pause loop
                    while (is_paused())
                    {
                        std::this_thread::yield();
                        if (self.should_stop())
                            return;
                    }

                    // Poll streamer for next market event
                    if (auto ev = poll_streamer())
                    {
                        ++tick_count;
                        handle_event(*ev);
                    }
                    else
                    {
                        // Decides to continue
                        if (!self.handle_no_event())
                        {
                            break;
                        }
                    }

                    // Drain queue
                    while (!queue_.empty())
                    {
                        auto sub_ev = queue_.pop();
                        handle_event(sub_ev);
                    }
                }
                catch (const std::exception &ex)
                {
                    self.on_error(ex); // default rethrow
                }

                // Log metrics
                auto loop_end = std::chrono::high_resolution_clock::now();
                self.on_loop_metrics(
                    tick_count,
                    std::chrono::duration_cast<std::chrono::nanoseconds>(loop_end - loop_start));
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

        /**
         * @brief Getter for const strategy.
         */
        const Strategy &strategy() const noexcept
        {
            return strategy_;
        }

        /**
         * @brief Gett for const execution handler.
         */
        const ExecHandler &exec_handler() const noexcept
        {
            return exec_handler_;
        }

        /**
         * @brief Pause streaming.
         */
        void pause() noexcept
        {
            paused_.store(true, std::memory_order_relaxed);
        }

        /**
         * @brief Resume streaming.
         */
        void resume() noexcept
        {
            paused_.store(false, std::memory_order_relaxed);
        }

        /**
         * @brief Get paused flag.
         */
        bool is_paused() const noexcept
        {
            return paused_.load(std::memory_order_relaxed);
        }

    protected:
        /// Poll streamer and wrap into a market_event
        std::optional<events::event> poll_streamer()
        {
            // Check streamer for data
            if (auto tick = streamer_.next())
            {
                return events::market_event{
                    std::move(tick->symbol),
                    tick->price,
                    tick->qty,
                    tick->timestamp_ms,
                    tick->is_buyer_match};
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
                    // Update portfolio with new market price
                    portfolio_manager_.on_market(e.symbol_, e.price_);

                    // Let execution handler re check resting orders
                    exec_handler_.on_market(e, queue_);

                    // Strategy reacts to the market
                    strategy_.on_market(e, queue_);
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
                } }, ev);
        }

        /// Default error handler
        void on_error(const std::exception &ex)
        {
            throw ex; // rethrow by default
        }

        /// Metrics hook, overriden by derived
        void on_loop_metrics(size_t, std::chrono::nanoseconds)
        {
            // empty
        }

        std::atomic<bool> paused_; ///< Atomic pause flag.

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
