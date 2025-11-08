#include <gtest/gtest.h>
#include "events/event_queue.hpp"
#include "events/event.hpp"

using namespace engine::events;

static order_event order_ev{"BTCUSD",
                     "order",
                     2,
                     false,
                     200.0,
                     order_type::Market,
                     order_flags::None,
                     std::chrono::system_clock::now(),
                     market_event{"BTCUSD", 200.0, 2, 1, false}};

TEST(EventQueueTest, InitiallyEmpty)
{
    event_queue q;
    EXPECT_TRUE(q.empty());
}

TEST(EventQueueTest, PushThenPopSingleEvent)
{
    event_queue q;
    fill_event f{"BTCUSD", "1", 1, 1, true, 100.0, order_ev};

    q.push(f);
    EXPECT_FALSE(q.empty());

    auto ev = q.pop();
    EXPECT_TRUE(std::holds_alternative<fill_event>(ev));
    auto &fe = std::get<fill_event>(ev);

    EXPECT_EQ(fe.symbol_, "BTCUSD");
    EXPECT_EQ(fe.filled_qty_, 1);
    EXPECT_TRUE(fe.is_buy_);
    EXPECT_DOUBLE_EQ(fe.fill_price_, 100.0);

    EXPECT_TRUE(q.empty());
}

TEST(EventQueueTest, MultiplePushMaintainsFIFO)
{
    event_queue q;

    order_event o1{"BTCUSD", "1", 5, true, 101.0, order_type::Limit, order_flags::FOK};
    order_event o2{"BTCUSD", "2", 10, false, 99.5, order_type::Limit, order_flags::FOK};

    q.push(o1);
    q.push(o2);

    // First pop must return o1
    auto ev1 = q.pop();
    ASSERT_TRUE(std::holds_alternative<order_event>(ev1));
    auto &oe1 = std::get<order_event>(ev1);
    EXPECT_EQ(oe1.quantity_, 5);
    EXPECT_TRUE(oe1.is_buy_);

    // Second pop must return o2
    auto ev2 = q.pop();
    ASSERT_TRUE(std::holds_alternative<order_event>(ev2));
    auto &oe2 = std::get<order_event>(ev2);
    EXPECT_EQ(oe2.quantity_, 10);
    EXPECT_FALSE(oe2.is_buy_);

    EXPECT_TRUE(q.empty());
}

TEST(EventQueueTest, PushDifferentEventTypes)
{
    event_queue q;

    signal_event s;
    market_event m{"BTCUSD", 100.5, 10.0, 123456789, false};
    fill_event f{"BTCUSD", "2", 2, 2, false, 101.2, order_ev};

    q.push(s);
    q.push(m);
    q.push(f);

    // Pop in order
    auto ev1 = q.pop();
    EXPECT_TRUE(std::holds_alternative<signal_event>(ev1));

    auto ev2 = q.pop();
    EXPECT_TRUE(std::holds_alternative<market_event>(ev2));

    auto ev3 = q.pop();
    EXPECT_TRUE(std::holds_alternative<fill_event>(ev3));
}

TEST(EventQueueTest, MovesEventsCorrectly)
{
    event_queue q;
    fill_event f{"BTCUSD", "1", 3, 3, true, 102.5, order_ev};

    q.push(std::move(f));
    auto ev = q.pop();

    ASSERT_TRUE(std::holds_alternative<fill_event>(ev));
    auto &fe = std::get<fill_event>(ev);
    EXPECT_EQ(fe.filled_qty_, 3);
    EXPECT_DOUBLE_EQ(fe.fill_price_, 102.5);
}

TEST(EventQueueTest, PopThrowsOnEmptyQueue)
{
    event_queue q;
    EXPECT_TRUE(q.empty());
    EXPECT_THROW(q.pop(), std::runtime_error);
}
