#!/usr/bin/env python
"""Draw the figures of the Intraday Options document from the examples' expected output.

Usage, from this folder: python make_doc_figures.py [output folder]
The default output folder is ../../Docs/IntradayOptions/pics.
"""

import csv
import os
import sys
from datetime import datetime

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join("..", "..", "Docs", "IntradayOptions", "pics")

# Validated categorical palette (light mode), assigned in fixed order
BLUE, ORANGE, AQUA = "#2a78d6", "#eb6834", "#1baf7a"
INK, MUTED, GRID = "#2b2b2b", "#6b6b66", "#e4e3df"

plt.rcParams.update({
    "font.family": "serif", "font.size": 9, "axes.edgecolor": MUTED, "axes.labelcolor": INK,
    "xtick.color": MUTED, "ytick.color": MUTED, "axes.grid": True, "grid.color": GRID, "grid.linewidth": 0.6,
    "axes.spines.top": False, "axes.spines.right": False, "lines.linewidth": 2, "legend.frameon": False,
    "figure.dpi": 150, "savefig.bbox": "tight",
})


def read(path):
    with open(path) as f:
        return [{k.lstrip("#"): v for k, v in r.items()} for r in csv.DictReader(f)]


def hours(stamp):
    t = datetime.fromisoformat(stamp)
    return t.hour + t.minute / 60.0


def time_axis(ax, ticks):
    ax.set_xticks(ticks)
    ax.set_xticklabels([f"{int(h):02d}:{int(round((h % 1) * 60)):02d}" for h in ticks])
    ax.set_xlabel("Asof, UTC")


def label_end(ax, x, y, text, color, dy=0.0):
    ax.annotate(text, (x, y), xytext=(6, dy), textcoords="offset points", color=INK, va="center", fontsize=8)
    ax.plot([x], [y], "o", color=color, markersize=4)


def atm_gamma():
    rows = read("FlatIntraday/AtmGreeksToExpiry/ExpectedOutput/atm_greeks.csv")
    x = [hours(r["AsOf"]) for r in rows]
    fig, ax = plt.subplots(figsize=(5.6, 3.0))
    ax.plot(x, [float(r["Gamma_BS"]) for r in rows], color=BLUE, label="Closed form")
    ax.plot(x, [float(r["Gamma"]) for r in rows], "o", color=ORANGE, markersize=5, markeredgecolor="white",
            markeredgewidth=1, label="ORE, 1bp shift")
    ax.axvline(16, color=MUTED, linewidth=1, linestyle=":")
    ax.annotate("Cut 16:00", (16, ax.get_ylim()[1] * 0.92), xytext=(-4, 0), textcoords="offset points",
                ha="right", color=MUTED, fontsize=8)
    time_axis(ax, [9, 10, 11, 12, 13, 14, 15, 16])
    ax.set_ylabel("Gamma per index point")
    ax.set_title("ATM call expiring at 16:00: gamma through the day", loc="left", fontsize=9, color=INK)
    ax.legend(loc="upper left")
    fig.savefig(os.path.join(OUT, "atm_gamma.pdf"))
    plt.close(fig)


def time_weighted():
    rows = read("TimeWeighted/TimeWeightedVersusFlat/ExpectedOutput/time_weighted_vs_flat.csv")
    keyed = float(rows[0]["WeightedVol"])
    x = [hours(r["AsOf"]) for r in rows]
    panels = [
        ("Volatility for today's cut", [100 * float(r["WeightedVol"]) for r in rows], [100 * keyed] * len(rows), "%"),
        ("NPV of today's ATM call", [float(r["NPV_Weighted"]) for r in rows], [float(r["NPV_Flat"]) for r in rows], ""),
        ("Gamma of today's ATM call", [float(r["Gamma_Weighted"]) for r in rows],
         [float(r["Gamma_Flat"]) for r in rows], ""),
    ]
    fig, axes = plt.subplots(1, 3, figsize=(7.2, 2.6), sharex=True)
    for ax, (title, dec, flat, unit) in zip(axes, panels):
        ax.plot(x, flat, color=BLUE, linestyle="--", label="Flat, keyed vol")
        ax.plot(x, dec, color=ORANGE, label="Time-weighted")
        ax.set_title(title, loc="left", fontsize=9, color=INK)
        time_axis(ax, [14.5, 17, 19, 21])
        if unit:
            ax.yaxis.set_major_formatter(matplotlib.ticker.FormatStrFormatter("%.0f%%"))
    axes[0].legend(loc="lower left")
    fig.tight_layout()
    fig.savefig(os.path.join(OUT, "time_weighted_vs_flat.pdf"))
    plt.close(fig)


