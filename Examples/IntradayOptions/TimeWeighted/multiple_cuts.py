"""Shared driver of the MultipleCuts examples: several option cuts on one day, each with its own time-weighted vol pillar.

The script plays the external time weighting process. For every asof it publishes a market data snapshot stamped with the
asof: the shared end of day market, the time-weighted ATM vol for each of today's cuts as a pillar at that cut's datetime,
and the keyed tenor pillars beyond today. ORE then prices one ATM call per cut, plus a control expiring later, and the
summary compares the engine's vol with the quote of the trade's cut and the NPV with Black on the engine's inputs.

Input/cuts.json of the calling example holds the market specific data, see the Readme.
"""

import csv
import json
import math
import os
from datetime import datetime, timedelta

OUT = "Output"
MINUTES_PER_DAY = 24 * 60


def _minutes(hhmm):
    h, m = hhmm.split(":")
    return int(h) * 60 + int(m)


class TimeWeightingProcess:
    """Variance accrues at a piecewise constant intraday weight, normalised to an average of one over the day.

    The remaining variance to a cut T at asof t is keyed_vol^2 * W(t, T) / 365 days, with W the weighted time. The
    quoted vol is that variance per clock time, keyed_vol * sqrt(W(t, T) / (T - t)). W is cumulative, so at every
    asof the total variance is non-decreasing across the cuts.
    """

    def __init__(self, model):
        self.keyed_vol = model["keyed_vol"]
        starts = [_minutes(s) for s, _ in model["weights"]]
        assert starts[0] == 0 and starts == sorted(starts), "weights must start at 00:00 and be ordered"
        ends = starts[1:] + [MINUTES_PER_DAY]
        raw = [w for _, w in model["weights"]]
        mean = sum(w * (e - s) for w, s, e in zip(raw, starts, ends)) / MINUTES_PER_DAY
        self.segments = [(s, e, w / mean) for s, e, w in zip(starts, ends, raw)]

    def weighted_minutes(self, a, b):
        return sum(w * max(0, min(b, e) - max(a, s)) for s, e, w in self.segments)

    def vol(self, asof, cut):
        a, b = _minutes(asof), _minutes(cut)
        assert a < b, f"cut {cut} is not after asof {asof}"
        return self.keyed_vol * math.sqrt(self.weighted_minutes(a, b) / (b - a))


def _norm_cdf(x):
    return 0.5 * (1.0 + math.erf(x / math.sqrt(2.0)))


def black_npv(res):
    """Black on the forward from the engine's additional results, scaled by the instrument multiplier."""
    f, k = float(res["forward"]), float(res["strike"])
    vol, t, df = float(res["volatility"]), float(res["timeToExpiry"]), float(res["riskFreeDiscount"])
    sd = vol * math.sqrt(t)
    d1 = math.log(f / k) / sd + 0.5 * sd
    return float(res.get("instMultiplier", 1.0)) * df * (f * _norm_cdf(d1) - k * _norm_cdf(d1 - sd))


