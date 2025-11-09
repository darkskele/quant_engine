#include <gtest/gtest.h>
#include "portfolio/portfolio_manager.hpp"
#include <limits>
#include <cmath>

using namespace engine::portfolio;

// Mock EventBus for testing
class MockEventBus
{
public:
    struct Order
    {
        uint64_t order_id;
        uint32_t symbol_id;
        int32_t quantity;
        double price;
        uint64_t timestamp_ns;
    };

    std::vector<Order> emitted_orders;

    void emit_order(uint64_t order_id, uint32_t symbol_id, int32_t quantity,
                    double price, uint64_t timestamp_ns)
    {
        emitted_orders.push_back({order_id, symbol_id, quantity, price, timestamp_ns});
    }

    void clear()
    {
        emitted_orders.clear();
    }
};

class PortfolioManagerTest : public ::testing::Test
{
protected:
    static constexpr size_t MAX_SYMBOLS = 1024;
    static constexpr double INITIAL_CAPITAL = 1000000.0;
    static constexpr double EPSILON = 1e-9;

    MockEventBus bus;
    std::unique_ptr<PortfolioManager<MockEventBus, MAX_SYMBOLS>> pm;

    void SetUp() override
    {
        pm = std::make_unique<PortfolioManager<MockEventBus, MAX_SYMBOLS>>(bus, INITIAL_CAPITAL);
    }

    void TearDown() override
    {
        pm.reset();
        bus.clear();
    }

    // Helper to compare doubles
    bool nearly_equal(double a, double b, double epsilon = EPSILON)
    {
        return std::abs(a - b) < epsilon;
    }
};

// Constructor Tests
TEST_F(PortfolioManagerTest, ConstructorInitializesCorrectly)
{
    EXPECT_DOUBLE_EQ(pm->get_cash(), INITIAL_CAPITAL);
    EXPECT_EQ(pm->get_order_count(), 0);
    EXPECT_EQ(pm->get_fill_count(), 0);
    EXPECT_EQ(pm->get_reject_count(), 0);
    EXPECT_DOUBLE_EQ(pm->get_total_value(), INITIAL_CAPITAL);
}

// on_signal Tests
TEST_F(PortfolioManagerTest, OnSignalValidInput)
{
    uint32_t symbol_id = 0;
    int32_t quantity = 100;
    double price = 50.0;
    uint64_t timestamp = 1000;

    // Set reasonable risk limits
    RiskLimits risk;
    risk.max_positions_ = 1000;
    risk.max_order_size_ = 500;
    risk.max_notional_ = 100000.0;
    pm->set_risk_limit(symbol_id, risk);

    pm->on_signal(symbol_id, quantity, price, timestamp);

    EXPECT_EQ(pm->get_order_count(), 1);
    EXPECT_EQ(pm->get_reject_count(), 0);
    EXPECT_EQ(bus.emitted_orders.size(), 1);

    const auto &order = bus.emitted_orders[0];
    EXPECT_EQ(order.symbol_id, symbol_id);
    EXPECT_EQ(order.quantity, quantity);
    EXPECT_DOUBLE_EQ(order.price, price);
    EXPECT_EQ(order.timestamp_ns, timestamp);
}

TEST_F(PortfolioManagerTest, OnSignalInvalidSymbolId)
{
    EXPECT_THROW(
        pm->on_signal(MAX_SYMBOLS, 100, 50.0, 1000),
        std::out_of_range);
}

TEST_F(PortfolioManagerTest, OnSignalNegativePrice)
{
    EXPECT_THROW(
        pm->on_signal(0, 100, -50.0, 1000),
        std::invalid_argument);
}

TEST_F(PortfolioManagerTest, OnSignalZeroPrice)
{
    EXPECT_THROW(
        pm->on_signal(0, 100, 0.0, 1000),
        std::invalid_argument);
}

