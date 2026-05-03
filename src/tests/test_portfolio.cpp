// test_portfolio.cpp
// Self-contained test file for Position and PortfolioManager
// Compile with: g++ -std=c++17 -o test_portfolio test_portfolio.cpp
// Run with: ./test_portfolio

#include <iostream>
#include <cmath>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <algorithm>

// ─────────────────────────────────────────────
// Minimal stubs so we can test without the full codebase
// ─────────────────────────────────────────────

struct Bar {
    int index;
    unsigned long timestamp;
    double open;
    double high;
    double low;
    double close;
    unsigned long volume;
};

class Fill {
    int instrument_id;
    double size;
    double filled_price;
    int order_id;
public:
    static constexpr double commission_rate_per_share = 0.005;
    static constexpr double min_commission_rate = 1.0;

    Fill(int instrument_id, double size, double price, int order_id)
        : instrument_id(instrument_id), size(size), filled_price(price), order_id(order_id) {}

    double get_commission() const {
        return std::max(min_commission_rate, std::abs(size) * commission_rate_per_share);
    }

    double get_filled_price() const { return filled_price; }
    double get_size()         const { return size; }
    int    get_instrument_id()const { return instrument_id; }
};

// ─────────────────────────────────────────────
// Position (copy-pasted from your implementation)
// ─────────────────────────────────────────────

class Position {
    int instrument_id;
    double size;
    double avg_entry_price;
    double unrealized_PL;
    double realized_PL;

public:
    Position() : instrument_id(0), size(0.0), avg_entry_price(0.0), unrealized_PL(0.0), realized_PL(0.0) {}
    Position(int id) : instrument_id(id), size(0.0), avg_entry_price(0.0), unrealized_PL(0.0), realized_PL(0.0) {}

    void update_unrealized_PL(Bar& bar) {
        unrealized_PL = (bar.close - avg_entry_price) * size;
    }

    void update_avg_entry_price(double sz, double filled_price) {
        avg_entry_price = (std::abs(size) * avg_entry_price + std::abs(sz) * filled_price)
                        / (std::abs(size) + std::abs(sz));
    }

    void close_position(double fill_price) {
        double pnl;
        if (size > 0)
            pnl = (fill_price - avg_entry_price) * std::abs(size);
        else
            pnl = (avg_entry_price - fill_price) * std::abs(size);

        realized_PL += pnl;
        realized_PL -= std::max(Fill::min_commission_rate,
                                std::abs(size) * Fill::commission_rate_per_share);
        size = 0.0;
        avg_entry_price = 0.0;
    }

    void add_to_position(Fill& fill) {
        double fill_direction   = std::copysign(1.0, fill.get_size());
        double portfolio_direction = std::copysign(1.0, size);
        double fill_size  = fill.get_size();
        double fill_price = fill.get_filled_price();

        if (fill_direction == portfolio_direction || portfolio_direction == 0.0) {
            update_avg_entry_price(fill_size, fill_price);
            set_size(size + fill_size);
        } else if (std::abs(fill_size) < std::abs(size)) {
            set_size(size + fill_size);
            double pnl;
            if (fill_size < 0)
                pnl = (fill_price - avg_entry_price) * std::abs(fill_size);
            else
                pnl = (avg_entry_price - fill_price) * std::abs(fill_size);
            realized_PL += pnl;
        } else if (std::abs(std::abs(fill_size) - std::abs(size)) < 1e-5) {
            close_position(fill_price);
        } else {
            double size_difference = std::copysign(std::abs(fill_size) - std::abs(size), fill_size);
            close_position(fill_price);
            size = size_difference;
            avg_entry_price = fill_price;
        }
    }

    int    get_instrument_id()  const { return instrument_id; }
    double get_size()           const { return size; }
    double get_avg_entry_price()const { return avg_entry_price; }
    double get_unrealized_PL()  const { return unrealized_PL; }
    double get_realized_PL()    const { return realized_PL; }
    void   set_size(double s)         { size = s; }
};

// ─────────────────────────────────────────────
// PortfolioManager (copy-pasted from your implementation)
// ─────────────────────────────────────────────

class PortfolioManager {
    double cash;
    std::vector<double> equity_ts;
    double curr_equity;
    std::unordered_map<int, Position> positions;
    std::unordered_set<int> instrument_ids;

public:
    PortfolioManager(double cash, std::unordered_set<int> ids)
        : cash(cash), curr_equity(cash), instrument_ids(ids) {}

    void close_position(int instrument_id) {
        positions.erase(instrument_id);
        instrument_ids.erase(instrument_id);
    }

