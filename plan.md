BACKTESTER IMPLEMENTATION ROADMAP
==================================

PHASE 0: ENVIRONMENT
- CMake build system configured
- Catch2 or Google Test integrated
- Directory structure created (src/core, data, strategy, execution, portfolio, risk, performance | python/ingest, analysis, visualization | tests/ output/)
- Git repository initialized
DONE WHEN: CMake builds cleanly, empty test suite runs, initial commit made.

PHASE 1: DATA LAYER
- Bar struct (timestamp as int64, open, high, low, close, volume, adj_close)
- DataHandler class (reads CSV, feeds bars one at a time, maintains history buffer)
- Python ingestion script (downloads data via yfinance, outputs to agreed CSV schema)
DONE WHEN: Tests verify bars load correctly, return in timestamp order, future bars are inaccessible.

PHASE 2: EVENT SYSTEM
- Base Event class with type enum and timestamp
- Concrete event types: MarketEvent, SignalEvent, OrderEvent, FillEvent
- EventQueue class (chronologically ordered, push/pop interface)
- Engine main loop (owns queue + all subsystems, dispatches events, runs until data exhausted)
DONE WHEN: Tests verify chronological ordering, mock DataHandler produces correct MarketEvents, loop terminates cleanly.

PHASE 3: EXECUTION LAYER
- Order class with state machine (Created → Submitted → PartialFill → Filled / Cancelled / Rejected)
- Fill class (immutable value type)
- ExecutionHandler class with three models:
  [1] Naive: fill at close, zero slippage
  [2] Simple: fill at next open, fixed slippage + commission
  [3] Advanced: fill at next open, volume-proportional slippage, spread adjustment
DONE WHEN: Tests verify market order fill price, limit order fill logic, filled orders cannot be refilled.

PHASE 4: PORTFOLIO LAYER
- Position class (asset, quantity, avg entry price, unrealized P&L)
- PortfolioManager class (owns positions map, cash balance, realized P&L, equity curve)
- Mark-to-market on every bar
- Reconciliation check: cash + position market values == total portfolio value (runs after every update, halts on failure)
DONE WHEN: Tests verify cash accounting on buy/sell, average entry price on multi-tranche fills, P&L calculation, reconciliation catches injected errors.

PHASE 5: STRATEGY LAYER
- Strategy abstract base class (receives MarketEvent, emits SignalEvent, no future data access)
- PositionSizer class (receives SignalEvent + portfolio state, emits OrderEvent, fixed fractional sizing)
- First concrete strategy: moving average crossover (fast/slow periods as constructor params)
DONE WHEN: Tests verify signal fires at correct bar, no false signals, full integration test produces at least one complete round-trip trade on synthetic data.

PHASE 6: RISK MANAGER
- RiskManager class sitting between PositionSizer and ExecutionHandler
- Checks: max position size (fraction of NAV), max drawdown halt, max open positions
- Actions: approve / scale down / reject incoming orders, generate forced exits on breach
DONE WHEN: Tests verify oversized orders are scaled, drawdown halt stops trading, clean orders pass through unchanged.

PHASE 7: PERFORMANCE CALCULATOR
- PerformanceCalculator class (inputs: equity curve + trade log)
- Metrics: CAGR, max drawdown + duration, Sharpe, Sortino, win rate, profit factor, avg trade duration, turnover, Calmar
- Series outputs: full drawdown series, rolling Sharpe
- CSV outputs: summary metrics, equity curve + drawdown, trade log, fill log
DONE WHEN: Tests verify each metric against hand-calculated reference. All CSVs readable by pandas.

PHASE 8: PYTHON LAYER
- Dashboard (Plotly + Dash):
  [1] Equity curve + drawdown panel with benchmark overlay
  [2] Monthly returns heatmap
  [3] Return distribution with normal overlay
  [4] Trade P&L scatter plot
  [5] Win/loss distribution
- Parameter sensitivity script: calls C++ binary via subprocess across parameter grid, plots Sharpe heatmap
DONE WHEN: Single script launches dashboard in browser with all panels populated. Sensitivity heatmap renders across 10x10 parameter grid.

PHASE 9: VALIDATION AND HARDENING
- Run on synthetic data with known signals, verify exact trade count and P&L
- Deliberately introduce lookahead bias, verify results change (proves system is sensitive to it)
- Run on real data, sanity-check results against published MA crossover benchmarks
- Profile C++ engine, identify hot path, understand where time goes
DONE WHEN: System produces results consistent with known references. No reconciliation errors on full historical run. Hot path identified and documented.

==================================
IMPLEMENTATION ORDER IS STRICT.
Strategy is Phase 5. Four phases of infrastructure before any strategy logic.
Design on paper first. Implement second. Test before moving to next phase.
==================================


Things to keep in mind + Plan

Market structure and mechanics.
Bid-ask spread. Ask price. Bid price. Market maker. Liquidity. Slippage. Implementation shortfall. Market impact. Square-root market impact model. Commission. Exchange fee. Market order. Limit order. Stop order. Stop slippage. Gap risk. Partial fill. Fill. Time in force. Day order. Good till cancelled. Average daily volume. Order book. Price-time priority. Survivorship bias. Point-in-time database. Lookahead bias. Adjusted prices. Unadjusted prices. Corporate action. Stock split. Dividend adjustment. Roll's spread estimator. Basis points.

Trade lifecycle.
Signal. Order. Fill. Position. Average entry price. Mark to market. Unrealized P&L. Realized P&L. Maximum adverse excursion. Maximum favorable excursion. Trade log. Fill log. Order state machine. Reconciliation. Fixed fractional sizing. Volatility targeting. Kelly criterion. Full Kelly. Fractional Kelly.

Performance metrics.
Equity curve. Return series. CAGR. Maximum drawdown. Drawdown duration. Recovery time. Sharpe ratio. Sortino ratio. Downside deviation. Win rate. Profit factor. Average trade duration. Turnover. Calmar ratio. Rolling Sharpe. T-statistic of the Sharpe ratio. Monte Carlo analysis. Fat tails. Skewness. Kurtosis. Normal distribution. In-sample period. Out-of-sample period. Walk-forward analysis. Parameter sensitivity. Overfitting. Curve fitting.

Architecture.
Event-driven architecture. Event queue. Market event. Signal event. Order event. Fill event. Risk event. Event handler. State machine. Vectorized backtesting. Feedback loop. Hot path. Separation of concerns. Data contract. Schema.

C++ and Python integration.
pybind11. Shared library. Extension module. Subprocess. Pipe. Serialization. CSV. Parquet. HDF5. FlatBuffers. MessagePack. Protocol Buffers. GIL. Buffer protocol. NumPy array. CMake. Build system.

Visualization.
Plotly. Dash. Matplotlib. Seaborn. Drawdown chart. Monthly returns heatmap. Return distribution. Rolling volatility. Position concentration. Parameter sensitivity surface. Out-of-sample comparison. Implementation shortfall.