TEST_F(PortfolioManagerTest, OnSignalInfinitePrice)
{
    EXPECT_THROW(
        pm->on_signal(0, 100, std::numeric_limits<double>::infinity(), 1000),
        std::invalid_argument);
}

TEST_F(PortfolioManagerTest, OnSignalNaNPrice)
{
    EXPECT_THROW(
        pm->on_signal(0, 100, std::numeric_limits<double>::quiet_NaN(), 1000),
        std::invalid_argument);
}

TEST_F(PortfolioManagerTest, OnSignalZeroQuantity)
{
    EXPECT_THROW(
        pm->on_signal(0, 0, 50.0, 1000),
        std::invalid_argument);
}

TEST_F(PortfolioManagerTest, OnSignalExceedsPositionLimit)
{
    uint32_t symbol_id = 0;

    RiskLimits risk;
    risk.max_positions_ = 50;
    risk.max_order_size_ = 500;
    risk.max_notional_ = 100000.0;
    pm->set_risk_limit(symbol_id, risk);

    pm->on_signal(symbol_id, 100, 50.0, 1000);

    EXPECT_EQ(pm->get_order_count(), 0);
    EXPECT_EQ(pm->get_reject_count(), 1);
    EXPECT_EQ(bus.emitted_orders.size(), 0);
}

TEST_F(PortfolioManagerTest, OnSignalExceedsOrderSizeLimit)
{
    uint32_t symbol_id = 0;

    RiskLimits risk;
    risk.max_positions_ = 1000;
    risk.max_order_size_ = 50;
    risk.max_notional_ = 100000.0;
    pm->set_risk_limit(symbol_id, risk);

    pm->on_signal(symbol_id, 100, 50.0, 1000);

    EXPECT_EQ(pm->get_order_count(), 0);
    EXPECT_EQ(pm->get_reject_count(), 1);
}

TEST_F(PortfolioManagerTest, OnSignalExceedsNotionalLimit)
{
    uint32_t symbol_id = 0;

    RiskLimits risk;
    risk.max_positions_ = 1000;
    risk.max_order_size_ = 500;
    risk.max_notional_ = 1000.0; // Very low
    pm->set_risk_limit(symbol_id, risk);

    pm->on_signal(symbol_id, 100, 50.0, 1000);

    EXPECT_EQ(pm->get_order_count(), 0);
    EXPECT_EQ(pm->get_reject_count(), 1);
}

TEST_F(PortfolioManagerTest, OnSignalInsufficientCash)
{
    uint32_t symbol_id = 0;

    RiskLimits risk;
    risk.max_positions_ = 100000;
    risk.max_order_size_ = 100000;
    risk.max_notional_ = 10000000.0;
    pm->set_risk_limit(symbol_id, risk);

    // Try to buy more than we have cash for
    pm->on_signal(symbol_id, 100000, 50.0, 1000);

    EXPECT_EQ(pm->get_order_count(), 0);
    EXPECT_EQ(pm->get_reject_count(), 1);
}

TEST_F(PortfolioManagerTest, OnSignalShortDoesNotRequireCash)
{
    uint32_t symbol_id = 0;

    RiskLimits risk;
    risk.max_positions_ = 1000;
    risk.max_order_size_ = 500;
    risk.max_notional_ = 100000.0;
    pm->set_risk_limit(symbol_id, risk);

    // Selling/shorting doesn't need cash
    pm->on_signal(symbol_id, -100, 50.0, 1000);

    EXPECT_EQ(pm->get_order_count(), 1);
    EXPECT_EQ(pm->get_reject_count(), 0);
}