class MultipleCuts:

    def __init__(self, oreex, model_file="Input/cuts.json"):
        self.oreex = oreex
        with open(model_file) as f:
            self.model = json.load(f)
        self.day = self.model["day"]
        self.process = TimeWeightingProcess(self.model)

    def quote(self, stamp, expiry, vol):
        m = self.model
        return f"{stamp} {m['quote_prefix']}{expiry}{m['quote_suffix']} {vol:.10f}\n"

    def published(self, time):
        """The cut vols the time weighting process publishes at an asof. A passed cut keeps the vol published at the last asof
        before it, the vendor does not retire it until the end of the day, and ORE ignores it as its pillar is not
        after the asof."""
        quotes = {}
        for name, cut in self.model["cuts"].items():
            stale = _minutes(time) >= _minutes(cut)
            last = max((t for t in self.model["asof_times"] if _minutes(t) < _minutes(cut)), key=_minutes,
                       default="00:00") if stale else time
            quotes[name] = (cut, self.process.vol(last, cut), stale)
        return quotes

    def write_snapshot(self, time, path):
        stamp = f"{self.day}T{time}"
        prefix = self.model["quote_prefix"]
        with open(self.model["shared_market"]) as src, open(path, "w") as dst:
            for line in src:
                if not line.startswith("20160205 "):
                    continue
                rest = line[len("20160205 "):]
                if not rest.startswith(prefix):
                    dst.write(f"{stamp} {rest}")
            for cut, vol, _ in self.published(time).values():
                dst.write(self.quote(stamp, f"{self.day}T{cut}Z", vol))
            for pillar, vol in self.model["keyed_pillars"].items():
                dst.write(self.quote(stamp, pillar, vol))

    def run_at(self, time):
        stamp = f"{self.day}T{time}"
        out = f"{OUT}/T{time.replace(':', '')}"
        os.makedirs(out, exist_ok=True)
        market = f"{out}/market.txt"
        self.write_snapshot(time, market)
        with open("Input/ore.xml") as f:
            xml = f.read()
        xml = (xml.replace(">ASOF<", f">{stamp}<")
                  .replace(">OUTPUT<", f">{out}<")
                  .replace(">MARKET<", f">../{market}<"))
        path = f"{out}/ore.xml"
        with open(path, "w") as f:
            f.write(xml)
        self.oreex.print_headline(f"Run ORE, asof {stamp}")
        self.oreex.run(path)
        return out

    def summarise(self, time, out):
        with open(f"{out}/npv.csv") as f:
            npvs = {r["#TradeId"]: float(r["NPV"]) for r in csv.DictReader(f)}
        res = {}
        with open(f"{out}/additional_results.csv") as f:
            for r in csv.reader(f):
                res.setdefault(r[0], {})[r[1]] = r[3]
        published = self.published(time)
        rows = []
        for trade, spec in self.model["trades"].items():
            expiry = datetime.fromisoformat(f"{spec['expiry']}T{spec['cut']}")
            left = round((expiry - datetime.fromisoformat(f"{self.day}T{time}")) / timedelta(minutes=1))
            quoted = published[spec["pillar"]][1] if "pillar" in spec else None
            row = {"AsOf": f"{self.day}T{time}", "TradeId": trade, "Expiry": f"{spec['expiry']}T{spec['cut']}Z",
                   "MinutesToExpiry": left, "QuotedVol": quoted}
            if trade not in npvs:
                row["Status"] = "Matured, not in npv.csv"
                rows.append(row)
                continue
            engine_vol = float(res[trade]["volatility"])
            cf = black_npv(res[trade])
            row.update({"Status": "Live", "EngineVol": engine_vol,
                        "VolDiff": None if quoted is None else engine_vol - quoted,
                        "NPV": npvs[trade], "NPV_CF": cf, "NPV_AbsDiff": npvs[trade] - cf})
            rows.append(row)
        return rows

    def run(self, summary_name):
        os.makedirs(OUT, exist_ok=True)
        times = self.model["asof_times"]
        with open(f"{OUT}/published_vols.csv", "w", newline="") as f:
            w = csv.writer(f)
            w.writerow(["#AsOf", "Pillar", "Vol", "Stale"])
            for time in times:
                for cut, vol, stale in self.published(time).values():
                    w.writerow([f"{self.day}T{time}", f"{self.day}T{cut}Z", f"{vol:.10f}", stale])
                for pillar, vol in self.model["keyed_pillars"].items():
                    w.writerow([f"{self.day}T{time}", pillar, f"{vol:.10f}", False])

        outs = [(time, self.run_at(time)) for time in times]
        if self.oreex.dry:
            return

        rows = [r for time, out in outs for r in self.summarise(time, out)]
        columns = ["AsOf", "TradeId", "Expiry", "MinutesToExpiry", "Status", "QuotedVol", "EngineVol", "VolDiff",
                   "NPV", "NPV_CF", "NPV_AbsDiff"]

        def fmt(v):
            if v is None:
                return ""
            return str(v) if isinstance(v, (str, int)) else f"{v:.10g}"

        path = f"{OUT}/{summary_name}"
        with open(path, "w", newline="") as f:
            w = csv.writer(f)
            w.writerow(["#" + columns[0]] + columns[1:])
            for r in rows:
                w.writerow([fmt(r.get(c)) for c in columns])

        self.oreex.print_headline(f"Engine vol against the cut's quote, NPV against Black ({path})")
        print(f"{'AsOf':>16} {'Trade':>24} {'Min':>6} {'Quoted':>9} {'Engine':>9} {'NPV':>11} {'NPV-CF':>9}  Status")
        for r in rows:
            q = "" if r.get("QuotedVol") is None else f"{r['QuotedVol']:.6f}"
            if r["Status"] != "Live":
                print(f"{r['AsOf']:>16} {r['TradeId']:>24} {r['MinutesToExpiry']:>6} {q:>9} {'':>9} {'':>11} {'':>9}"
                      f"  {r['Status']}")
                continue
            print(f"{r['AsOf']:>16} {r['TradeId']:>24} {r['MinutesToExpiry']:>6} {q:>9} {r['EngineVol']:9.6f} "
                  f"{r['NPV']:11.6f} {r['NPV_AbsDiff']:9.1e}  {r['Status']}")
