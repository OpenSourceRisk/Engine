#!/usr/bin/env python

import csv
import json
import math
import os
import sys
from datetime import datetime
sys.path.append('../../../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+-----------------------------------------------+")
print("| Time Weighting: ATM Option Expiring Today     |")
print("+-----------------------------------------------+")

with open("Input/intraday_weights.json") as f:
    model = json.load(f)

OUT = "Output"
NAME, CCY = model["underlying"], model["currency"]
CUT = model["cut"]
T0 = datetime.fromisoformat(model["keyed_at"])
T = datetime.fromisoformat(CUT)
DAY = CUT[:10]
TRADE, CONTROL = "SAMEDAY_CALL_CUT", "NEXTWEEK_CALL_CUT"


def hours(a, b):
    return (b - a).total_seconds() / 3600.0


def weight(t):
    # Weight of an instant in variance time, falling linearly from the keyed time to the cut
    s = hours(T0, t) / hours(T0, T)
    return model["weight_at_keyed"] + (model["weight_at_cut"] - model["weight_at_keyed"]) * s


def weighted_vol(t):
    # Keep the variance keyed at T0 and remove it as weighted time passes. With a linear weight the average
    # weight over [t, T] is the mean of its end points, and the vol for the remaining clock time is the keyed
    # vol scaled by the square root of the remaining average weight relative to the average over the day.
    avg_remaining = 0.5 * (weight(t) + weight(T))
    avg_day = 0.5 * (weight(T0) + weight(T))
    return model["keyed_vol"] * math.sqrt(avg_remaining / avg_day)


def vol_quote(stamp, expiry, vol):
    return f"{stamp} EQUITY_OPTION/RATE_LNVOL/{NAME}/{CCY}/{expiry}/ATM/AtmFwd {vol:.10f}\n"


def write_snapshot(stamp, path, same_day_vol):
    # The external time weighting process publishes a market data snapshot per asof: the shared end of day market
    # data restamped with the asof, the time-weighted vol for today's cut, and the keyed vols beyond today.
    prefix = f"EQUITY_OPTION/RATE_LNVOL/{NAME}/"
    with open("../../../Input/market_20160205.txt") as src, open(path, "w") as dst:
        for line in src:
            if not line.startswith("20160205 "):
                continue
            rest = line[len("20160205 "):]
            if not rest.startswith(prefix):
                dst.write(f"{stamp} {rest}")
        dst.write(vol_quote(stamp, CUT, same_day_vol))
        for pillar, vol in model["keyed_pillars"].items():
            dst.write(vol_quote(stamp, pillar, vol))


def run(scenario, time, same_day_vol):
    stamp = f"{DAY}T{time}"
    tag = time.replace(":", "")
    out = f"{OUT}/{scenario}/T{tag}"
    os.makedirs(out, exist_ok=True)
    market = f"{out}/market.txt"
    write_snapshot(stamp, market, same_day_vol)
    with open("Input/ore.xml") as f:
        xml = f.read()
    xml = (xml.replace(">ASOF<", f">{stamp}<")
              .replace(">OUTPUT<", f">{out}<")
              .replace(">MARKET<", f">../{market}<"))
    path = f"{out}/ore.xml"
    with open(path, "w") as f:
        f.write(xml)
    oreex.print_headline(f"Run ORE, {scenario} vol, asof {stamp}")
    oreex.run(path)
    return out


def results(out):
    with open(f"{out}/npv.csv") as f:
        npv = {r[0]: float(r[4]) for r in csv.reader(f) if not r[0].startswith("#")}
    with open(f"{out}/additional_results.csv") as f:
        engine_vol = next(float(r[3]) for r in csv.reader(f) if r[0] == TRADE and r[1] == "volatility")
    gamma = None
    with open(f"{out}/sensitivity.csv") as f:
        for r in csv.DictReader(f):
            if r["#TradeId"] == TRADE and r["Factor_1"].startswith(f"EquitySpot/{NAME}"):
                h = float(r["ShiftSize_1"])
                gamma = float(r["Gamma(Trade)"]) / (h * h)
    return npv, engine_vol, gamma


os.makedirs(OUT, exist_ok=True)
times = model["asof_times"]
with open(f"{OUT}/intraday_vols.csv", "w", newline="") as f:
    w = csv.writer(f)
    w.writerow(["#Time", "Vol"])
    for time in times:
        w.writerow([f"{DAY}T{time}", f"{weighted_vol(datetime.fromisoformat(f'{DAY}T{time}')):.8f}"])

runs = []
for time in times:
    vol = weighted_vol(datetime.fromisoformat(f"{DAY}T{time}"))
    runs.append((time, vol, run("TimeWeighted", time, vol), run("Flat", time, model["keyed_vol"])))

if not oreex.dry:
    rows = []
    for time, vol, weighted, flat in runs:
        npv_d, vol_d, gamma_d = results(weighted)
        npv_f, _, gamma_f = results(flat)
        rows.append({"AsOf": f"{DAY}T{time}",
                     "MinutesToCut": round(hours(datetime.fromisoformat(f"{DAY}T{time}"), T) * 60),
                     "WeightedVol": vol, "EngineVol": vol_d,
                     "NPV_Weighted": npv_d[TRADE], "NPV_Flat": npv_f[TRADE],
                     "Gamma_Weighted": gamma_d, "Gamma_Flat": gamma_f,
                     "Control_NPV_Weighted": npv_d[CONTROL], "Control_NPV_Flat": npv_f[CONTROL]})
    with open(f"{OUT}/time_weighted_vs_flat.csv", "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["#" + k if i == 0 else k for i, k in enumerate(rows[0])])
        for r in rows:
            w.writerow([v if isinstance(v, (str, int)) else f"{v:.8f}" for v in r.values()])

    oreex.print_headline(f"ATM call expiring at the {CUT} cut, time-weighted and flat vol ({OUT}/time_weighted_vs_flat.csv)")
    print(f"{'AsOf':>16} {'Min':>4} {'Vol':>7} {'NPV dec':>9} {'NPV flat':>9} {'Gam dec':>8} {'Gam flat':>8} "
          f"{'Ctl dec':>9} {'Ctl flat':>9}")
    for r in rows:
        print(f"{r['AsOf']:>16} {r['MinutesToCut']:>4} {r['WeightedVol']:7.4f} {r['NPV_Weighted']:9.5f} "
              f"{r['NPV_Flat']:9.5f} {r['Gamma_Weighted']:8.5f} {r['Gamma_Flat']:8.5f} "
              f"{r['Control_NPV_Weighted']:9.4f} {r['Control_NPV_Flat']:9.4f}")
