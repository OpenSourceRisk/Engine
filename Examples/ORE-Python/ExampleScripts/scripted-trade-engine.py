# ---
# jupyter:
#   jupytext:
#     formats: py:percent
#     text_representation:
#       extension: .py
#       format_name: percent
#       format_version: '1.3'
#   kernelspec:
#     display_name: Python 3
#     language: python
#     name: python3
# ---

# %% [markdown]
# # Scripted Trade Engine — European Option Pricing
#
# This example demonstrates the ORE scripting engine Python bindings by
# implementing a European option pricer entirely within the scripting
# language. The Black-Scholes formula is expressed as a script, parsed
# into an AST, and evaluated by the ScriptEngine to produce a present
# value — all without XML round-trips or C++ extensions.
#
# Capabilities shown:
# - Parsing payoff scripts into an abstract syntax tree (AST)
# - Inspecting the AST structure programmatically
# - Setting up execution contexts with market variables
# - Running scripts via ScriptEngine to compute PV
# - Evaluating the same script across multiple market scenarios
# - Built-in math functions: `ln`, `exp`, `sqrt`, `normalCdf`, `max`

# %%
import ORE as ore
import math

# %% [markdown]
# ## 1. Black-Scholes Formula as an ORE Script
#
# The scripting language provides `ln`, `exp`, `sqrt`, and `normalCdf`
# functions, which are all we need to implement the closed-form
# Black-Scholes call/put price:
#
# $$C = S \cdot N(d_1) - K \cdot e^{-rT} \cdot N(d_2)$$
# $$P = K \cdot e^{-rT} \cdot N(-d_2) - S \cdot N(-d_1)$$
#
# where $d_1 = \frac{\ln(S/K) + (r + \sigma^2/2)T}{\sigma\sqrt{T}}$
# and $d_2 = d_1 - \sigma\sqrt{T}$.

# %%
BLACK_SCHOLES_SCRIPT = """
d1 = (ln(S / K) + (r + sigma * sigma / 2.0) * T) / (sigma * sqrt(T));
d2 = d1 - sigma * sqrt(T);
CallPV = S * normalCdf(d1) - K * exp(-r * T) * normalCdf(d2);
PutPV  = K * exp(-r * T) * normalCdf(-d2) - S * normalCdf(-d1);
"""

parser = ore.ScriptParser(BLACK_SCHOLES_SCRIPT)
assert parser.success(), f"Parse error: {parser.error()}"
print("Black-Scholes script parsed successfully.")
print(f"\nAST:\n{parser.ast().toString()}")

# %% [markdown]
# ## 2. Pricing a European Call Option
#
# Set up market parameters and execute the script to get a PV.

# %%
# Market parameters
spot = 100.0        # Current underlying price
strike = 100.0      # Strike price (ATM)
rate = 0.05         # Risk-free rate (5%)
vol = 0.20          # Implied volatility (20%)
maturity = 1.0      # Time to expiry (1 year)

# Build the execution context
ctx = ore.Context()
ctx.resetSize(1)
# Input variables
ctx.setScalar("S", spot)
ctx.setScalar("K", strike)
ctx.setScalar("r", rate)
ctx.setScalar("sigma", vol)
ctx.setScalar("T", maturity)
# Output variables (must be pre-declared)
ctx.setScalar("d1", 0.0)
ctx.setScalar("d2", 0.0)
ctx.setScalar("CallPV", 0.0)
ctx.setScalar("PutPV", 0.0)

# Execute the pricing script
engine = ore.ScriptEngine(parser.ast(), ctx)
engine.run(BLACK_SCHOLES_SCRIPT)

# Extract results
call_pv = ctx.getScalar("CallPV")
put_pv = ctx.getScalar("PutPV")
d1 = ctx.getScalar("d1")
d2 = ctx.getScalar("d2")