// on_fill Tests
TEST_F(PortfolioManagerTest, OnFillSimpleBuy)
{
    uint32_t symbol_id = 0;
    int32_t quantity = 100;
    double price = 50.0;

    pm->on_fill(symbol_id, quantity, price);

    EXPECT_EQ(pm->get_fill_count(), 1);
    EXPECT_TRUE(pm->has_position(symbol_id));

    const auto &pos = pm->get_position(symbol_id);
    EXPECT_EQ(pos.quantity_, quantity);
    EXPECT_DOUBLE_EQ(pos.average_cost_, price);
    EXPECT_DOUBLE_EQ(pos.realized_pnl_, 0.0);

    // Cash should decrease
    EXPECT_DOUBLE_EQ(pm->get_cash(), INITIAL_CAPITAL - (quantity * price));
}

TEST_F(PortfolioManagerTest, OnFillSimpleSell)
{
    uint32_t symbol_id = 0;
    int32_t quantity = -100;
    double price = 50.0;

    pm->on_fill(symbol_id, quantity, price);

    EXPECT_EQ(pm->get_fill_count(), 1);
    EXPECT_TRUE(pm->has_position(symbol_id));

    const auto &pos = pm->get_position(symbol_id);
    EXPECT_EQ(pos.quantity_, quantity);
    EXPECT_DOUBLE_EQ(pos.average_cost_, price);

    // Cash should increase (short sale)
    EXPECT_DOUBLE_EQ(pm->get_cash(), INITIAL_CAPITAL - (quantity * price));
}

TEST_F(PortfolioManagerTest, OnFillInvalidSymbolId)
{
    EXPECT_THROW(
        pm->on_fill(MAX_SYMBOLS, 100, 50.0),
        std::out_of_range);
}

TEST_F(PortfolioManagerTest, OnFillInvalidPrice)
{
    EXPECT_THROW(pm->on_fill(0, 100, -50.0), std::invalid_argument);
    EXPECT_THROW(pm->on_fill(0, 100, 0.0), std::invalid_argument);
    EXPECT_THROW(pm->on_fill(0, 100, std::numeric_limits<double>::infinity()), std::invalid_argument);
    EXPECT_THROW(pm->on_fill(0, 100, std::numeric_limits<double>::quiet_NaN()), std::invalid_argument);
}

TEST_F(PortfolioManagerTest, OnFillZeroQuantity)
{
    EXPECT_THROW(
        pm->on_fill(0, 0, 50.0),
        std::invalid_argument);
}

TEST_F(PortfolioManagerTest, OnFillAddToLongPosition)
{
    uint32_t symbol_id = 0;

    // First fill
    pm->on_fill(symbol_id, 100, 50.0);

    // Second fill at different price
    pm->on_fill(symbol_id, 100, 60.0);

    const auto &pos = pm->get_position(symbol_id);
    EXPECT_EQ(pos.quantity_, 200);
    EXPECT_DOUBLE_EQ(pos.average_cost_, 55.0); // VWAP: (100*50 + 100*60) / 200
    EXPECT_DOUBLE_EQ(pos.realized_pnl_, 0.0);
}

TEST_F(PortfolioManagerTest, OnFillAddToShortPosition)
{
    uint32_t symbol_id = 0;

    // First short
    pm->on_fill(symbol_id, -100, 50.0);

    // Add to short
    pm->on_fill(symbol_id, -100, 60.0);

    const auto &pos = pm->get_position(symbol_id);
    EXPECT_EQ(pos.quantity_, -200);
    EXPECT_DOUBLE_EQ(pos.average_cost_, 55.0); // VWAP
    EXPECT_DOUBLE_EQ(pos.realized_pnl_, 0.0);
}

TEST_F(PortfolioManagerTest, OnFillPartialClose)
{
    uint32_t symbol_id = 0;

    // Buy 100 @ 50
    pm->on_fill(symbol_id, 100, 50.0);

    // Sell 60 @ 55 (partial close)
    pm->on_fill(symbol_id, -60, 55.0);

    const auto &pos = pm->get_position(symbol_id);
    EXPECT_EQ(pos.quantity_, 40);
    EXPECT_DOUBLE_EQ(pos.average_cost_, 50.0);  // Cost basis unchanged
    EXPECT_DOUBLE_EQ(pos.realized_pnl_, 300.0); // 60 * (55 - 50)
}

