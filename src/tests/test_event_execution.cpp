/**
 * test_backtester.cpp
 *
 * A lightweight, dependency-free test suite for the backtester.
 * Compile example (adjust paths as needed):
 *
 *   g++ -std=c++17 -o run_tests test_backtester.cpp execution.cpp data.cpp -I.
 *
 * Because portfolio.h is not yet complete, a minimal stub is defined at the
 * top of this file. Remove it once the real header is ready.
 */

#include <cassert>
#include <cmath>
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// Replace this include with your real portfolio.h once it is complete.
// ─────────────────────────────────────────────────────────────────────────────
#include "portfolio.h"

#include "data.h"
#include "execution.h"

// ─────────────────────────────────────────────────────────────────────────────
// Tiny test harness
// ─────────────────────────────────────────────────────────────────────────────
static int tests_run    = 0;
static int tests_passed = 0;

#define RUN_TEST(name)                                          \
    do {                                                        \
        ++tests_run;                                            \
        std::cout << "  [ ] " #name " ... " << std::flush;     \
        try {                                                   \
            name();                                             \
            ++tests_passed;                                     \
            std::cout << "PASS\n";                              \
        } catch (const std::exception& e) {                    \
            std::cout << "FAIL  (" << e.what() << ")\n";       \
        } catch (...) {                                         \
            std::cout << "FAIL  (unknown exception)\n";         \
        }                                                       \
    } while(0)

#define ASSERT(expr)                                                        \
    do {                                                                    \
        if (!(expr))                                                        \
            throw std::runtime_error("Assertion failed: " #expr            \
                " at line " + std::to_string(__LINE__));                    \
    } while(0)

#define ASSERT_NEAR(a, b, eps)                                              \
    do {                                                                    \
        if (std::fabs((a)-(b)) > (eps))                                     \
            throw std::runtime_error(                                       \
                std::string("ASSERT_NEAR failed: |")                        \
                + std::to_string(a) + " - " + std::to_string(b)            \
                + "| > " + std::to_string(eps)                              \
                + " at line " + std::to_string(__LINE__));                  \
    } while(0)


// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

/** Write a temporary CSV in the format expected by DataHandler::read_csv(). */
static void write_temp_csv(const std::string& ticker,
                                  const std::vector<Bar>& bars)
{
    // Column layout expected by data.cpp:
    //   [0] index  [1] open  [2] high  [3] low  [4] close  [5] volume
    //   [6] ignored_col  [7] timestamp
    std::string path = BASE_DIR + ticker;
    std::ofstream f(path);
    if (!f.is_open()) throw std::runtime_error("write_temp_csv: cannot open " + path);
    f << "garbage_header_line\n";            // first line is skipped
    for (const auto& b : bars) {
        f << b.index   << ","   // 0
          << b.open    << ","   // 1
          << b.high    << ","   // 2
          << b.low     << ","   // 3
          << b.close   << ","   // 4
          << b.volume  << ","   // 5
          << "0"       << ","   // 6  (ignored column, e.g. Stock Splits)
          << b.timestamp        // 7
          << "\n";
    }
}

/** Build a vector of N synthetic bars with simple, deterministic values. */
static std::vector<Bar> make_bars(int n,
                                  double base_price = 100.0,
                                  unsigned long base_ts = 1'000'000UL)
{
    std::vector<Bar> bars;
    bars.reserve(n);
    for (int i = 0; i < n; ++i) {
        double noise = (i % 5) * 0.5;          // small, repeating pattern
        bars.push_back({
            i,                                  // index
            base_ts + static_cast<unsigned long>(i) * 86400UL,  // timestamp
            base_price + noise,                 // open
            base_price + noise + 2.0,           // high
            base_price + noise - 2.0,           // low
            base_price + noise + 0.5,           // close
            1'000'000UL                         // volume
        });
    }
    return bars;
}