print("=" * 50)
print("  European Option Pricing (Black-Scholes)")
print("=" * 50)
print(f"  Spot (S)      = {spot:.2f}")
print(f"  Strike (K)    = {strike:.2f}")
print(f"  Rate (r)      = {rate:.2%}")
print(f"  Vol (sigma)   = {vol:.2%}")
print(f"  Maturity (T)  = {maturity:.2f} years")
print("-" * 50)
print(f"  d1            = {d1:.6f}")
print(f"  d2            = {d2:.6f}")
print(f"  Call PV       = {call_pv:.6f}")
print(f"  Put PV        = {put_pv:.6f}")
print("=" * 50)

# %% [markdown]
# ## 3. Verification Against Python Black-Scholes
#
# Verify the scripted result matches a direct Python computation.

# %%
from math import log, exp, sqrt
from statistics import NormalDist

N = NormalDist().cdf  # Standard normal CDF

# Python Black-Scholes
d1_py = (log(spot / strike) + (rate + vol**2 / 2) * maturity) / (vol * sqrt(maturity))
d2_py = d1_py - vol * sqrt(maturity)
call_py = spot * N(d1_py) - strike * exp(-rate * maturity) * N(d2_py)
put_py = strike * exp(-rate * maturity) * N(-d2_py) - spot * N(-d1_py)

print("Verification:")
print(f"  Script Call PV = {call_pv:.10f}")
print(f"  Python Call PV = {call_py:.10f}")
print(f"  Difference     = {abs(call_pv - call_py):.2e}")
print()
print(f"  Script Put PV  = {put_pv:.10f}")
print(f"  Python Put PV  = {put_py:.10f}")
print(f"  Difference     = {abs(put_pv - put_py):.2e}")
print()

# Put-Call parity check: C - P = S - K*exp(-rT)
parity_lhs = call_pv - put_pv
parity_rhs = spot - strike * exp(-rate * maturity)
print(f"  Put-Call Parity: C - P = {parity_lhs:.6f}")
print(f"                  S - Ke^(-rT) = {parity_rhs:.6f}")
print(f"                  Error = {abs(parity_lhs - parity_rhs):.2e}")

assert abs(call_pv - call_py) < 1e-10, "Call PV mismatch!"
assert abs(put_pv - put_py) < 1e-10, "Put PV mismatch!"
print("\n[OK] All results verified to machine precision")

# %% [markdown]
# ## 4. Scenario Analysis — Spot Ladder
#
# Re-use the parsed AST to sweep spot levels and compute a PV profile.

# %%
spot_levels = [70, 75, 80, 85, 90, 95, 100, 105, 110, 115, 120, 125, 130]

print(f"{'Spot':>6} | {'Call PV':>10} | {'Put PV':>10} | {'Intrinsic':>10}")
print(f"{'-'*6}-+-{'-'*10}-+-{'-'*10}-+-{'-'*10}")

for s in spot_levels:
    ctx = ore.Context()
    ctx.resetSize(1)
    ctx.setScalar("S", float(s))
    ctx.setScalar("K", 100.0)
    ctx.setScalar("r", 0.05)
    ctx.setScalar("sigma", 0.20)
    ctx.setScalar("T", 1.0)
    # Pre-declare output variables
    ctx.setScalar("d1", 0.0)
    ctx.setScalar("d2", 0.0)
    ctx.setScalar("CallPV", 0.0)
    ctx.setScalar("PutPV", 0.0)

    engine = ore.ScriptEngine(parser.ast(), ctx)
    engine.run(BLACK_SCHOLES_SCRIPT)

    call_pv = ctx.getScalar("CallPV")
    put_pv = ctx.getScalar("PutPV")
    intrinsic = max(s - 100.0, 0.0)
    print(f"{s:6.1f} | {call_pv:10.4f} | {put_pv:10.4f} | {intrinsic:10.4f}")

# %% [markdown]
# ## 5. Greeks via Finite Differences
#
# Compute Delta and Gamma by bumping spot and re-running the script.