    void update_instrument_position_from_fill(int instrument_id, Fill& fill) {
        double fill_size = fill.get_size();
        if (fill_size > 0)
            cash -= fill_size * fill.get_filled_price() + fill.get_commission();
        else
            cash += (-fill_size) * fill.get_filled_price() - fill.get_commission();

        if (positions.find(instrument_id) != positions.end()) {
            positions[instrument_id].add_to_position(fill);
        } else {
            positions[instrument_id] = Position(instrument_id);
            instrument_ids.insert(instrument_id);
            positions[instrument_id].add_to_position(fill);
        }

        if (std::abs(positions[instrument_id].get_size()) < 1e-4)
            close_position(instrument_id);
    }

    void update_positions_from_bars(std::vector<std::pair<int, Bar>>& new_bars) {
        double new_equity = cash;
        for (const auto& pair : new_bars)
            new_equity += positions[pair.first].get_size() * pair.second.close;
        curr_equity = new_equity;
        equity_ts.push_back(new_equity);
    }

    bool equity_value_check(std::vector<std::pair<int, Bar>>& present_bars) {
        double computed = cash;
        for (const auto& pair : present_bars)
            computed += positions[pair.first].get_size() * pair.second.close;
        return std::abs(computed - curr_equity) < 1e-4;
    }

    // Test-only method to deliberately corrupt cash (for reconciliation test)
    void corrupt_cash(double amount) { cash += amount; }

    double get_cash()   const { return cash; }
    double get_equity() const { return curr_equity; }

    Position* get_position(int id) {
        auto it = positions.find(id);
        return it != positions.end() ? &it->second : nullptr;
    }
};

// ─────────────────────────────────────────────
// Tiny test framework
// ─────────────────────────────────────────────

static int tests_run    = 0;
static int tests_passed = 0;

void check(bool condition, const std::string& test_name, const std::string& detail = "") {
    ++tests_run;
    if (condition) {
        ++tests_passed;
        std::cout << "  [PASS] " << test_name << "\n";
    } else {
        std::cout << "  [FAIL] " << test_name;
        if (!detail.empty()) std::cout << " — " << detail;
        std::cout << "\n";
    }
}

bool near(double a, double b, double tol = 1e-6) {
    return std::abs(a - b) < tol;
}

// ─────────────────────────────────────────────
// Tests
// ─────────────────────────────────────────────

// ── 1. Buy fill: cash decreases by exactly quantity × price + commission
void test_buy_fill_cash_accounting() {
    std::cout << "\n[Test 1] Buy fill cash accounting\n";
    PortfolioManager pm(10000.0, {});
    Fill buy(1, 100.0, 50.0, 1); // 100 shares at $50
    // expected cash decrease = 100*50 + max(1, 100*0.005) = 5000 + 1 = 5001
    double expected_cash = 10000.0 - (100.0 * 50.0 + buy.get_commission());
    pm.update_instrument_position_from_fill(1, buy);
    check(near(pm.get_cash(), expected_cash), "Cash after buy",
          "expected=" + std::to_string(expected_cash) + " got=" + std::to_string(pm.get_cash()));
}

// ── 2. Buy then sell at higher price: realized P&L = (exit-entry)*qty - exit_commission
//       Cash must reflect the full round trip correctly
void test_buy_then_sell_realized_pnl() {
    std::cout << "\n[Test 2] Buy then sell — realized P&L and cash\n";
    const double initial_cash = 10000.0;
    PortfolioManager pm(initial_cash, {});

    Fill buy(1, 100.0, 10.0, 1);   // buy  100 @ $10
    Fill sell(1, -100.0, 15.0, 2); // sell 100 @ $15

    double buy_commission  = buy.get_commission();
    double sell_commission = sell.get_commission();

    pm.update_instrument_position_from_fill(1, buy);
    pm.update_instrument_position_from_fill(1, sell);

    // After full close, position should be gone
    check(pm.get_position(1) == nullptr, "Position removed after full close");

    // Cash: started with 10000, paid 100*10 + comm_buy, received 100*15 - comm_sell
    double expected_cash = initial_cash
                         - (100.0 * 10.0 + buy_commission)
                         + (100.0 * 15.0 - sell_commission);
    check(near(pm.get_cash(), expected_cash), "Cash after round trip",
          "expected=" + std::to_string(expected_cash) + " got=" + std::to_string(pm.get_cash()));

    // Realized P&L on the position = (15-10)*100 - sell_commission
    // (buy commission already left via cash, only exit commission hits realized PL)
    double expected_realized = (15.0 - 10.0) * 100.0 - sell_commission;

    // We verify indirectly: cash increase from trading = realized_pnl_economic - total_commissions
    double cash_change = pm.get_cash() - initial_cash;
    double expected_cash_change = (15.0 - 10.0) * 100.0 - buy_commission - sell_commission;
    check(near(cash_change, expected_cash_change), "Net cash change equals price gain minus total commissions",
          "expected=" + std::to_string(expected_cash_change) + " got=" + std::to_string(cash_change));
}