/** Push a MarketEvent for each bar in `bars` onto the ExecutionHandler. */
static void seed_past_bars(ExecutionHandler& eh, const std::vector<Bar>& bars,
                           const std::string& ticker = "TEST")
{
    for (const auto& b : bars) {
        auto me = std::make_unique<MarketEvent>(ticker, b);
        eh.handle_market_event(*me);
    }
}


// ─────────────────────────────────────────────────────────────────────────────
// ── Section 1 : DataHandler ──────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────────────────────

void test_data_csv_row_count()
{
    // The queue should contain exactly as many bars as rows written (minus the header).
    auto bars = make_bars(5);
    write_temp_csv("TEST_ROWCOUNT", bars);

    DataHandler dh;
    dh.read_csv("TEST_ROWCOUNT");

    int count = 0;
    while (!dh.instrument_bars(0).empty()) { dh.get_next_market_event(); ++count; }
    ASSERT(count == 5);
}

void test_data_csv_field_values()
{
    // Confirm that open / high / low / close / volume / timestamp are parsed
    // into the correct fields (column mapping in data.cpp).
    Bar expected{0, 1'620'000'000UL, 150.0, 155.0, 148.0, 152.0, 5'000'000UL};
    write_temp_csv("TEST_FIELDS", {expected});

    DataHandler dh;
    dh.read_csv("TEST_FIELDS");

    Bar got = dh.get_next_market_event();
    ASSERT(got.index     == expected.index);
    ASSERT(got.timestamp == expected.timestamp);
    ASSERT_NEAR(got.open,  expected.open,  1e-9);
    ASSERT_NEAR(got.high,  expected.high,  1e-9);
    ASSERT_NEAR(got.low,   expected.low,   1e-9);
    ASSERT_NEAR(got.close, expected.close, 1e-9);
    ASSERT(got.volume    == expected.volume);
}

void test_data_queue_fifo_order()
{
    // get_next_market_event() should return bars in the order they were written.
    auto bars = make_bars(3);
    write_temp_csv("TEST_FIFO", bars);

    DataHandler dh;
    dh.read_csv("TEST_FIFO");

    for (int i = 0; i < 3; ++i) {
        Bar b = dh.get_next_market_event();
        ASSERT(b.index == i);
    }
}

void test_data_empty_after_drain()
{
    auto bars = make_bars(2);
    write_temp_csv("TEST_EMPTY", bars);

    DataHandler dh;
    dh.read_csv("TEST_EMPTY");
    dh.get_next_market_event();
    dh.get_next_market_event();
    ASSERT(dh.bars.empty());
}


// ─────────────────────────────────────────────────────────────────────────────
// ── Section 2 : ExecutionHandler event queue ─────────────────────────────────
// ─────────────────────────────────────────────────────────────────────────────

void test_event_queue_fifo()
{
    // Events must come out in the order they were pushed.
    ExecutionHandler eh;

    Bar b1{0, 1000UL, 10, 11, 9, 10, 100UL};
    Bar b2{1, 2000UL, 20, 21, 19, 20, 200UL};

    eh.add_event(std::make_unique<MarketEvent>("X", b1));
    eh.add_event(std::make_unique<MarketEvent>("X", b2));

    auto e1 = eh.pop_event();
    auto e2 = eh.pop_event();

    ASSERT(e1->get_timestamp() == 1000UL);
    ASSERT(e2->get_timestamp() == 2000UL);
}

void test_event_type_routing()
{
    // Each event class must report the correct EventType.
    Bar b{0, 1000UL, 10, 11, 9, 10, 100UL};
    MarketEvent me("X", b);
    SignalEvent  se(2000UL);
    Order        o(OrderType::MARKET, "X", 10.0, 1000UL, 0);
    OrderEvent   oe(o, 1000UL);

    ASSERT(me.get_type() == EventType::MARKETEVENT);
    ASSERT(se.get_type() == EventType::SIGNALEVENT);
    ASSERT(oe.get_type() == EventType::ORDEREVENT);
}

void test_orders_to_add_round_trip()
{
    // push_o_event_to_add should only accept ORDEREVENTS;
    // pop_o_event_to_add should return them in FIFO order.
    ExecutionHandler eh;

    Bar b{0, 1000UL, 10, 11, 9, 10, 100UL};
    Order o1(OrderType::LIMIT, "X", 5.0, 1000UL, 0, 9.5);
    Order o2(OrderType::LIMIT, "X", 3.0, 2000UL, 1, 9.0);

    eh.push_o_event_to_add(std::make_unique<OrderEvent>(o1, 1000UL));
    eh.push_o_event_to_add(std::make_unique<OrderEvent>(o2, 2000UL));

    // A non-order event should be silently rejected.
    eh.push_o_event_to_add(std::make_unique<MarketEvent>("X", b));

    // Only the two order events must remain.
    ASSERT(!eh.orders_to_add_empty());
    auto r1 = eh.pop_o_event_to_add();
    auto r2 = eh.pop_o_event_to_add();
    ASSERT(eh.orders_to_add_empty());

    ASSERT(r1->get_timestamp() == 1000UL);
    ASSERT(r2->get_timestamp() == 2000UL);
}


// ─────────────────────────────────────────────────────────────────────────────
// ── Section 3 : MarketEvent handler ──────────────────────────────────────────
// ─────────────────────────────────────────────────────────────────────────────

void test_handle_market_event_accumulates_bars()
{
    // Each call to handle_market_event should grow past_bars.
    // We verify indirectly: after 252 bars, handle_order_event no longer
    // returns early, which means past_bars must have been populated.
    // Here we just confirm no exception is thrown.
    ExecutionHandler eh;
    auto bars = make_bars(5);
    for (auto& b : bars) {
        auto me = std::make_unique<MarketEvent>("TEST", b);
        eh.handle_market_event(*me);    // must not throw
    }
    // If we reach here, bars were accepted without error.
    ASSERT(true);
}


// ─────────────────────────────────────────────────────────────────────────────
// ── Section 4 : Order handling ───────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────────────────────

void test_order_below_252_bars_not_filled()
{
    // With fewer than 252 past bars, handle_order_event should return early
    // and produce no FillEvent (event queue stays empty).
    auto bars = make_bars(10);
    write_temp_csv("TEST_UNDERFILL", make_bars(300));  // plenty of future bars

    DataHandler dh;
    dh.read_csv("TEST_UNDERFILL");

    ExecutionHandler eh;
    seed_past_bars(eh, bars);               // only 10 bars in past_bars

    Order o(OrderType::MARKET, "TEST_UNDERFILL", 1.0, bars.back().timestamp, 9);
    auto oe = std::make_unique<OrderEvent>(o, bars.back().timestamp);
    eh.handle_order_event(*oe, dh);

    // The event_queue should be empty: no FillEvent was generated.
    // (We pop speculatively — if it throws, the queue was empty, which is what we want.)
    std::unique_ptr<Event> e_p = eh.pop_event();
 
    ASSERT(e_p == nullptr);
}

void test_market_order_generates_fill()
{
    // With 252+ past bars and at least one future bar in the DataHandler,
    // a market order must produce exactly one FillEvent.
    auto past  = make_bars(252, 100.0);
    auto future = make_bars(2,  100.0, 1'000'000UL + 252UL * 86400UL);
    write_temp_csv("TEST_MKT", future);

    DataHandler dh("TEST_MKT");
    dh.read_csv();

    ExecutionHandler eh;
    seed_past_bars(eh, past);

    Order o(OrderType::MARKET, "TEST_MKT", 1.0,
            past.back().timestamp, past.back().index);
    auto oe = std::make_unique<OrderEvent>(o, past.back().timestamp);
    eh.handle_order_event(*oe, dh);

    // A FillEvent should be the first (and only) event on the queue.
    auto ev = eh.pop_event();
    if(ev){
        std::cout << "not null\n";
    }else{
        std::cout << "null\n";
    }
    ASSERT(ev && ev->get_type() == EventType::FILLEVENT);
}

void test_limit_order_gap_fill_buy()
{
    // A buy limit order with limit_price ABOVE the next bar's open triggers
    // a gap fill (always filled).
    auto past = make_bars(252, 100.0);

    // Future bar: open=95, high=98, low=93, close=96
    Bar future_bar{252, 1'000'000UL + 252UL * 86400UL, 95.0, 98.0, 93.0, 96.0, 1'000'000UL};
    write_temp_csv("TEST_LGAP", {future_bar});

    DataHandler dh("TEST_LGAP");
    dh.read_csv();

    ExecutionHandler eh;
    seed_past_bars(eh, past);

    // limit_price=99 > open=95  →  gap fill, must produce a FillEvent
    Order o(OrderType::LIMIT, "TEST_LGAP", 10.0,
            past.back().timestamp, past.back().index, /*price=*/99.0);
    auto oe = std::make_unique<OrderEvent>(o, past.back().timestamp);
    eh.handle_order_event(*oe, dh);

    auto ev = eh.pop_event();
    ASSERT(ev->get_type() == EventType::FILLEVENT);
}

void test_limit_order_not_reached()
{
    // A buy limit order whose limit_price is BELOW the next bar's low should
    // never be filled, and the order should be re-queued in orders_to_add.
    auto past = make_bars(252, 100.0);

    // Future bar: open=105, high=108, low=103
    Bar future_bar{252, 1'000'000UL + 252UL * 86400UL, 105.0, 108.0, 103.0, 106.0, 1'000'000UL};
    write_temp_csv("TEST_LMISS", {future_bar});

    DataHandler dh("TEST_LMISS");
    dh.read_csv();

    ExecutionHandler eh;
    seed_past_bars(eh, past);

    // limit_price=100 < low=103  →  market never touches the price
    Order o(OrderType::LIMIT, "TEST_LMISS", 10.0,
            past.back().timestamp, past.back().index, /*price=*/100.0);
    auto oe = std::make_unique<OrderEvent>(o, past.back().timestamp);
    eh.handle_order_event(*oe, dh);

    // No FillEvent on event_queue
    bool fill_found = false;
    auto ev = eh.pop_event();
    if(ev){
        fill_found = (ev->get_type() == EventType::FILLEVENT);
    }
    ASSERT(!fill_found);

    // Order re-queued
    ASSERT(!eh.orders_to_add_empty());
}

void test_stop_order_gap_buy()
{
    // A buy stop order where open > stop_price should fill at open (stop gap slippage).
    auto past = make_bars(252, 100.0);

    // Future bar: open=112, high=115, low=110  →  open > stop_price=105
    Bar future_bar{252, 1'000'000UL + 252UL * 86400UL, 112.0, 115.0, 110.0, 113.0, 1'000'000UL};
    write_temp_csv("TEST_STOP", {future_bar});

    DataHandler dh("TEST_STOP");
    dh.read_csv();

    ExecutionHandler eh;
    seed_past_bars(eh, past);

    Order o(OrderType::STOP, "TEST_STOP", 5.0,
            past.back().timestamp, past.back().index, /*stop_price=*/105.0);
    auto oe = std::make_unique<OrderEvent>(o, past.back().timestamp);
    eh.handle_order_event(*oe, dh);

    auto ev = eh.pop_event();
    ASSERT(ev->get_type() == EventType::FILLEVENT);
}

void test_expired_order_not_filled()
{
    auto past = make_bars(260, 100.0);  // last bar has index=259

    Bar future_bar{260, 1'000'000UL + 260UL * 86400UL, 100.0, 102.0, 98.0, 101.0, 1'000'000UL};
    write_temp_csv("TEST_EXPIRY", {future_bar});

    DataHandler dh("TEST_EXPIRY");
    dh.read_csv();

    ExecutionHandler eh;
    seed_past_bars(eh, past);

    // LIMIT order created at index=257, time_in_force=1.
    // last_bar.index=259, 259 - 257 = 2 > 1  →  expired.
    Order o(OrderType::LIMIT, "TEST_EXPIRY", 1.0,
            past[257].timestamp, /*index=*/257, /*price=*/99.0, /*time_in_force=*/1);
    auto oe = std::make_unique<OrderEvent>(o, past[257].timestamp);
    eh.handle_order_event(*oe, dh);

    bool fill_found = false;
    auto ev = eh.pop_event();
    ASSERT(!ev && eh.orders_to_add_empty());
}


// ─────────────────────────────────────────────────────────────────────────────
// ── Section 5 : Volatility & spread helpers ───────────────────────────────────
// ─────────────────────────────────────────────────────────────────────────────

// Forward-declare the free functions we want to test directly.
double yang_zhang_rolling(const std::vector<Bar>& bars, int window);
double garman_klass_rolling(const std::vector<Bar>& bars, int window);
double volume_to_spread(double volume, double price);
double limit_fill_probability(double limit_price, const Bar& bar, double adv, double order_size);
std::pair<double,double> compute_vol_ratio_n_realized_vol(const std::vector<Bar>& bars);



void test_volume_to_spread_tiers()
{
    // Each dollar-volume tier must return the documented spread.
    ASSERT_NEAR(volume_to_spread(1e7, 100.0), 0.0001, 1e-9);  // 1e9 notional
    ASSERT_NEAR(volume_to_spread(1e6, 100.0), 0.0002, 1e-9);  // 1e8 notional
    ASSERT_NEAR(volume_to_spread(1e5, 100.0), 0.0005, 1e-9);  // 1e7 notional
    ASSERT_NEAR(volume_to_spread(1e4, 100.0), 0.0010, 1e-9);  // 1e6 notional
    ASSERT_NEAR(volume_to_spread(1e2, 100.0), 0.0050, 1e-9);  // < 1e6
}

void test_limit_fill_probability_bounds()
{
    // Result must always lie in [0, 1].
    Bar b{0, 0UL, 100.0, 110.0, 90.0, 100.0, 1'000'000UL};
    double adv = 1'000'000.0;

    // Buy at various penetration levels
    for (double lp : {91.0, 95.0, 100.0, 109.0}) {
        double p = limit_fill_probability(lp, b, adv, 10.0);
        ASSERT(p >= 0.0 && p <= 1.0);
    }

    // Sell at various penetration levels
    for (double lp : {91.0, 95.0, 105.0, 109.0}) {
        double p = limit_fill_probability(lp, b, adv, -10.0);
        ASSERT(p >= 0.0 && p <= 1.0);
    }
}

void test_yang_zhang_returns_positive()
{
    // Volatility estimates should be strictly positive for non-trivial data.
    auto bars = make_bars(30, 100.0);
    double vol = yang_zhang_rolling(bars, 20);
    ASSERT(vol > 0.0);
}

void test_compute_vol_ratio_requires_252_bars()
{
    // Must throw with fewer than 252 bars.
    auto bars = make_bars(100);
    bool threw = false;
    try {
        compute_vol_ratio_n_realized_vol(bars);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    ASSERT(threw);
}

void test_compute_vol_ratio_ok_with_252_bars()
{
    auto bars = make_bars(300, 100.0);
    auto [ratio, short_vol] = compute_vol_ratio_n_realized_vol(bars);
    ASSERT(ratio > 0.0);
    ASSERT(short_vol > 0.0);
}


// ─────────────────────────────────────────────────────────────────────────────
// ── Section 6 : post_loop_check ──────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────────────────────

void test_post_loop_check_adds_market_event()
{
    // post_loop_check must consume one bar from the DataHandler and push a
    // MarketEvent onto the event_queue.
    auto bars = make_bars(2);
    write_temp_csv("TEST_POST", bars);

    DataHandler dh("TEST_POST");
    dh.read_csv();

    ExecutionHandler eh;
    eh.post_loop_check(dh);

    auto ev = eh.pop_event();
    ASSERT(ev->get_type() == EventType::MARKETEVENT);
    ASSERT(dh.bars.size() == 1);  // one bar consumed
}

void test_post_loop_check_flushes_orders_to_add()
{
    // Pending orders in orders_to_add should be moved to the main event_queue.
    auto bars = make_bars(1);
    write_temp_csv("TEST_FLUSH", bars);

    DataHandler dh("TEST_FLUSH");
    dh.read_csv();

    ExecutionHandler eh;
    Order o(OrderType::LIMIT, "TEST_FLUSH", 5.0, 1000UL, 0, 99.0);
    eh.push_o_event_to_add(std::make_unique<OrderEvent>(o, 1000UL));

    eh.post_loop_check(dh);

    // Queue should now have: MarketEvent (from the bar) + OrderEvent (from orders_to_add)
    auto ev1 = eh.pop_event();
    auto ev2 = eh.pop_event();

    ASSERT(ev1->get_type() == EventType::MARKETEVENT);
    ASSERT(ev2->get_type() == EventType::ORDEREVENT);
    ASSERT(eh.orders_to_add_empty());
}

void test_post_loop_check_empty_data_no_market_event()
{
    // When the DataHandler is drained, post_loop_check must not push a MarketEvent.
    DataHandler dh("NONEXISTENT");   // no CSV read → bars queue is empty

    ExecutionHandler eh;
    eh.post_loop_check(dh);

    bool any_event = false;
    auto event = eh.pop_event();
    if(event){
        any_event = true;
    }
    ASSERT(!any_event);
}


// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────
int main()
{
    std::cout << "\n=== DataHandler ===\n";
    RUN_TEST(test_data_csv_row_count);
    RUN_TEST(test_data_csv_field_values);
    RUN_TEST(test_data_queue_fifo_order);
    RUN_TEST(test_data_empty_after_drain);

    std::cout << "\n=== Event queue ===\n";
    RUN_TEST(test_event_queue_fifo);
    RUN_TEST(test_event_type_routing);
    RUN_TEST(test_orders_to_add_round_trip);

    std::cout << "\n=== Market event handler ===\n";
    RUN_TEST(test_handle_market_event_accumulates_bars);

    std::cout << "\n=== Order handling ===\n";
    RUN_TEST(test_order_below_252_bars_not_filled);
    RUN_TEST(test_market_order_generates_fill);
    RUN_TEST(test_limit_order_gap_fill_buy);
    RUN_TEST(test_limit_order_not_reached);
    RUN_TEST(test_stop_order_gap_buy);
    RUN_TEST(test_expired_order_not_filled);

    std::cout << "\n=== Volatility & spread helpers ===\n";
    RUN_TEST(test_volume_to_spread_tiers);
    RUN_TEST(test_limit_fill_probability_bounds);
    RUN_TEST(test_yang_zhang_returns_positive);
    RUN_TEST(test_compute_vol_ratio_requires_252_bars);
    RUN_TEST(test_compute_vol_ratio_ok_with_252_bars);

    std::cout << "\n=== post_loop_check ===\n";
    RUN_TEST(test_post_loop_check_adds_market_event);
    RUN_TEST(test_post_loop_check_flushes_orders_to_add);
    RUN_TEST(test_post_loop_check_empty_data_no_market_event);

    std::cout << "\n──────────────────────────────────────\n";
    std::cout << tests_passed << " / " << tests_run << " tests passed\n\n";

    return (tests_passed == tests_run) ? 0 : 1;
}