TEST_F(PortfolioManagerTest, OnFillFullClose)
{
    uint32_t symbol_id = 0;

    // Buy 100 @ 50
    pm->on_fill(symbol_id, 100, 50.0);

    // Sell 100 @ 55 (full close)
    pm->on_fill(symbol_id, -100, 55.0);

    const auto &pos = pm->get_position(symbol_id);
    EXPECT_EQ(pos.quantity_, 0);
    EXPECT_DOUBLE_EQ(pos.realized_pnl_, 500.0); // 100 * (55 - 50)
    EXPECT_FALSE(pm->has_position(symbol_id));  // Should be inactive
}

TEST_F(PortfolioManagerTest, OnFillPositionReversal)
{
    uint32_t symbol_id = 0;

    // Buy 100 @ 50
    pm->on_fill(symbol_id, 100, 50.0);

    // Sell 150 @ 55 (close and reverse to short)
    pm->on_fill(symbol_id, -150, 55.0);

    const auto &pos = pm->get_position(symbol_id);
    EXPECT_EQ(pos.quantity_, -50);
    EXPECT_DOUBLE_EQ(pos.average_cost_, 55.0);  // New cost basis at reversal price
    EXPECT_DOUBLE_EQ(pos.realized_pnl_, 500.0); // Closed 100 @ profit of 5 each
}

TEST_F(PortfolioManagerTest, OnFillShortToLongReversal)
{
    uint32_t symbol_id = 0;

    // Short 100 @ 50
    pm->on_fill(symbol_id, -100, 50.0);

    // Buy 150 @ 45 (cover and go long)
    pm->on_fill(symbol_id, 150, 45.0);

    const auto &pos = pm->get_position(symbol_id);
    EXPECT_EQ(pos.quantity_, 50);
    EXPECT_DOUBLE_EQ(pos.average_cost_, 45.0);  // New cost basis
    EXPECT_DOUBLE_EQ(pos.realized_pnl_, 500.0); // Covered 100 @ profit of 5 each (50-45)
}

TEST_F(PortfolioManagerTest, OnFillShortProfitCalculation)
{
    uint32_t symbol_id = 0;

    // Short 100 @ 50
    pm->on_fill(symbol_id, -100, 50.0);

    // Cover 100 @ 45 (profit)
    pm->on_fill(symbol_id, 100, 45.0);

    const auto &pos = pm->get_position(symbol_id);
    EXPECT_EQ(pos.quantity_, 0);
    EXPECT_DOUBLE_EQ(pos.realized_pnl_, 500.0); // Short profit: 100 * (50 - 45)
}

TEST_F(PortfolioManagerTest, OnFillShortLossCalculation)
{
    uint32_t symbol_id = 0;

    // Short 100 @ 50
    pm->on_fill(symbol_id, -100, 50.0);

    // Cover 100 @ 55 (loss)
    pm->on_fill(symbol_id, 100, 55.0);

    const auto &pos = pm->get_position(symbol_id);
    EXPECT_EQ(pos.quantity_, 0);
    EXPECT_DOUBLE_EQ(pos.realized_pnl_, -500.0); // Short loss: 100 * (50 - 55)
}

TEST_F(PortfolioManagerTest, OnFillPendingQuantityUpdate)
{
    uint32_t symbol_id = 0;

    RiskLimits risk;
    risk.max_positions_ = 1000;
    risk.max_order_size_ = 500;
    risk.max_notional_ = 100000.0;
    pm->set_risk_limit(symbol_id, risk);

    // Send signal (increases pending)
    pm->on_signal(symbol_id, 100, 50.0, 1000);

    const auto &pos_before = pm->get_position(symbol_id);
    EXPECT_EQ(pos_before.pending_quantity_, 100);

    // Fill (decreases pending)
    pm->on_fill(symbol_id, 100, 50.0);

    const auto &pos_after = pm->get_position(symbol_id);
    EXPECT_EQ(pos_after.pending_quantity_, 0);
}