// ── 3. Average entry price across two tranches
void test_average_entry_price_two_tranches() {
    std::cout << "\n[Test 3] Average entry price — two tranches\n";
    PortfolioManager pm(50000.0, {});

    Fill buy1(1, 100.0, 10.0, 1); // 100 shares @ $10
    Fill buy2(1, 200.0, 20.0, 2); // 200 shares @ $20
    // expected avg = (100*10 + 200*20) / (100+200) = (1000+4000)/300 = 5000/300 = 16.6667

    pm.update_instrument_position_from_fill(1, buy1);
    pm.update_instrument_position_from_fill(1, buy2);

    Position* pos = pm.get_position(1);
    double expected_avg = (100.0 * 10.0 + 200.0 * 20.0) / 300.0;
    check(pos != nullptr, "Position exists after two buys");
    if (pos) {
        check(near(pos->get_avg_entry_price(), expected_avg), "Average entry price correct",
              "expected=" + std::to_string(expected_avg) + " got=" + std::to_string(pos->get_avg_entry_price()));
        check(near(pos->get_size(), 300.0), "Total size is 300");
    }
}

// ── 4. Partial close realizes P&L on sold portion only, avg entry price unchanged
void test_partial_close_pnl() {
    std::cout << "\n[Test 4] Partial close — realized P&L and avg entry price unchanged\n";
    PortfolioManager pm(50000.0, {});

    Fill buy(1, 100.0, 10.0, 1);      // buy  100 @ $10
    Fill sell_half(1, -50.0, 15.0, 2); // sell  50 @ $15

    pm.update_instrument_position_from_fill(1, buy);
    pm.update_instrument_position_from_fill(1, sell_half);

    Position* pos = pm.get_position(1);
    check(pos != nullptr, "Position still open after partial close");
    if (pos) {
        check(near(pos->get_size(), 50.0), "Remaining size is 50");
        check(near(pos->get_avg_entry_price(), 10.0), "Avg entry price unchanged after partial close");
        // Realized P&L on the closed portion = (15-10)*50 = 250 (no exit commission deducted here since it's a partial)
        double expected_realized = (15.0 - 10.0) * 50.0;
        check(near(pos->get_realized_PL(), expected_realized), "Realized P&L on partial close",
              "expected=" + std::to_string(expected_realized) + " got=" + std::to_string(pos->get_realized_PL()));
    }
}

// ── 5. Short position: cash increases on open, decreases on cover, P&L correct
void test_short_position_accounting() {
    std::cout << "\n[Test 5] Short position accounting\n";
    const double initial_cash = 10000.0;
    PortfolioManager pm(initial_cash, {});

    Fill short_open(1, -100.0, 50.0, 1); // short 100 @ $50 — cash should increase
    Fill short_cover(1, 100.0, 40.0, 2); // cover 100 @ $40 — cash should decrease

    double open_commission  = short_open.get_commission();
    double cover_commission = short_cover.get_commission();

    pm.update_instrument_position_from_fill(1, short_open);

    // After opening short: cash = initial + 100*50 - commission
    double expected_cash_after_open = initial_cash + 100.0 * 50.0 - open_commission;
    check(near(pm.get_cash(), expected_cash_after_open), "Cash increases when opening short",
          "expected=" + std::to_string(expected_cash_after_open) + " got=" + std::to_string(pm.get_cash()));

    pm.update_instrument_position_from_fill(1, short_cover);

    // After covering: cash = previous - 100*40 - commission
    double expected_cash_final = expected_cash_after_open - 100.0 * 40.0 - cover_commission;
    check(near(pm.get_cash(), expected_cash_final), "Cash decreases when covering short",
          "expected=" + std::to_string(expected_cash_final) + " got=" + std::to_string(pm.get_cash()));

    // Net cash change = profit - total commissions = (50-40)*100 - open_comm - cover_comm
    double net_change = pm.get_cash() - initial_cash;
    double expected_net = (50.0 - 40.0) * 100.0 - open_commission - cover_commission;
    check(near(net_change, expected_net), "Net cash change correct on short round trip",
          "expected=" + std::to_string(expected_net) + " got=" + std::to_string(net_change));
}

// ── 6. Unrealized P&L uses close price of bar
void test_unrealized_pnl_uses_close() {
    std::cout << "\n[Test 6] Unrealized P&L uses bar close price\n";
    Position pos(1);
    Fill buy(1, 100.0, 10.0, 1);
    pos.add_to_position(buy);

    Bar bar{1, 1000UL, 11.0, 13.0, 9.0, 12.0, 1000UL}; // close = 12
    pos.update_unrealized_PL(bar);

    double expected = (12.0 - 10.0) * 100.0; // (close - avg_entry) * size
    check(near(pos.get_unrealized_PL(), expected), "Unrealized P&L = (close - entry) * size",
          "expected=" + std::to_string(expected) + " got=" + std::to_string(pos.get_unrealized_PL()));
}

