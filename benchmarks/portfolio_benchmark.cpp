#include <benchmark/benchmark.h>

#include "portfolio/portfolio_manager.hpp"

using namespace engine::portfolio;

class MockEventBus
{
public:
    // Minimal implementation
    void emit_order(uint64_t, uint32_t, int32_t,
                    double, uint64_t)
    {
    }
};

// Hot path, on_signal
static void BM_OnSignal(benchmark::State &state)
{
    MockEventBus bus;
    PortfolioManager<MockEventBus, 1024> pm(bus, 1000000.0);

    RiskLimits risk;
    risk.max_positions_ = 1000;
    risk.max_order_size_ = 500;
    risk.max_notional_ = 100000.0;
    pm.set_risk_limit(0, risk);

    // Benchmark loop
    for (auto _ : state)
    {
        pm.on_signal(0, 100, 50.0, 1000);
        benchmark::DoNotOptimize(pm); // Prevent optimization
    }

    // Report latency in nanoseconds
    state.SetLabel("Hot path signal processing");
}
BENCHMARK(BM_OnSignal);

// Hot path,  on_fill
static void BM_OnFill(benchmark::State &state)
{
    MockEventBus bus;
    PortfolioManager<MockEventBus, 1024> pm(bus, 1000000.0);

    for (auto _ : state)
    {
        pm.on_fill(0, 100, 50.0);
        benchmark::DoNotOptimize(pm);
    }

    state.SetLabel("Hot path fill processing");
}
BENCHMARK(BM_OnFill);

// Hot path, on_market_data
static void BM_OnMarketData(benchmark::State &state)
{
    MockEventBus bus;
    PortfolioManager<MockEventBus, 1024> pm(bus, 1000000.0);

    for (auto _ : state)
    {
        pm.on_market_data(0, 52.5);
        benchmark::DoNotOptimize(pm);
    }

    state.SetLabel("Hot path market data update");
}
BENCHMARK(BM_OnMarketData);

// Cold path, compute_metrics with varying active positions
static void BM_ComputeMetrics_ActivePositions(benchmark::State &state)
{
    MockEventBus bus;
    PortfolioManager<MockEventBus, 1024> pm(bus, 1000000.0);

    // Create N active positions
    uint32_t num_positions = static_cast<uint32_t>(state.range(0));
    for (uint32_t i = 0; i < num_positions; ++i)
    {
        pm.on_fill(i, 100, 50.0);
        pm.on_market_data(i, 52.0);
    }

    for (auto _ : state)
    {
        auto metrics = pm.compute_metrics();
        benchmark::DoNotOptimize(metrics);
    }

    state.SetLabel("Compute metrics with " + std::to_string(num_positions) + " positions");
}
// Test with different numbers of active positions
BENCHMARK(BM_ComputeMetrics_ActivePositions)->Arg(10)->Arg(50)->Arg(100)->Arg(500);

// Realistic trading scenario - mixed operations
static void BM_RealisticTradingLoop(benchmark::State &state)
{
    MockEventBus bus;
    PortfolioManager<MockEventBus, 1024> pm(bus, 1000000.0);

    RiskLimits risk;
    risk.max_positions_ = 1000;
    risk.max_order_size_ = 500;
    risk.max_notional_ = 100000.0;

    for (uint32_t i = 0; i < 10; ++i)
    {
        pm.set_risk_limit(i, risk);
    }

    uint64_t timestamp = 0;
    for (auto _ : state)
    {
        // Typical loop, market data update, signal, fill
        pm.on_market_data(0, 50.0 + static_cast<double>(timestamp % 100) * 0.01);
        pm.on_signal(0, 100, 50.0, timestamp);
        pm.on_fill(0, 100, 50.0);

        timestamp++;
        benchmark::DoNotOptimize(pm);
    }

    state.SetLabel("Realistic trading loop");
}
BENCHMARK(BM_RealisticTradingLoop);

// Cache effects, test locality with scattered vs contiguous symbols
static void BM_ScatteredSymbols(benchmark::State &state)
{
    MockEventBus bus;
    PortfolioManager<MockEventBus, 1024> pm(bus, 1000000.0);

    // Access symbols with large gaps (poor cache locality)
    for (auto _ : state)
    {
        pm.on_market_data(0, 50.0);
        pm.on_market_data(100, 50.0);
        pm.on_market_data(200, 50.0);
        pm.on_market_data(300, 50.0);
        pm.on_market_data(400, 50.0);
        benchmark::DoNotOptimize(pm);
    }
}
BENCHMARK(BM_ScatteredSymbols);

static void BM_ContiguousSymbols(benchmark::State &state)
{
    MockEventBus bus;
    PortfolioManager<MockEventBus, 1024> pm(bus, 1000000.0);

    // Access adjacent symbols (good cache locality)
    for (auto _ : state)
    {
        pm.on_market_data(0, 50.0);
        pm.on_market_data(1, 50.0);
        pm.on_market_data(2, 50.0);
        pm.on_market_data(3, 50.0);
        pm.on_market_data(4, 50.0);
        benchmark::DoNotOptimize(pm);
    }
}
BENCHMARK(BM_ContiguousSymbols);

BENCHMARK_MAIN();