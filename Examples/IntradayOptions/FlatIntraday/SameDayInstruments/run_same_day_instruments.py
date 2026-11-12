#!/usr/bin/env python

import copy
import csv
import math
import os
import sys
sys.path.append('../../../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+----------------------------------------------+")
print("| Same Day Expiry: Instruments up to their Cut |")
print("+----------------------------------------------+")

ASOF_DAY = "2016-02-05"
TIMES = ["09:00", "12:00", "14:00", "14:30", "14:55", "15:00", "17:00", "20:00", "20:55"]
OUT = "Output"
SUMMARY = f"{OUT}/same_day_instruments.csv"
# The NPV report's MaturityTime is Actual/Actual (ISDA), 2016 has 366 days. The engines use Actual/365 Fixed.
MINUTES_PER_YEAR_REPORT = 366 * 24 * 60
MINUTES_PER_YEAR_ENGINE = 365 * 24 * 60


# The EURUSD vol published for today's New York cut, a pillar at the FX trades' expiry. The example does not
# time-weight vols, so the pillar keeps today's level, the 1M vol the FX trades were priced off before cut pillars.
FX_CUT_VOL = f"FX_OPTION/RATE_LNVOL/EUR/USD/{ASOF_DAY}T15:00Z/ATM 0.121699"
CURVE_CONFIG = f"{OUT}/curveconfig.xml"


def restamp_market(stamp, path):
    # Quotes are looked up by the exact asof datetime, so restamp the shared end of day market file. Every snapshot
    # carries the cut pillar, after the cut ORE ignores it as it is not after the asof.
    with open("../../../Input/market_20160205.txt") as src, open(path, "w") as dst:
        for line in src:
            dst.write(stamp + line[len("20160205"):] if line.startswith("20160205 ") else line)
        dst.write(f"{stamp} {FX_CUT_VOL}\n")


def write_curve_config():
    # The shared curve configuration with today's New York cut added to the EURUSD vol expiries
    with open("../../../Input/curveconfig.xml") as f:
        xml = f.read()
    eurusd = xml.index("<CurveId>EURUSD</CurveId>")
    start = xml.index("<Expiries>", eurusd) + len("<Expiries>")
    expiries = xml[start:xml.index("</Expiries>", start)]
    xml = xml[:start] + f"{ASOF_DAY}T15:00Z," + expiries.strip() + xml[start + len(expiries):]
    os.makedirs(OUT, exist_ok=True)
    with open(CURVE_CONFIG, "w") as f:
        f.write(xml)


def run_at(time):
    tag = time.replace(":", "")
    stamp = f"{ASOF_DAY}T{time}"
    market = f"market_20160205T{tag}.txt"
    os.makedirs(f"{OUT}/T{tag}", exist_ok=True)
    restamp_market(stamp, os.path.join("Output", market))
    with open("Input/ore.xml") as f:
        xml = f.read()
    xml = (xml.replace(f"{ASOF_DAY}T15:00", stamp)
              .replace("market_20160205T1500.txt", market)
              .replace(f'"outputPath">{OUT}<', f'"outputPath">{OUT}/T{tag}<'))
    path = f"{OUT}/T{tag}/ore.xml"
    with open(path, "w") as f:
        f.write(xml)
    oreex.print_headline(f"Run ORE with asof {stamp}")
    oreex.run(path)
    return tag


def minutes(time):
    h, m = time.split(":")
    return int(h) * 60 + int(m)


def norm_cdf(x):
    return 0.5 * (1.0 + math.erf(x / math.sqrt(2.0)))


def norm_pdf(x):
    return math.exp(-0.5 * x * x) / math.sqrt(2.0 * math.pi)


class Inputs:
    """The engine's market inputs from the additional results, and the time to the cut."""

    def __init__(self, res, t):
        self.spot = float(res["spot"])
        self.vol = float(res["volatility"])
        self.df = float(res["riskFreeDiscount"])
        self.div_df = float(res["dividendDiscount"])
        # The barrier engine reports no forward
        self.forward = float(res["forward"]) if "forward" in res else self.spot * self.div_df / self.df
        self.strike = float(res["strike"])
        self.mult = float(res.get("instMultiplier", 1.0))
        self.t = t
        self.sd = self.vol * math.sqrt(t)


def vanilla(m, phi, per_forward=False):
    # Black on the forward. Greeks are to the spot, or to the forward when per_forward is set (the commodity
    # sensitivity shifts the whole price curve, so it moves the forward).
    d1 = math.log(m.forward / m.strike) / m.sd + 0.5 * m.sd
    d2 = d1 - m.sd
    npv = m.df * phi * (m.forward * norm_cdf(phi * d1) - m.strike * norm_cdf(phi * d2))
    if per_forward:
        delta, gamma = m.df * phi * norm_cdf(phi * d1), m.df * norm_pdf(d1) / (m.forward * m.sd)
    else:
        delta, gamma = m.div_df * phi * norm_cdf(phi * d1), m.div_df * norm_pdf(d1) / (m.spot * m.sd)
    return {"NPV": m.mult * npv, "Delta": m.mult * delta, "Gamma": m.mult * gamma}


def cash_or_nothing_call(m, amount):
    d2 = math.log(m.forward / m.strike) / m.sd - 0.5 * m.sd
    scale = amount * m.df
    return {"NPV": scale * norm_cdf(d2),
            "Delta": scale * norm_pdf(d2) / (m.spot * m.sd),
            "Gamma": -scale * norm_pdf(d2) / (m.spot * m.spot * m.sd) * (d2 / m.sd + 1.0)}


def up_and_out_call(spot, m, barrier):
    # Reiner-Rubinstein (Haug, A - B + C - D) for K < H, continuous monitoring, no rebate
    if spot >= barrier:
        return 0.0
    r = -math.log(m.df) / m.t
    b = r + math.log(m.div_df) / m.t
    mu = (b - 0.5 * m.vol ** 2) / m.vol ** 2
    sd, k, h = m.sd, m.strike, barrier
    carry = math.exp((b - r) * m.t)
    disc = math.exp(-r * m.t)

    def term(x, power_s, power_k, eta):
        return (spot * carry * power_s * norm_cdf(eta * x) - k * disc * power_k * norm_cdf(eta * x - eta * sd))

    x1 = math.log(spot / k) / sd + (1 + mu) * sd
    x2 = math.log(spot / h) / sd + (1 + mu) * sd
    y1 = math.log(h * h / (spot * k)) / sd + (1 + mu) * sd
    y2 = math.log(h / spot) / sd + (1 + mu) * sd
    hs = h / spot
    a = term(x1, 1.0, 1.0, 1.0)
    b_ = term(x2, 1.0, 1.0, 1.0)
    c = term(y1, hs ** (2 * (mu + 1)), hs ** (2 * mu), -1.0)
    d = term(y2, hs ** (2 * (mu + 1)), hs ** (2 * mu), -1.0)
    return a - b_ + c - d


def one_touch_up(spot, m, barrier):
    # Pays at expiry if the spot reaches the barrier before the cut: discounted probability that the maximum
    # of a Brownian motion with drift b - vol^2/2 reaches log(H/S)
    if spot >= barrier:
        return m.df
    nu = (math.log(m.div_df) - math.log(m.df)) / m.t - 0.5 * m.vol ** 2
    a = math.log(barrier / spot)
    p = (norm_cdf((-a + nu * m.t) / m.sd)
         + math.exp(2.0 * nu * a / m.vol ** 2) * norm_cdf((-a - nu * m.t) / m.sd))
    return m.df * p


def moved(m, x, on_forward):
    # The inputs with the underlying moved to x, where x is the spot or, for on_forward, the forward
    c = copy.copy(m)
    scale = x / (m.forward if on_forward else m.spot)
    c.spot, c.forward = m.spot * scale, m.forward * scale
    return c


def numeric_greeks(price, x):
    # Central differences with a step far below the remaining standard deviation
    h = x * 1e-7
    up, mid, down = price(x + h), price(x), price(x - h)
    return {"NPV": mid, "Delta": (up - down) / (2 * h), "Gamma": (up - 2 * mid + down) / (h * h)}


# Each check returns the closed form NPV and greeks, the closed form price as a function of the shifted
# underlying, the name of the closed form and a note.

def check_fx_call(m, res, npvs):
    return vanilla(m, 1.0), lambda x: vanilla(moved(m, x, False), 1.0)["NPV"], "Garman-Kohlhagen", ""


def check_gold_call(m, res, npvs):
    price = lambda x: vanilla(moved(m, x, True), 1.0, per_forward=True)["NPV"]
    return vanilla(m, 1.0, per_forward=True), price, "Black-76, greeks to the forward", ""


def check_digital(m, res, npvs):
    amount = float(res["payoffAmount"])
    price = lambda x: cash_or_nothing_call(moved(m, x, False), amount)["NPV"]
    return cash_or_nothing_call(m, amount), price, "Cash-or-nothing", ""


def check_up_and_out(m, res, npvs):
    barrier = float(res["barrier"])
    price = lambda x: m.mult * up_and_out_call(x, m, barrier)
    vanilla_npv = vanilla(m, 1.0)["NPV"]
    # The NPV report rounds to 6 decimals, and 5 minutes before the cut the barrier is so far away that the
    # knock-out equals the vanilla
    ok = 0.0 <= npvs["EQ_UO_CALL_2100"] <= vanilla_npv + 5e-7
    note = f"{'' if ok else 'BOUNDS FAILED: '}0 <= NPV <= vanilla call {vanilla_npv:.6f}"
    return numeric_greeks(price, m.spot), price, "Reiner-Rubinstein up-and-out call", note


def check_one_touch(m, res, npvs):
    barrier = float(res["strike"])
    amount = float(res["payoffAmount"])
    price = lambda x: amount * one_touch_up(x, m, barrier)
    ok = 0.0 <= npvs["FX_ONE_TOUCH_1500"] <= amount * m.df
    note = f"{'' if ok else 'BOUNDS FAILED: '}0 <= NPV <= payoff x DF {amount * m.df:.6f}"
    return numeric_greeks(price, m.spot), price, "Up barrier hit probability, paid at expiry", note


def check_european_put(m, res, npvs):
    price = lambda x: vanilla(moved(m, x, False), -1.0)["NPV"]
    return vanilla(m, -1.0), price, "Black-Scholes European put", ""


def check_american_put(m, res, npvs):
    cf, price, reference, _ = check_european_put(m, res, npvs)
    premium = npvs["EQ_AMERICAN_PUT_2100"] - npvs["EQ_EUROPEAN_PUT_2100"]
    return cf, price, reference, f"American - European {premium:.6f}"


TRADES = {
    "FX_CALL_1500": {"cut": "15:00", "factor": "FXSpot/EURUSD", "check": check_fx_call},
    "GOLD_CALL_1500": {"cut": "15:00", "factor": "CommodityCurve/COMDTY_GOLD_USD", "check": check_gold_call},
    "EQ_DIGITAL_CALL_2100": {"cut": "21:00", "factor": "EquitySpot/SP5", "check": check_digital},
    "FX_DIGITAL_CALL_1500": {"cut": "15:00", "factor": "FXSpot/EURUSD", "check": check_digital},
    "EQ_UO_CALL_2100": {"cut": "21:00", "factor": "EquitySpot/SP5", "check": check_up_and_out},
    "FX_ONE_TOUCH_1500": {"cut": "15:00", "factor": "FXSpot/EURUSD", "check": check_one_touch},
    "EQ_AMERICAN_PUT_2100": {"cut": "21:00", "factor": "EquitySpot/SP5", "check": check_american_put},
    "EQ_EUROPEAN_PUT_2100": {"cut": "21:00", "factor": "EquitySpot/SP5", "check": check_european_put},
}


def read_reports(d):
    with open(f"{d}/npv.csv") as f:
        npv_rows = {r["#TradeId"]: r for r in csv.DictReader(f)}
    res = {}
    with open(f"{d}/additional_results.csv") as f:
        for r in csv.reader(f):
            res.setdefault(r[0], {})[r[1]] = r[3]
    sens = {}
    with open(f"{d}/sensitivity.csv") as f:
        for r in csv.DictReader(f):
            sens.setdefault(r["#TradeId"], []).append(r)
    return npv_rows, res, sens


def rel_diff(a, b):
    return None if a is None or b is None or b == 0 else a / b - 1.0


def summarise(tag, time):
    npv_rows, res, sens = read_reports(f"{OUT}/T{tag}")
    npvs = {k: float(r["NPV"]) for k, r in npv_rows.items()}
    rows = []
    for trade, spec in TRADES.items():
        left = minutes(spec["cut"]) - minutes(time)
        row = {"AsOf": f"{ASOF_DAY}T{time}", "TradeId": trade, "CutUTC": spec["cut"], "MinutesToCut": left}
        if trade not in npv_rows:
            # Past the cut the trade is removed as matured, see the "Trade is Matured" warning in log.txt
            row.update({"Status": "Matured, not in npv.csv"})
            rows.append(row)
            continue
        t = left / MINUTES_PER_YEAR_ENGINE
        m = Inputs(res[trade], t)
        cf, price, reference, note = spec["check"](m, res[trade], npvs)
        on_forward = spec["factor"].startswith("Commodity")
        x = m.forward if on_forward else m.spot
        expected_mt = left / MINUTES_PER_YEAR_REPORT
        mt = float(npv_rows[trade]["MaturityTime"])
        bump = next((r for r in sens.get(trade, []) if r["Factor_1"].startswith(spec["factor"])), None)
        # All shifts are 1bp relative. ShiftSize_1 is rounded to 8 decimals, and for gold it is quoted on the
        # 1D price while the parallel shift moves the forward by 1bp of the forward.
        h = x * 1e-4
        if bump is None:
            # Both sensitivities are below the report's outputSensitivityThreshold
            delta = gamma = 0.0
            note += "; no spot row in sensitivity.csv, below threshold"
        else:
            # ORE reports Delta = V(x+h) - V(x) and Gamma = V(x+h) - 2V(x) + V(x-h) for the shift h
            up, second = float(bump["Delta(Trade)"]), float(bump["Gamma(Trade)"])
            delta, gamma = (up - 0.5 * second) / h, second / (h * h)
        # The closed form under the same shift separates the shift's own error from a pricing difference
        p_up, p_mid, p_down = price(x + h), price(x), price(x - h)
        delta_shift, gamma_shift = (p_up - p_down) / (2 * h), (p_up - 2 * p_mid + p_down) / (h * h)
        row.update({
            "Status": "Live",
            "MaturityTime": mt, "MaturityTime_Expected": round(expected_mt, 6),
            "MaturityTimeOK": abs(mt - expected_mt) <= 5e-7,
            "EngineTime": float(res[trade]["timeToExpiry"]),
            "ShiftOverStdDev": h / x / m.sd,
            "NPV": npvs[trade], "NPV_CF": cf["NPV"], "NPV_AbsDiff": npvs[trade] - cf["NPV"],
            "NPV_RelDiff": rel_diff(npvs[trade], cf["NPV"]),
            "Delta": delta, "Delta_CF": cf["Delta"], "Delta_RelDiff": rel_diff(delta, cf["Delta"]),
            "Delta_CF_SameShift": delta_shift, "Delta_SameShiftRelDiff": rel_diff(delta, delta_shift),
            "Gamma": gamma, "Gamma_CF": cf["Gamma"], "Gamma_RelDiff": rel_diff(gamma, cf["Gamma"]),
            "Gamma_CF_SameShift": gamma_shift, "Gamma_SameShiftRelDiff": rel_diff(gamma, gamma_shift),
            "Reference": reference, "Note": note})
        rows.append(row)
    return rows


COLUMNS = ["AsOf", "TradeId", "CutUTC", "MinutesToCut", "Status", "MaturityTime", "MaturityTime_Expected",
           "MaturityTimeOK", "EngineTime", "ShiftOverStdDev", "NPV", "NPV_CF", "NPV_AbsDiff", "NPV_RelDiff",
           "Delta", "Delta_CF", "Delta_RelDiff", "Delta_CF_SameShift", "Delta_SameShiftRelDiff",
           "Gamma", "Gamma_CF", "Gamma_RelDiff", "Gamma_CF_SameShift", "Gamma_SameShiftRelDiff",
           "Reference", "Note"]


def fmt(v):
    if v is None:
        return ""
    if isinstance(v, (str, int, bool)):
        return str(v)
    return f"{v:.8g}"


write_curve_config()
tags = [(run_at(time), time) for time in TIMES]

if not oreex.dry:
    rows = [r for tag, time in tags for r in summarise(tag, time)]
    with open(SUMMARY, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["#" + COLUMNS[0]] + COLUMNS[1:])
        for r in rows:
            w.writerow([fmt(r.get(c)) for c in COLUMNS])

    oreex.print_headline(f"Same day instruments against closed forms ({SUMMARY})")
    diffs = ["NPV_RelDiff", "Delta_RelDiff", "Delta_SameShiftRelDiff", "Gamma_RelDiff", "Gamma_SameShiftRelDiff"]
    print(f"{'AsOf':>16} {'Trade':>20} {'Min':>4} {'NPV':>10} {'dNPV':>9} {'Delta':>11} {'dDelta':>9} {'dDeltaS':>9} "
          f"{'Gamma':>12} {'dGamma':>9} {'dGammaS':>9}  Note")
    for r in rows:
        if r["Status"] != "Live":
            print(f"{r['AsOf']:>16} {r['TradeId']:>20} {r['MinutesToCut']:>4}  {r['Status']}")
            continue
        d = {k: "" if r[k] is None else f"{r[k]:.2e}" for k in diffs}
        print(f"{r['AsOf']:>16} {r['TradeId']:>20} {r['MinutesToCut']:>4} {r['NPV']:10.5f} {d['NPV_RelDiff']:>9} "
              f"{r['Delta']:11.5f} {d['Delta_RelDiff']:>9} {d['Delta_SameShiftRelDiff']:>9} "
              f"{r['Gamma']:12.5f} {d['Gamma_RelDiff']:>9} {d['Gamma_SameShiftRelDiff']:>9}  {r['Note']}")