// ── 7. Equity = cash + sum of position market values
void test_equity_equals_cash_plus_market_value() {
    std::cout << "\n[Test 7] Equity = cash + sum of position market values\n";
    PortfolioManager pm(10000.0, {});

    Fill buy(1, 100.0, 10.0, 1);
    pm.update_instrument_position_from_fill(1, buy);

    Bar bar{1, 1000UL, 12.0, 13.0, 11.0, 12.0, 1000UL}; // close = 12
    std::vector<std::pair<int, Bar>> bars = {{1, bar}};
    pm.update_positions_from_bars(bars);

    // equity = cash + 100 * 12
    double expected_equity = pm.get_cash() + 100.0 * 12.0;
    check(near(pm.get_equity(), expected_equity), "Equity = cash + position market value",
          "expected=" + std::to_string(expected_equity) + " got=" + std::to_string(pm.get_equity()));
}

// ── 8. Reconciliation check catches a deliberately introduced cash error
void test_reconciliation_catches_cash_error() {
    std::cout << "\n[Test 8] Reconciliation check catches cash corruption\n";
    PortfolioManager pm(10000.0, {});

    Fill buy(1, 100.0, 10.0, 1);
    pm.update_instrument_position_from_fill(1, buy);

    Bar bar{1, 1000UL, 10.0, 11.0, 9.0, 10.0, 1000UL};
    std::vector<std::pair<int, Bar>> bars = {{1, bar}};
    pm.update_positions_from_bars(bars);

    check(pm.equity_value_check(bars), "Reconciliation passes before corruption");

    // Deliberately corrupt cash
    pm.corrupt_cash(500.0);

    check(!pm.equity_value_check(bars), "Reconciliation FAILS after cash corruption");
}

// ── 9. Short unrealized P&L is negative when price rises
void test_short_unrealized_pnl() {
    std::cout << "\n[Test 9] Short unrealized P&L — negative when price rises\n";
    Position pos(1);
    Fill short_open(1, -100.0, 50.0, 1); // short 100 @ $50
    pos.add_to_position(short_open);

    Bar bar{1, 1000UL, 55.0, 57.0, 53.0, 55.0, 1000UL}; // price rose to 55
    pos.update_unrealized_PL(bar);

    // unrealized = (close - avg_entry) * size = (55 - 50) * (-100) = -500
    double expected = (55.0 - 50.0) * (-100.0);
    check(near(pos.get_unrealized_PL(), expected), "Short unrealized P&L negative when price rises",
          "expected=" + std::to_string(expected) + " got=" + std::to_string(pos.get_unrealized_PL()));
}

// ── 10. Position reversal: long to short, avg entry price resets to reversal price
void test_position_reversal() {
    std::cout << "\n[Test 10] Position reversal — long flips to short\n";
    PortfolioManager pm(50000.0, {});

    Fill buy(1, 100.0, 10.0, 1);        // long 100 @ $10
    Fill reverse(1, -150.0, 15.0, 2);   // sell 150: closes long 100, opens short 50

    pm.update_instrument_position_from_fill(1, buy);
    pm.update_instrument_position_from_fill(1, reverse);

    Position* pos = pm.get_position(1);
    check(pos != nullptr, "Position exists after reversal (short side open)");
    if (pos) {
        check(near(pos->get_size(), -50.0), "Size is -50 after reversal",
              "got=" + std::to_string(pos->get_size()));
        check(near(pos->get_avg_entry_price(), 15.0), "Avg entry price reset to reversal fill price",
              "got=" + std::to_string(pos->get_avg_entry_price()));
    }
}

// ─────────────────────────────────────────────
// Main
// ─────────────────────────────────────────────

int main() {
    std::cout << "========================================\n";
    std::cout << "  Portfolio & Position Unit Tests\n";
    std::cout << "========================================\n";

    test_buy_fill_cash_accounting();
    test_buy_then_sell_realized_pnl();
    test_average_entry_price_two_tranches();
    test_partial_close_pnl();
    test_short_position_accounting();
    test_unrealized_pnl_uses_close();
    test_equity_equals_cash_plus_market_value();
    test_reconciliation_catches_cash_error();
    test_short_unrealized_pnl();
    test_position_reversal();

    std::cout << "\n========================================\n";
    std::cout << "  " << tests_passed << " / " << tests_run << " tests passed\n";
    std::cout << "========================================\n";

    return (tests_passed == tests_run) ? 0 : 1;
}