# %%
def compute_greeks(S, K, r, sigma, T, bump=0.01):
    """Compute Delta, Gamma, Vega, Theta via finite differences."""

    def price(spot, vol, time):
        ctx = ore.Context()
        ctx.resetSize(1)
        ctx.setScalar("S", spot)
        ctx.setScalar("K", K)
        ctx.setScalar("r", r)
        ctx.setScalar("sigma", vol)
        ctx.setScalar("T", time)
        # Pre-declare output variables
        ctx.setScalar("d1", 0.0)
        ctx.setScalar("d2", 0.0)
        ctx.setScalar("CallPV", 0.0)
        ctx.setScalar("PutPV", 0.0)
        eng = ore.ScriptEngine(parser.ast(), ctx)
        eng.run(BLACK_SCHOLES_SCRIPT)
        return ctx.getScalar("CallPV")

    pv = price(S, sigma, T)
    pv_up = price(S * (1 + bump), sigma, T)
    pv_down = price(S * (1 - bump), sigma, T)
    ds = S * bump

    delta = (pv_up - pv_down) / (2 * ds)
    gamma = (pv_up - 2 * pv + pv_down) / (ds ** 2)

    # Vega (1% vol bump)
    pv_vol_up = price(S, sigma + 0.01, T)
    vega = pv_vol_up - pv

    # Theta (1-day decay)
    dt = 1.0 / 365.0
    if T > dt:
        pv_theta = price(S, sigma, T - dt)
        theta = pv_theta - pv
    else:
        theta = 0.0

    return {"PV": pv, "Delta": delta, "Gamma": gamma, "Vega": vega, "Theta": theta}


greeks = compute_greeks(S=100.0, K=100.0, r=0.05, sigma=0.20, T=1.0)
print("=" * 50)
print("  European Call Greeks (ATM, 1Y)")
print("=" * 50)
for name, value in greeks.items():
    print(f"  {name:8s} = {value:12.6f}")
print("=" * 50)

# Verify Delta is approximately N(d1) for ATM option
d1_atm = (log(100.0/100.0) + (0.05 + 0.04/2)*1.0) / (0.20 * sqrt(1.0))
expected_delta = N(d1_atm)
print(f"\n  Analytical Delta N(d1) = {expected_delta:.6f}")
print(f"  FD Delta               = {greeks['Delta']:.6f}")
print(f"  Error                  = {abs(greeks['Delta'] - expected_delta):.2e}")

# %% [markdown]
# ## 6. Conditional Logic — Digital Option
#
# IF/THEN/ELSE blocks enable digital (binary) payoffs.

# %%
digital_script = """
IF S > K THEN
    Payoff = Notional;
ELSE
    Payoff = 0.0;
END;
"""

parser_dig = ore.ScriptParser(digital_script)
assert parser_dig.success()

ctx = ore.Context()
ctx.resetSize(1)
ctx.setScalar("S", 105.0)
ctx.setScalar("K", 100.0)
ctx.setScalar("Notional", 1000000.0)
# Pre-declare output variable
ctx.setScalar("Payoff", 0.0)

engine = ore.ScriptEngine(parser_dig.ast(), ctx)
engine.run(digital_script)

print(f"Digital Option (S=105, K=100): Payoff = {ctx.getScalar('Payoff'):,.0f}")

ctx2 = ore.Context()
ctx2.resetSize(1)
ctx2.setScalar("S", 95.0)
ctx2.setScalar("K", 100.0)
ctx2.setScalar("Notional", 1000000.0)
# Pre-declare output variable
ctx2.setScalar("Payoff", 0.0)

engine2 = ore.ScriptEngine(parser_dig.ast(), ctx2)
engine2.run(digital_script)

print(f"Digital Option (S=95,  K=100): Payoff = {ctx2.getScalar('Payoff'):,.0f}")

# %% [markdown]
# ## 7. Multi-Leg Structured Product — Collar
#
# Combine call and put to implement a zero-cost collar.

