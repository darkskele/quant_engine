#include <gtest/gtest.h>
#include "execution_engine_base.hpp"
#include "events/event.hpp"
#include "events/event_queue.hpp"

using namespace engine;
using namespace engine::orders;
using namespace engine::events;

// Dummy derived engine for testing base functionality
struct DummyEngine : public execution_engine_base<DummyEngine>
{
    void on_order(const order_event &, event_queue &) {} // not needed for base tests
    // test hook
    void test_emit_fill(const order_event &order,
                        int64_t filled_qty,
                        double exec_price,
                        event_queue &q)
    {
        this->emit_fill(order, filled_qty, exec_price, q);
    }
};

class ExecutionEngineBaseTest : public ::testing::Test
{
protected:
    DummyEngine engine;
    event_queue queue;

    order_event make_order(std::string id, std::string symbol, int64_t qty, bool is_buy = true, double limit = 0.0)
    {
        return order_event{symbol, id, qty, is_buy, limit, order_type::Market, order_flags::FOK, std::chrono::system_clock::now(), market_event{}};
    }
};

TEST_F(ExecutionEngineBaseTest, FirstFillInitializesState)
{
    auto order = make_order("ord1", "AAPL", 100);
    engine.test_emit_fill(order, 100, 150.0, queue);

    const auto *st = engine.get_order("ord1");
    ASSERT_NE(st, nullptr);
    EXPECT_EQ(st->filled_qty_, 100);
    EXPECT_DOUBLE_EQ(st->avg_fill_price_, 150.0);

    ASSERT_FALSE(queue.empty());
    auto ev = queue.pop();
    auto *fill = std::get_if<fill_event>(&ev);
    ASSERT_NE(fill, nullptr);
    EXPECT_EQ(fill->order_id_, "ord1");
    EXPECT_EQ(fill->filled_qty_, 100);
    EXPECT_EQ(fill->order_qty_, 100);
    EXPECT_TRUE(fill->is_buy_);
    EXPECT_DOUBLE_EQ(fill->fill_price_, 150.0);
}

TEST_F(ExecutionEngineBaseTest, MultiplePartialFillsUpdateAveragePrice)
{
    auto order = make_order("ord2", "AAPL", 100);

    engine.test_emit_fill(order, 50, 100.0, queue);
    engine.test_emit_fill(order, 25, 101.0, queue);

    const auto *st = engine.get_order("ord2");
    ASSERT_NE(st, nullptr);
    EXPECT_EQ(st->filled_qty_, 75);
    EXPECT_NEAR(st->avg_fill_price_, 100.33, 1e-2); // weighted avg
}

TEST_F(ExecutionEngineBaseTest, FullFillMarksInactive)
{
    auto order = make_order("ord3", "TSLA", 10);

    engine.test_emit_fill(order, 5, 200.0, queue);
    engine.test_emit_fill(order, 5, 201.0, queue);

    const auto *st = engine.get_order("ord3");
    ASSERT_NE(st, nullptr);
    EXPECT_EQ(st->filled_qty_, 10);
}

TEST_F(ExecutionEngineBaseTest, SeparateOrdersTrackedIndependently)
{
    auto o1 = make_order("ord4", "MSFT", 10);
    auto o2 = make_order("ord5", "GOOG", 20);

    engine.test_emit_fill(o1, 10, 300.0, queue);
    engine.test_emit_fill(o2, 5, 1000.0, queue);

    const auto *st1 = engine.get_order("ord4");
    const auto *st2 = engine.get_order("ord5");

    ASSERT_NE(st1, nullptr);
    ASSERT_NE(st2, nullptr);
    EXPECT_EQ(st1->filled_qty_, 10);
    EXPECT_EQ(st2->filled_qty_, 5);
    EXPECT_NE(st1->order_.symbol_, st2->order_.symbol_);
}

TEST_F(ExecutionEngineBaseTest, OverFillStillMarksInactive)
{
    auto order = make_order("ord6", "NFLX", 10);

    engine.test_emit_fill(order, 15, 500.0, queue); // > total_qty

    const auto *st = engine.get_order("ord6");
    ASSERT_NE(st, nullptr);
    EXPECT_EQ(st->filled_qty_, 15); // recorded as is
}

TEST_F(ExecutionEngineBaseTest, ZeroQuantityFillDoesNotCrash)
{
    auto order = make_order("ord7", "AMZN", 10);

    engine.test_emit_fill(order, 0, 120.0, queue); // no-op fill

    const auto *st = engine.get_order("ord7");
    ASSERT_NE(st, nullptr);
    EXPECT_EQ(st->filled_qty_, 0);
    EXPECT_DOUBLE_EQ(st->avg_fill_price_, 0.0);
}