// on_market_data Tests
TEST_F(PortfolioManagerTest, OnMarketDataValidUpdate)
{
    uint32_t symbol_id = 0;
    double price = 100.0;

    pm->on_market_data(symbol_id, price);

    const auto &pos = pm->get_position(symbol_id);
    EXPECT_DOUBLE_EQ(pos.last_price_, price);
}

TEST_F(PortfolioManagerTest, OnMarketDataInvalidSymbolId)
{
    EXPECT_THROW(
        pm->on_market_data(MAX_SYMBOLS, 100.0),
        std::out_of_range);
}

TEST_F(PortfolioManagerTest, OnMarketDataInvalidPrice)
{
    EXPECT_THROW(pm->on_market_data(0, -100.0), std::invalid_argument);
    EXPECT_THROW(pm->on_market_data(0, 0.0), std::invalid_argument);
    EXPECT_THROW(pm->on_market_data(0, std::numeric_limits<double>::infinity()), std::invalid_argument);
    EXPECT_THROW(pm->on_market_data(0, std::numeric_limits<double>::quiet_NaN()), std::invalid_argument);
}

TEST_F(PortfolioManagerTest, OnMarketDataMultipleUpdates)
{
    uint32_t symbol_id = 0;

    pm->on_market_data(symbol_id, 100.0);
    pm->on_market_data(symbol_id, 105.0);
    pm->on_market_data(symbol_id, 95.0);

    const auto &pos = pm->get_position(symbol_id);
    EXPECT_DOUBLE_EQ(pos.last_price_, 95.0); // Last update wins
}

// compute_metrics Tests
TEST_F(PortfolioManagerTest, ComputeMetricsEmptyPortfolio)
{
    auto metrics = pm->compute_metrics();

    EXPECT_DOUBLE_EQ(metrics.total_pnl_, 0.0);
    EXPECT_DOUBLE_EQ(metrics.realized_pnl_, 0.0);
    EXPECT_DOUBLE_EQ(metrics.unrealized_pnl_, 0.0);
    EXPECT_DOUBLE_EQ(metrics.gross_exposure_, 0.0);
    EXPECT_DOUBLE_EQ(metrics.net_exposure_, 0.0);
    EXPECT_EQ(metrics.num_positions_, 0);
    EXPECT_EQ(metrics.total_trades_, 0);
}

TEST_F(PortfolioManagerTest, ComputeMetricsSingleLongPosition)
{
    uint32_t symbol_id = 0;

    // Buy 100 @ 50
    pm->on_fill(symbol_id, 100, 50.0);

    // Update market price to 55
    pm->on_market_data(symbol_id, 55.0);

    auto metrics = pm->compute_metrics();

    EXPECT_EQ(metrics.num_positions_, 1);
    EXPECT_EQ(metrics.total_trades_, 1);
    EXPECT_DOUBLE_EQ(metrics.realized_pnl_, 0.0);
    EXPECT_DOUBLE_EQ(metrics.unrealized_pnl_, 500.0); // 100 * (55 - 50)
    EXPECT_DOUBLE_EQ(metrics.total_pnl_, 500.0);
    EXPECT_DOUBLE_EQ(metrics.gross_exposure_, 5500.0); // 100 * 55
    EXPECT_DOUBLE_EQ(metrics.net_exposure_, 5500.0);
}

TEST_F(PortfolioManagerTest, ComputeMetricsSingleShortPosition)
{
    uint32_t symbol_id = 0;

    // Short 100 @ 50
    pm->on_fill(symbol_id, -100, 50.0);

    // Update market price to 45
    pm->on_market_data(symbol_id, 45.0);

    auto metrics = pm->compute_metrics();

    EXPECT_EQ(metrics.num_positions_, 1);
    EXPECT_DOUBLE_EQ(metrics.unrealized_pnl_, 500.0);  // -100 * (45 - 50) = 500
    EXPECT_DOUBLE_EQ(metrics.gross_exposure_, 4500.0); // abs(-100 * 45)
    EXPECT_DOUBLE_EQ(metrics.net_exposure_, -4500.0);  // -100 * 45
}

