#include <gtest/gtest.h>
#include "portfolio/portfolio_manager.hpp"
#include "events/event.hpp"

using namespace engine::events;
using namespace engine::portfolio;

// compare doubles with tolerance
constexpr double EPS = 1e-9;
#define EXPECT_NEAR_EQ(val, expected) EXPECT_NEAR((val), (expected), EPS)

TEST(PortfolioTest, InitialState)
{
    portfolio_manager pf(100000.0);

    EXPECT_NEAR_EQ(pf.cash_balance(), 100000.0);
    EXPECT_NEAR_EQ(pf.total_equity(), 100000.0);
    EXPECT_NEAR_EQ(pf.realized_pnl(), 0.0);
    EXPECT_NEAR_EQ(pf.unrealized_pnl(), 0.0);
    EXPECT_EQ(pf.trade_log().size(), 0);
}

TEST(PortfolioTest, OpensLongCorrectly)
{
    portfolio_manager pf(1000.0);
    pf.on_fill(fill_event{"BTCUSD", "1", 10, 10, true, 100.0}); // buy 10 @ 100

    EXPECT_NEAR_EQ(pf.cash_balance(), 0.0);
    EXPECT_EQ(pf.position("BTCUSD").quantity, 10);
    EXPECT_NEAR_EQ(pf.position("BTCUSD").avg_price, 100.0);
    EXPECT_NEAR_EQ(pf.realized_pnl(), 0.0);
}

TEST(PortfolioTest, AddsToLongAveragesPrice)
{
    portfolio_manager pf(3000.0);
    pf.on_fill(fill_event{"BTCUSD", "1", 10, 10, true, 100.0});
    pf.on_fill(fill_event{"BTCUSD", "2", 10, 10, true, 120.0}); // avg to 110

    EXPECT_EQ(pf.position("BTCUSD").quantity, 20);
    EXPECT_NEAR_EQ(pf.position("BTCUSD").avg_price, 110.0);
    EXPECT_NEAR_EQ(pf.cash_balance(), 800.0);
    EXPECT_NEAR_EQ(pf.realized_pnl(), 0.0);
}

TEST(PortfolioTest, ReducesLongRealizesPnL)
{
    portfolio_manager pf(5000.0);
    pf.on_fill(fill_event{"BTCUSD", "1", 20, 20, true, 100.0}); // long 20 @ 100
    pf.on_fill(fill_event{"BTCUSD", "2", 5, 5, false, 130.0});  // sell 5 @ 130

    EXPECT_EQ(pf.position("BTCUSD").quantity, 15);
    EXPECT_NEAR_EQ(pf.position("BTCUSD").avg_price, 100.0);
    EXPECT_NEAR_EQ(pf.realized_pnl(), 150.0); // 5*(130-100)
}

TEST(PortfolioTest, ClosesLongResetsPosition)
{
    portfolio_manager pf(2000.0);
    pf.on_fill(fill_event{"BTCUSD", "1", 20, 20, true, 100.0});
    pf.on_fill(fill_event{"BTCUSD", "2", 20, 20, false, 90.0}); // sell all

    EXPECT_EQ(pf.position("BTCUSD").quantity, 0);
    EXPECT_NEAR_EQ(pf.position("BTCUSD").avg_price, 0.0);
    EXPECT_NEAR_EQ(pf.realized_pnl(), -200.0); // 20*(90-100)
}

TEST(PortfolioTest, FlipsLongToShort)
{
    portfolio_manager pf(2000.0);
    pf.on_fill(fill_event{"BTCUSD", "1", 10, 10, true, 100.0});
    pf.on_fill(fill_event{"BTCUSD", "2", 15, 15, false, 110.0}); // sell 15

    EXPECT_EQ(pf.position("BTCUSD").quantity, -5); // now short 5
    EXPECT_NEAR_EQ(pf.position("BTCUSD").avg_price, 110.0);
    EXPECT_NEAR_EQ(pf.realized_pnl(), 100.0); // 10*(110-100)
}

TEST(PortfolioTest, OpensShortCorrectly)
{
    portfolio_manager pf(2000.0);
    pf.on_fill(fill_event{"BTCUSD", "1", 10, 10, false, 200.0}); // short 10 @ 200

    EXPECT_EQ(pf.position("BTCUSD").quantity, -10);
    EXPECT_NEAR_EQ(pf.position("BTCUSD").avg_price, 200.0);
    EXPECT_NEAR_EQ(pf.realized_pnl(), 0.0);
}

TEST(PortfolioTest, CoversShortPartially)
{
    portfolio_manager pf(4000.0);
    pf.on_fill(fill_event{"BTCUSD", "1", 10, 10, false, 200.0}); // short 10
    pf.on_fill(fill_event{"BTCUSD", "2", 5, 5, true, 180.0});    // buy 5

    EXPECT_EQ(pf.position("BTCUSD").quantity, -5);
    EXPECT_NEAR_EQ(pf.realized_pnl(), 100.0); // 5*(200-180)
    EXPECT_NEAR_EQ(pf.position("BTCUSD").avg_price, 200.0);
}

TEST(PortfolioTest, FlipsShortToLong)
{
    portfolio_manager pf(4000.0);
    pf.on_fill(fill_event{"BTCUSD", "1", 10, 10, false, 200.0}); // short 10
    pf.on_fill(fill_event{"BTCUSD", "2", 15, 15, true, 210.0});  // buy 15

    EXPECT_EQ(pf.position("BTCUSD").quantity, 5); // now long 5
    EXPECT_NEAR_EQ(pf.position("BTCUSD").avg_price, 210.0);
    EXPECT_NEAR_EQ(pf.realized_pnl(), -100.0); // 10*(210-200)
}

TEST(PortfolioTest, UnrealizedPnLTracksMarket)
{
    portfolio_manager pf(2000.0);
    pf.on_fill(fill_event{"BTCUSD", "1", 10, 10, true, 100.0});
    pf.on_market("BTCUSD", 110.0); // mark to market

    EXPECT_NEAR_EQ(pf.unrealized_pnl(), 100.0); // 10*(110-100)
    EXPECT_NEAR_EQ(pf.total_equity(), 2100.0);  // cash=1000 + pos=1100
}

TEST(PortfolioTest, TradeLogRecordsFills)
{
    portfolio_manager pf(1000.0);
    pf.on_fill(fill_event{"BTCUSD", "1", 1, 1, true, 100.0});
    pf.on_fill(fill_event{"BTCUSD", "2", 1, 1, false, 120.0});

    ASSERT_EQ(pf.trade_log().size(), 2);
    EXPECT_EQ(pf.trade_log()[0].symbol_, "BTCUSD");
    EXPECT_TRUE(pf.trade_log()[0].is_buy_);
    EXPECT_FALSE(pf.trade_log()[1].is_buy_);
}
