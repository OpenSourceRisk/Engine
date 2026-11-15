#!/usr/bin/env python

import csv
import math
import os
import sys
sys.path.append('../../../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+-----------------------------------------------+")
print("| Same Day Expiry: Intraday NPV and Sensitivity |")
print("+-----------------------------------------------+")

ASOF_DAY = "2016-02-05"
EXPIRY_MINUTE = 16 * 60
TIMES = ["09:00", "12:00", "14:00", "15:00", "15:30", "15:45", "15:55", "15:59"]
ATM_TRADE = "SAMEDAY_CALL_1600"
OUT = "Output"


def restamp_market(stamp, path):
    # Quotes are looked up by the exact asof datetime, so restamp the shared end of day market file
    with open("../../../Input/market_20160205.txt") as src, open(path, "w") as dst:
        for line in src:
            dst.write(stamp + line[len("20160205"):] if line.startswith("20160205 ") else line)


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
    oreex.print_headline(f"Run ORE with asof {stamp}, {EXPIRY_MINUTE - minutes(time)} minutes before the 16:00 expiry")
    oreex.run(path)
    return tag


def minutes(time):
    h, m = time.split(":")
    return int(h) * 60 + int(m)


def norm_cdf(x):
    return 0.5 * (1.0 + math.erf(x / math.sqrt(2.0)))


def norm_pdf(x):
    return math.exp(-0.5 * x * x) / math.sqrt(2.0 * math.pi)


def black_scholes_call(spot, strike, forward, vol, t, df, div_df):
    sd = vol * math.sqrt(t)
    d1 = math.log(forward / strike) / sd + 0.5 * sd
    d2 = d1 - sd
    return {"NPV": df * (forward * norm_cdf(d1) - strike * norm_cdf(d2)),
            "Delta": div_df * norm_cdf(d1),
            "Gamma": div_df * norm_pdf(d1) / (spot * sd),
            "Vega": spot * div_df * norm_pdf(d1) * math.sqrt(t)}


def summarise(tag, time):
    d = f"{OUT}/T{tag}"
    with open(f"{d}/additional_results.csv") as f:
        res = {r[1]: r[3] for r in csv.reader(f) if r[0] == ATM_TRADE}
    npv = None
    with open(f"{d}/npv.csv") as f:
        for r in csv.reader(f):
            if r[0] == ATM_TRADE:
                npv = float(r[4])
    spot_bump = vol_bump = None
    with open(f"{d}/sensitivity.csv") as f:
        rows = csv.DictReader(f)
        for r in rows:
            if r["#TradeId"] != ATM_TRADE:
                continue
            if r["Factor_1"].startswith("EquitySpot/SP5"):
                spot_bump = r
            elif r["Factor_1"].startswith("EquityVolatility/SP5/0/1D"):
                vol_bump = r
    # Actual/365 Fixed: the time to expiry printed in the additional results is rounded to 6 decimals
    t = (EXPIRY_MINUTE - minutes(time)) / (365.0 * 24 * 60)
    spot = float(res["spot"])
    bs = black_scholes_call(spot, float(res["strike"]), float(res["forward"]), float(res["volatility"]), t,
                            float(res["riskFreeDiscount"]), float(res["dividendDiscount"]))
    # ORE reports Delta = V(S+h) - V(S) and Gamma = V(S+h) - 2V(S) + V(S-h) for the shift h
    h = float(spot_bump["ShiftSize_1"])
    up, second = float(spot_bump["Delta(Trade)"]), float(spot_bump["Gamma(Trade)"])
    vega = float(vol_bump["Delta(Trade)"]) / float(vol_bump["ShiftSize_1"]) if vol_bump else 0.0
    return {"AsOf": f"{ASOF_DAY}T{time}", "MinutesToExpiry": EXPIRY_MINUTE - minutes(time),
            "NPV": npv, "NPV_BS": bs["NPV"],
            "Delta": (up - 0.5 * second) / h, "Delta_BS": bs["Delta"],
            "Gamma": second / (h * h), "Gamma_BS": bs["Gamma"],
            "Vega": vega, "Vega_BS": bs["Vega"]}


tags = [(run_at(time), time) for time in TIMES]

if not oreex.dry:
    rows = [summarise(tag, time) for tag, time in tags]
    with open(f"{OUT}/atm_greeks.csv", "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["#" + k if i == 0 else k for i, k in enumerate(rows[0])])
        for r in rows:
            w.writerow([v if isinstance(v, (str, int)) else f"{v:.8f}" for v in r.values()])

    oreex.print_headline(f"ATM call evolution towards the 16:00 expiry ({OUT}/atm_greeks.csv)")
    print(f"{'AsOf':>17} {'Min':>4} {'NPV':>9} {'NPV_BS':>9} {'Delta':>8} {'Delta_BS':>8} "
          f"{'Gamma':>8} {'Gamma_BS':>8} {'Vega':>7} {'Vega_BS':>7}")
    for r in rows:
        print(f"{r['AsOf']:>17} {r['MinutesToExpiry']:>4} {r['NPV']:9.5f} {r['NPV_BS']:9.5f} {r['Delta']:8.5f} "
              f"{r['Delta_BS']:8.5f} {r['Gamma']:8.5f} {r['Gamma_BS']:8.5f} {r['Vega']:7.3f} {r['Vega_BS']:7.3f}")
