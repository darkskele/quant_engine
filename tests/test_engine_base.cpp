#include <gtest/gtest.h>
#include "engine_base.hpp"
#include "events/event.hpp"
#include "portfolio/portfolio_manager.hpp"

using namespace engine;
using namespace engine::events;
using namespace portfolio;

struct tick_data
{
    std::string symbol;   ///< Trade symbol.
    double price;         ///< Trade price at the time of the tick.
    double qty;           ///< Quantity of the base asset traded.
    int64_t timestamp_ms; ///< Epoch timestamp of the trade in milliseconds.
    bool is_buyer_match;  ///< True if the buyer initiated the trade (i.e., aggressive buy).
};

// Dummy streamer: feeds a fixed sequence of ticks
struct DummyStreamer
{
    std::vector<tick_data> ticks;
    size_t index = 0;

    std::optional<tick_data> next()
    {
        if (index < ticks.size())
            return ticks[index++];
        return std::nullopt;
    }
};

// Dummy strategy: records calls and pushes a signal/order into the queue
struct DummyStrategy
{
    bool saw_market = false;
    bool saw_signal = false;

    void on_market(const market_event &, event_queue &q)
    {
        saw_market = true;
        // Push a fake signal after market
        q.push(signal_event{});
    }

    void on_signal(const signal_event &, event_queue &q)
    {
        saw_signal = true;
        // Push a dummy order to keep the pipeline flowing
        q.push(order_event{"BTCUSD", "1", 1, true, 100.0, order_type::Limit, order_flags::FOK});
    }

    void on_cancel(const cancel_event&)
    {
        // empty
    }
};

static order_event order_ev{"BTCUSD",
                     "order",
                     2,
                     false,
                     200.0,
                     order_type::Market,
                     order_flags::None,
                     std::chrono::system_clock::now(),
                     market_event{"BTCUSD", 200.0, 2, 1, false}};

// Dummy execution handler: records orders
struct DummyExec
{
    bool saw_order = false;

    void on_order(const order_event &order, event_queue &q)
    {
        saw_order = true;
        q.push(fill_event{order.symbol_, "1", order.quantity_, order.quantity_, order.is_buy_, order.price_, order_ev});
    }

    void on_market(const market_event &, event_queue &)
    {
        // empty
    }
};

// Minimal derived engine
struct TestEngine
    : public engine_base<TestEngine, DummyStreamer, DummyStrategy, DummyExec>
{
    using Base = engine_base<TestEngine, DummyStreamer, DummyStrategy, DummyExec>;
    using Base::Base;

    bool should_stop() { return stop_; }
    bool handle_no_event() { return false; }

    bool stop_ = false;
};

TEST(EngineBaseTest, MarketEventFlowTriggersStrategyAndSignal)
{
    DummyStreamer streamer{{tick_data{"BTCUSD", 100.0, 1.0, 12345, false}}};
    DummyStrategy strat;
    portfolio_manager pf(1000.0);
    DummyExec exec;

    TestEngine engine{std::move(streamer), std::move(strat), std::move(pf), std::move(exec)};
    engine.run();

    EXPECT_TRUE(engine.strategy().saw_market);
    EXPECT_TRUE(engine.strategy().saw_signal);
}

TEST(EngineBaseTest, OrderAndFillFlowUpdatesPortfolio)
{
    DummyStreamer streamer{{tick_data{"BTCUSD", 100.0, 1.0, 12345, false}}};
    DummyStrategy strat;
    portfolio_manager pf(1000.0);
    DummyExec exec;

    TestEngine engine{std::move(streamer), std::move(strat), std::move(pf), std::move(exec)};
    engine.run();

    EXPECT_TRUE(engine.exec_handler().saw_order);

    // Portfolio should reflect the fill
    EXPECT_EQ(engine.portfolio_manager().position("BTCUSD").quantity, 1);
    EXPECT_NEAR(engine.portfolio_manager().cash_balance(), 900.0, 1e-9);
}

TEST(EngineBaseTest, MultipleTicksProcessedInOrder)
{
    DummyStreamer streamer{
        {tick_data{"BTCUSD", 100.0, 1.0, 1, false},
         tick_data{"BTCUSD", 101.0, 1.0, 2, false},
         tick_data{"BTCUSD", 102.0, 1.0, 3, false}}};
    DummyStrategy strat;
    portfolio_manager pf(1000.0);
    DummyExec exec;

    TestEngine engine{std::move(streamer), std::move(strat), std::move(pf), std::move(exec)};
    engine.run();

    // Market prices should be updated to last tick
    EXPECT_NEAR(engine.portfolio_manager().total_equity(), 1006.0, 1e-9); // only 1 fill from first tick
    EXPECT_EQ(engine.portfolio_manager().position("BTCUSD").quantity, 3);
    EXPECT_NEAR(engine.portfolio_manager().unrealized_pnl(), 6.0, 1e-9); // mark-to-market at 102 vs entry 100
}

TEST(EngineBaseTest, NoEventsHandleNoEventStopsLoop)
{
    DummyStreamer streamer{}; // empty
    DummyStrategy strat;
    portfolio_manager pf(1000.0);
    DummyExec exec;

    struct StopEngine : public TestEngine
    {
        using TestEngine::TestEngine;
        bool handle_no_event() { return false; } // stop immediately
    };

    StopEngine engine{std::move(streamer), std::move(strat), std::move(pf), std::move(exec)};
    engine.run();

    EXPECT_EQ(engine.portfolio_manager().cash_balance(), 1000.0); // unchanged
}

TEST(EngineBaseTest, MultipleSignalsOrdersFills)
{
    // Two ticks => two fills
    DummyStreamer streamer{
        {tick_data{"BTCUSD", 100.0, 1.0, 1, false},
         tick_data{"BTCUSD", 100.0, 1.0, 2, false}}};
    DummyStrategy strat;
    portfolio_manager pf(1000.0);
    DummyExec exec;

    TestEngine engine{std::move(streamer), std::move(strat), std::move(pf), std::move(exec)};
    engine.run();

    EXPECT_EQ(engine.portfolio_manager().position("BTCUSD").quantity, 2);
    EXPECT_NEAR(engine.portfolio_manager().cash_balance(), 800.0, 1e-9);
}