TEST_F(PortfolioManagerTest, ComputeMetricsMultiplePositions)
{
    // Symbol 0: Long 100 @ 50, now @ 55
    pm->on_fill(0, 100, 50.0);
    pm->on_market_data(0, 55.0);

    // Symbol 1: Short 50 @ 100, now @ 95
    pm->on_fill(1, -50, 100.0);
    pm->on_market_data(1, 95.0);

    auto metrics = pm->compute_metrics();

    EXPECT_EQ(metrics.num_positions_, 2);
    EXPECT_EQ(metrics.total_trades_, 2);
    EXPECT_DOUBLE_EQ(metrics.unrealized_pnl_, 750.0);   // 500 + 250
    EXPECT_DOUBLE_EQ(metrics.gross_exposure_, 10250.0); // 5500 + 4750
    EXPECT_DOUBLE_EQ(metrics.net_exposure_, 750.0);     // 5500 - 4750
}

TEST_F(PortfolioManagerTest, ComputeMetricsWithRealizedPnL)
{
    uint32_t symbol_id = 0;

    // Buy 100 @ 50
    pm->on_fill(symbol_id, 100, 50.0);

    // Sell 100 @ 55 (realize profit)
    pm->on_fill(symbol_id, -100, 55.0);

    auto metrics = pm->compute_metrics();

    EXPECT_EQ(metrics.num_positions_, 0); // Position closed
    EXPECT_DOUBLE_EQ(metrics.realized_pnl_, 500.0);
    EXPECT_DOUBLE_EQ(metrics.unrealized_pnl_, 0.0);
    EXPECT_DOUBLE_EQ(metrics.total_pnl_, 500.0);
}

TEST_F(PortfolioManagerTest, ComputeMetricsMixedRealizedAndUnrealized)
{
    uint32_t symbol_id = 0;

    // Buy 100 @ 50
    pm->on_fill(symbol_id, 100, 50.0);

    // Sell 60 @ 55 (partial close, realize 300)
    pm->on_fill(symbol_id, -60, 55.0);

    // Update price to 60 for remaining 40 shares
    pm->on_market_data(symbol_id, 60.0);

    auto metrics = pm->compute_metrics();

    EXPECT_EQ(metrics.num_positions_, 1);
    EXPECT_DOUBLE_EQ(metrics.realized_pnl_, 300.0);   // 60 * 5
    EXPECT_DOUBLE_EQ(metrics.unrealized_pnl_, 400.0); // 40 * (60 - 50)
    EXPECT_DOUBLE_EQ(metrics.total_pnl_, 700.0);
}

// Getter Tests
TEST_F(PortfolioManagerTest, GetPositionValid)
{
    uint32_t symbol_id = 0;
    pm->on_fill(symbol_id, 100, 50.0);

    const auto &pos = pm->get_position(symbol_id);
    EXPECT_EQ(pos.quantity_, 100);
    EXPECT_DOUBLE_EQ(pos.average_cost_, 50.0);
}

TEST_F(PortfolioManagerTest, GetPositionInvalid)
{
    EXPECT_THROW(
        pm->get_position(MAX_SYMBOLS),
        std::out_of_range);
}

TEST_F(PortfolioManagerTest, GetUnrealizedPnLNoPosition)
{
    uint32_t symbol_id = 0;
    EXPECT_DOUBLE_EQ(pm->get_unrealized_pnl(symbol_id), 0.0);
}

TEST_F(PortfolioManagerTest, GetUnrealizedPnLWithPosition)
{
    uint32_t symbol_id = 0;

    pm->on_fill(symbol_id, 100, 50.0);
    pm->on_market_data(symbol_id, 55.0);

    EXPECT_DOUBLE_EQ(pm->get_unrealized_pnl(symbol_id), 500.0);
}