def bump_error():
    rows = [r for r in read("FlatIntraday/SameDayInstruments/ExpectedOutput/same_day_instruments.csv")
            if r["Status"] == "Live" and r["Gamma_RelDiff"] not in ("", None) and r["ShiftOverStdDev"]]
    kinds = [("Vanilla and American", ("FX_CALL", "GOLD_CALL", "AMERICAN", "EUROPEAN"), BLUE, "o"),
             ("Digital", ("DIGITAL",), ORANGE, "s"),
             ("Barrier and touch", ("_UO_", "TOUCH"), AQUA, "^")]
    fig, ax = plt.subplots(figsize=(5.6, 3.0))
    for name, keys, color, marker in kinds:
        pts = [(float(r["ShiftOverStdDev"]), abs(float(r["Gamma_RelDiff"]))) for r in rows
               if any(k in r["TradeId"] for k in keys) and float(r["Gamma_RelDiff"]) != 0.0]
        if pts:
            ax.plot(*zip(*pts), marker, color=color, markersize=5, markeredgecolor="white", markeredgewidth=0.8,
                    label=name)
    guide = [0.01, 0.4]
    ax.plot(guide, [0.1 * g * g for g in guide], ":", color=MUTED, linewidth=1.2)
    ax.annotate("0.1 x (shift / sd)$^2$", (0.05, 0.1 * 0.05 ** 2), xytext=(4, -12), textcoords="offset points",
                ha="left", color=MUTED, fontsize=8)
    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xlabel("Spot shift / standard deviation of the move to the cut")
    ax.set_ylabel("Gamma vs exact closed form, relative")
    ax.set_title("Finite difference gamma error against shift size",
                 loc="left", fontsize=9, color=INK)
    ax.legend(loc="upper left")
    fig.savefig(os.path.join(OUT, "bump_error.pdf"))
    plt.close(fig)


def multi_cuts_fx():
    rows = [r for r in read("TimeWeighted/MultipleCutsFx/ExpectedOutput/published_vols.csv") if r["Stale"] == "False"]
    cuts = [("2016-02-05T06:00Z", "06:00 Tokyo", BLUE), ("2016-02-05T13:30Z", "13:30 ECB", ORANGE),
            ("2016-02-05T15:00Z", "15:00 New York", AQUA)]
    fig, ax = plt.subplots(figsize=(5.6, 3.0))
    for pillar, name, color in cuts:
        pts = [(hours(r["AsOf"]), 100 * float(r["Vol"])) for r in rows if r["Pillar"] == pillar]
        cut_h = hours(pillar[:16])
        ax.plot(*zip(*pts), "o-", color=color, markersize=4, label=f"Cut {name}")
        ax.axvline(cut_h, color=color, linewidth=0.8, linestyle=":")
    time_axis(ax, [3, 6, 9, 12, 13.5, 15])
    ax.yaxis.set_major_formatter(matplotlib.ticker.FormatStrFormatter("%.0f%%"))
    ax.set_ylabel("Published ATM vol")
    ax.set_title("EURUSD: one time-weighted vol series per cut", loc="left", fontsize=9, color=INK)
    ax.legend(loc="lower left")
    fig.savefig(os.path.join(OUT, "multi_cuts_fx.pdf"))
    plt.close(fig)


def _profile_vol(weights, keyed_vol, asof_min, cut_min):
    starts = [h * 60 + m for h, m in (map(int, s.split(":")) for s, _ in weights)]
    ends = starts[1:] + [24 * 60]
    raw = [w for _, w in weights]
    mean = sum(w * (e - s) for w, s, e in zip(raw, starts, ends)) / (24 * 60)
    seg = [(s, e, w / mean) for s, e, w in zip(starts, ends, raw)]
    wt = sum(w * max(0, min(cut_min, e) - max(asof_min, s)) for s, e, w in seg)
    return keyed_vol * (wt / (cut_min - asof_min)) ** 0.5


def vol_profiles():
    # Same model as TimeWeighted/multiple_cuts.py, SP5 cut at 21:00 UTC, keyed vol 16%
    front = [["00:00", 2.0], ["10:00", 1.5], ["14:00", 1.2], ["16:00", 0.9], ["20:30", 0.5]]
    busy_close = [["00:00", 0.15], ["14:30", 3.0], ["15:00", 1.0], ["20:30", 2.0], ["21:00", 0.15]]
    cut = 21 * 60
    t = list(range(3 * 60, cut - 1, 5))
    fig, ax = plt.subplots(figsize=(5.6, 3.0))
    for weights, name, color, style in [(front, "Busiest early, quiet before the cut", BLUE, "-"),
                                        (busy_close, "Quiet night, busy open and close", ORANGE, "--")]:
        ax.plot([m / 60 for m in t], [100 * _profile_vol(weights, 0.16, m, cut) for m in t], style, color=color,
                label=name)
    time_axis(ax, [3, 6, 9, 12, 15, 18, 21])
    ax.yaxis.set_major_formatter(matplotlib.ticker.FormatStrFormatter("%.0f%%"))
    ax.set_ylabel("Vol for the rest of the day")
    ax.set_title("Same keyed vol, two intraday weight profiles, cut at 21:00", loc="left", fontsize=9, color=INK)
    ax.legend(loc="upper left")
    fig.savefig(os.path.join(OUT, "vol_profiles.pdf"))
    plt.close(fig)


os.makedirs(OUT, exist_ok=True)
atm_gamma()
time_weighted()
bump_error()
multi_cuts_fx()
vol_profiles()
print("figures written to", OUT)