# %%
collar_script = """
longCall  = max(S - Kcall, 0.0);
shortPut  = -max(Kput - S, 0.0);
Payoff    = (longCall + shortPut) * Notional;
"""

parser_collar = ore.ScriptParser(collar_script)
assert parser_collar.success()

print(f"{'Spot':>6} | {'Long Call':>10} | {'Short Put':>10} | {'Collar PV':>12}")
print(f"{'-'*6}-+-{'-'*10}-+-{'-'*10}-+-{'-'*12}")

for s in [80, 85, 90, 95, 100, 105, 110, 115, 120]:
    ctx = ore.Context()
    ctx.resetSize(1)
    ctx.setScalar("S", float(s))
    ctx.setScalar("Kcall", 110.0)
    ctx.setScalar("Kput", 90.0)
    ctx.setScalar("Notional", 100.0)
    # Pre-declare output variables
    ctx.setScalar("longCall", 0.0)
    ctx.setScalar("shortPut", 0.0)
    ctx.setScalar("Payoff", 0.0)

    engine = ore.ScriptEngine(parser_collar.ast(), ctx)
    engine.run(collar_script)

    lc = ctx.getScalar("longCall")
    sp = ctx.getScalar("shortPut")
    pv = ctx.getScalar("Payoff")
    print(f"{s:6.1f} | {lc:10.4f} | {sp:10.4f} | {pv:12.2f}")

# %% [markdown]
# ## 8. Loop Constructs — Asian Option (Arithmetic Average)
#
# FOR loops enable accumulation over observation fixings.

# %%
asian_script = """
sum = 0.0;
FOR i IN (1, NumObs, 1) DO
    sum = sum + i;
END;
avgIndex = sum / NumObs;
AsianPayoff = max(avgIndex - K, 0.0);
"""

parser_asian = ore.ScriptParser(asian_script)
assert parser_asian.success(), f"Parse error: {parser_asian.error()}"

ctx = ore.Context()
ctx.resetSize(1)
ctx.setScalar("NumObs", 12.0)
ctx.setScalar("K", 5.0)
# Pre-declare output variables and loop variable
ctx.setScalar("sum", 0.0)
ctx.setScalar("i", 0.0)
ctx.setScalar("avgIndex", 0.0)
ctx.setScalar("AsianPayoff", 0.0)

engine = ore.ScriptEngine(parser_asian.ast(), ctx)
engine.run(asian_script)

print(f"Asian Option (average of 1..12 fixings):")
print(f"  Average index   = {ctx.getScalar('avgIndex'):.4f}")
print(f"  Strike          = 5.0")
print(f"  Asian Payoff    = {ctx.getScalar('AsianPayoff'):.4f}")

# %% [markdown]
# ## 9. Shared Market Objects — Native QL vs. Scripted Engine
#
# A European call can be represented two ways within the ORE/QL ecosystem:
#
# 1. **Native QL instrument** — `VanillaOption` with `AnalyticEuropeanEngine`,
#    wired to a `BlackScholesProcess` built from QL market term structures.
# 2. **Scripted trade** — the same Black-Scholes payoff as a script, executed
#    by `ScriptEngine` with inputs extracted from the **same** QL market objects.
#
# Both approaches share one source of market truth. Updating a `SimpleQuote`
# automatically propagates through QuantLib's observer graph to the native
# engine; the scripted engine reads the updated values on the next evaluation.

# %%
# Anchor the global evaluation date so term structures resolve correctly.
today  = ore.Date(27, ore.May, 2026)
ore.Settings.instance().evaluationDate = today
expiry = ore.Date(27, ore.May, 2027)  # 1Y maturity

day_count_ql = ore.Actual365Fixed()
calendar_ql  = ore.NullCalendar()
strike_ql    = 100.0

# Shared market objects — single source of truth for both pricers.
spot_quote = ore.SimpleQuote(100.0)
rf_flat    = ore.FlatForward(today, 0.05, day_count_ql)
vol_flat   = ore.BlackConstantVol(today, calendar_ql, 0.20, day_count_ql)