TEST_F(PortfolioManagerTest, GetUnrealizedPnLInvalid)
{
    EXPECT_THROW(
        pm->get_unrealized_pnl(MAX_SYMBOLS),
        std::out_of_range);
}

TEST_F(PortfolioManagerTest, GetCash)
{
    EXPECT_DOUBLE_EQ(pm->get_cash(), INITIAL_CAPITAL);

    pm->on_fill(0, 100, 50.0);
    EXPECT_DOUBLE_EQ(pm->get_cash(), INITIAL_CAPITAL - 5000.0);
}

TEST_F(PortfolioManagerTest, GetTotalValueNoPositions)
{
    EXPECT_DOUBLE_EQ(pm->get_total_value(), INITIAL_CAPITAL);
}

TEST_F(PortfolioManagerTest, GetTotalValueWithPositions)
{
    // Buy 100 @ 50
    pm->on_fill(0, 100, 50.0);
    pm->on_market_data(0, 55.0);

    double expected = INITIAL_CAPITAL - 5000.0 + 5500.0; // cash + position value
    EXPECT_DOUBLE_EQ(pm->get_total_value(), expected);
}

TEST_F(PortfolioManagerTest, GetOrderCount)
{
    RiskLimits risk;
    risk.max_positions_ = 1000;
    risk.max_order_size_ = 500;
    risk.max_notional_ = 100000.0;
    pm->set_risk_limit(0, risk);

    EXPECT_EQ(pm->get_order_count(), 0);

    pm->on_signal(0, 100, 50.0, 1000);
    EXPECT_EQ(pm->get_order_count(), 1);

    pm->on_signal(0, 50, 50.0, 2000);
    EXPECT_EQ(pm->get_order_count(), 2);
}

TEST_F(PortfolioManagerTest, GetFillCount)
{
    EXPECT_EQ(pm->get_fill_count(), 0);

    pm->on_fill(0, 100, 50.0);
    EXPECT_EQ(pm->get_fill_count(), 1);

    pm->on_fill(1, 50, 100.0);
    EXPECT_EQ(pm->get_fill_count(), 2);
}

TEST_F(PortfolioManagerTest, GetRejectCount)
{
    RiskLimits risk;
    risk.max_positions_ = 50;
    risk.max_order_size_ = 500;
    risk.max_notional_ = 100000.0;
    pm->set_risk_limit(0, risk);

    EXPECT_EQ(pm->get_reject_count(), 0);

    // This should be rejected (exceeds position limit)
    pm->on_signal(0, 100, 50.0, 1000);
    EXPECT_EQ(pm->get_reject_count(), 1);
}

TEST_F(PortfolioManagerTest, SetAndGetRiskLimit)
{
    uint32_t symbol_id = 0;

    RiskLimits risk;
    risk.max_positions_ = 500;
    risk.max_order_size_ = 200;
    risk.max_notional_ = 50000.0;

    pm->set_risk_limit(symbol_id, risk);

    RiskLimits retrieved = pm->get_risk_limit(symbol_id);
    EXPECT_EQ(retrieved.max_positions_, 500);
    EXPECT_EQ(retrieved.max_order_size_, 200);
    EXPECT_DOUBLE_EQ(retrieved.max_notional_, 50000.0);
}

TEST_F(PortfolioManagerTest, SetRiskLimitInvalid)
{
    RiskLimits risk;
    EXPECT_THROW(
        pm->set_risk_limit(MAX_SYMBOLS, risk),
        std::out_of_range);
}

TEST_F(PortfolioManagerTest, GetRiskLimitInvalid)
{
    EXPECT_THROW(
        pm->get_risk_limit(MAX_SYMBOLS),
        std::out_of_range);
}