# 1. Native QL: VanillaOption + BlackScholesProcess + AnalyticEuropeanEngine.
bs_proc = ore.BlackScholesProcess(
    ore.QuoteHandle(spot_quote),
    ore.YieldTermStructureHandle(rf_flat),
    ore.BlackVolTermStructureHandle(vol_flat),
)
option_ql = ore.VanillaOption(
    ore.PlainVanillaPayoff(ore.Option.Call, strike_ql),
    ore.EuropeanExercise(expiry),
)
option_ql.setPricingEngine(ore.AnalyticEuropeanEngine(bs_proc))
ql_npv = option_ql.NPV()

# 2. Scripted engine — pull market values from the same QL objects.
T_ql   = day_count_ql.yearFraction(today, expiry)
S_ql   = spot_quote.value()
r_ql   = -math.log(rf_flat.discount(expiry)) / T_ql
sig_ql = vol_flat.blackVol(expiry, strike_ql)

ctx_ql = ore.Context()
ctx_ql.resetSize(1)
ctx_ql.setScalar("S",      S_ql)
ctx_ql.setScalar("K",      strike_ql)
ctx_ql.setScalar("r",      r_ql)
ctx_ql.setScalar("sigma",  sig_ql)
ctx_ql.setScalar("T",      T_ql)
ctx_ql.setScalar("d1",     0.0)
ctx_ql.setScalar("d2",     0.0)
ctx_ql.setScalar("CallPV", 0.0)
ctx_ql.setScalar("PutPV",  0.0)
eng_ql = ore.ScriptEngine(parser.ast(), ctx_ql)
eng_ql.run(BLACK_SCHOLES_SCRIPT)
scripted_npv = ctx_ql.getScalar("CallPV")

print("=" * 60)
print("  Shared Market Objects: Native QL vs. Scripted Engine")
print("=" * 60)
print(f"  Native QL Call NPV   = {ql_npv:.6f}")
print(f"  Scripted  Call NPV   = {scripted_npv:.6f}")
assert abs(ql_npv - scripted_npv) < 1e-4, "NPV mismatch!"
print("[OK] Both representations produce matching NPVs")

# 3. Spot bump — observe both pricers re-pricing from the same quote.
spot_quote.setValue(110.0)          # QL observer graph propagates automatically
ql_bumped = option_ql.NPV()         # native engine re-prices via lazy evaluation

ctx_b = ore.Context()
ctx_b.resetSize(1)
ctx_b.setScalar("S",      spot_quote.value())
ctx_b.setScalar("K",      strike_ql)
ctx_b.setScalar("r",      r_ql)
ctx_b.setScalar("sigma",  sig_ql)
ctx_b.setScalar("T",      T_ql)
ctx_b.setScalar("d1",     0.0)
ctx_b.setScalar("d2",     0.0)
ctx_b.setScalar("CallPV", 0.0)
ctx_b.setScalar("PutPV",  0.0)
eng_b = ore.ScriptEngine(parser.ast(), ctx_b)
eng_b.run(BLACK_SCHOLES_SCRIPT)
scripted_bumped = ctx_b.getScalar("CallPV")

print()
print(f"  Spot bump 100 -> 110 (via SimpleQuote):")
print(f"  Native QL Call NPV   = {ql_bumped:.6f}")
print(f"  Scripted  Call NPV   = {scripted_bumped:.6f}")
assert abs(ql_bumped - scripted_bumped) < 1e-4, "NPV mismatch after spot bump!"
print("[OK] Both pricers re-price correctly after spot bump")

spot_quote.setValue(100.0)