TEST_F(PortfolioManagerTest, HasPositionTrue)
{
    pm->on_fill(0, 100, 50.0);
    EXPECT_TRUE(pm->has_position(0));
}

TEST_F(PortfolioManagerTest, HasPositionFalse)
{
    EXPECT_FALSE(pm->has_position(0));
}

TEST_F(PortfolioManagerTest, HasPositionAfterClose)
{
    pm->on_fill(0, 100, 50.0);
    EXPECT_TRUE(pm->has_position(0));

    pm->on_fill(0, -100, 55.0);
    EXPECT_FALSE(pm->has_position(0));
}

TEST_F(PortfolioManagerTest, HasPositionInvalid)
{
    EXPECT_FALSE(pm->has_position(MAX_SYMBOLS)); // Returns false for invalid
}

// Integration / Complex Scenarios
TEST_F(PortfolioManagerTest, ComplexTradingScenario)
{
    RiskLimits risk;
    risk.max_positions_ = 1000;
    risk.max_order_size_ = 500;
    risk.max_notional_ = 100000.0;
    pm->set_risk_limit(0, risk);

    // Day 1: Buy 100 @ 50
    pm->on_signal(0, 100, 50.0, 1000);
    pm->on_fill(0, 100, 50.0);
    pm->on_market_data(0, 52.0);

    auto m1 = pm->compute_metrics();
    EXPECT_DOUBLE_EQ(m1.unrealized_pnl_, 200.0);

    // Day 2: Add 100 @ 55
    pm->on_signal(0, 100, 55.0, 2000);
    pm->on_fill(0, 100, 55.0);
    pm->on_market_data(0, 58.0);

    auto m2 = pm->compute_metrics();
    EXPECT_EQ(m2.num_positions_, 1);
    EXPECT_DOUBLE_EQ(m2.unrealized_pnl_, 1100.0); // 200*(58-52.5)

    // Day 3: Sell 150 @ 60
    pm->on_signal(0, -150, 60.0, 3000);
    pm->on_fill(0, -150, 60.0);

    auto m3 = pm->compute_metrics();
    EXPECT_EQ(m3.num_positions_, 1);                          // 50 shares remaining
    EXPECT_TRUE(nearly_equal(m3.realized_pnl_, 1125.0, 0.1)); // 150 * (60 - 52.5)
}

TEST_F(PortfolioManagerTest, MultipleSymbolsIndependent)
{
    RiskLimits risk;
    risk.max_positions_ = 1000;
    risk.max_order_size_ = 500;
    risk.max_notional_ = 100000.0;

    // Set limits for multiple symbols
    pm->set_risk_limit(0, risk);
    pm->set_risk_limit(1, risk);
    pm->set_risk_limit(2, risk);

    // Trade different symbols
    pm->on_fill(0, 100, 50.0);
    pm->on_fill(1, -50, 100.0);
    pm->on_fill(2, 75, 25.0);

    EXPECT_TRUE(pm->has_position(0));
    EXPECT_TRUE(pm->has_position(1));
    EXPECT_TRUE(pm->has_position(2));

    auto metrics = pm->compute_metrics();
    EXPECT_EQ(metrics.num_positions_, 3);
}

TEST_F(PortfolioManagerTest, OrderIdIncremental)
{
    RiskLimits risk;
    risk.max_positions_ = 1000;
    risk.max_order_size_ = 500;
    risk.max_notional_ = 100000.0;
    pm->set_risk_limit(0, risk);

    pm->on_signal(0, 100, 50.0, 1000);
    pm->on_signal(0, 50, 50.0, 2000);
    pm->on_signal(0, 25, 50.0, 3000);

    EXPECT_EQ(bus.emitted_orders.size(), 3);
    EXPECT_EQ(bus.emitted_orders[0].order_id, 1);
    EXPECT_EQ(bus.emitted_orders[1].order_id, 2);
    EXPECT_EQ(bus.emitted_orders[2].order_id, 3);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}