# %% [markdown]
# ## 10. Payoff Script vs. Close Price
#
# A European call can be described at two levels within the scripting framework:
#
# 1. **Payoff script** — `Payoff = max(S - K, 0.0)` — evaluates the terminal payoff
#    at a given spot level. This is *what* the option pays, not *what it is worth today*.
# 2. **Pricing script** (the Black-Scholes formula in sections 1–3) — evaluates the
#    *risk-neutral expected payoff*, discounted to today. This is the close price.
#
# The difference between the two is the **time value**: the market charges a premium
# over intrinsic value because the underlying may move favourably before expiry.

# %%
PAYOFF_SCRIPT = "Payoff = max(S - K, 0.0);"
parser_payoff = ore.ScriptParser(PAYOFF_SCRIPT)
assert parser_payoff.success(), f"Parse error: {parser_payoff.error()}"

print("=" * 60)
print("  Payoff Script vs. Close Price (time value decomposition)")
print("=" * 60)
print(f"  European Call: K=100, r=5%, vol=20%, T=1Y")
print(f"  {'Spot':>6} | {'Payoff':>10} | {'Close PV':>10} | {'Time Val':>10}")
print(f"  {'-'*6}-+-{'-'*10}-+-{'-'*10}-+-{'-'*10}")

for s_val in [80, 90, 100, 105, 110, 120, 130]:
    # Terminal payoff at the given spot (deterministic — no discounting)
    ctx_p = ore.Context()
    ctx_p.resetSize(1)
    ctx_p.setScalar("S",      float(s_val))
    ctx_p.setScalar("K",      100.0)
    ctx_p.setScalar("Payoff", 0.0)
    eng_p = ore.ScriptEngine(parser_payoff.ast(), ctx_p)
    eng_p.run(PAYOFF_SCRIPT)
    payoff_val = ctx_p.getScalar("Payoff")

    # Close price from the Black-Scholes pricing script (same formula as sections 1–3)
    ctx_c = ore.Context()
    ctx_c.resetSize(1)
    ctx_c.setScalar("S",      float(s_val))
    ctx_c.setScalar("K",      100.0)
    ctx_c.setScalar("r",      0.05)
    ctx_c.setScalar("sigma",  0.20)
    ctx_c.setScalar("T",      1.0)
    ctx_c.setScalar("d1",     0.0)
    ctx_c.setScalar("d2",     0.0)
    ctx_c.setScalar("CallPV", 0.0)
    ctx_c.setScalar("PutPV",  0.0)
    eng_c = ore.ScriptEngine(parser.ast(), ctx_c)
    eng_c.run(BLACK_SCHOLES_SCRIPT)
    close_pv = ctx_c.getScalar("CallPV")

    time_val = close_pv - payoff_val
    print(f"  {s_val:6.1f} | {payoff_val:10.4f} | {close_pv:10.4f} | {time_val:10.4f}")

# %% [markdown]
# ## 11. Full Pricing Workflow Summary
#
# The scripting engine enables a complete pricing workflow from Python:
#
# ```
# ┌─────────────────────────────────────────────────┐
# │  1. Define payoff in ORE scripting language      │
# │  2. Parse script → AST (validates syntax)       │
# │  3. Create Context with market parameters       │
# │  4. Execute with ScriptEngine                   │
# │  5. Read PV / Greeks from context               │
# │  6. Repeat for scenario analysis                │
# └─────────────────────────────────────────────────┘
# ```
#
# | Component | Purpose |
# |-----------|---------|
# | `ScriptParser` | Parse script text → AST |
# | `ASTNode` | Inspect/print the syntax tree |
# | `Context` | Store variables (Numbers, Events, Currencies, etc.) |
# | `ScriptEngine` | Execute AST with a context |
# | `PayLog` | Record cashflows from PAY() calls |
# | `DummyModel` | Placeholder model for testing |
#
# Built-in math: `ln`, `exp`, `sqrt`, `normalCdf`, `normalPdf`,
# `max`, `min`, `pow`, `abs`
#
# For full Monte-Carlo pricing with market-calibrated models, combine
# these primitives with `TodaysMarket` and `EngineFactory`.
