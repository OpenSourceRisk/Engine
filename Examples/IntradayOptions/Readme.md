# Intraday Options

These examples value options that expire on the valuation date, using an ORE build with intraday date
resolution, that is QuantLib configured with `QL_HIGH_RESOLUTION_DATE=ON`:

    cmake ... -DQL_HIGH_RESOLUTION_DATE=ON

In such a build, dates in ORE inputs can carry a time of day in ISO 8601 form, `2016-02-05T15:00`, with
optional seconds and fractional seconds. Times are UTC. A time may carry a UTC offset,
`2016-02-05T16:00-05:00`, which ORE converts to UTC. In a default build these inputs fail with an error
naming `QL_HIGH_RESOLUTION_DATE`, which is why these examples are not part of the default examples test
suite.

A trade's `ExerciseDate` stays a plain date, so the same trade XML loads in both builds and validates
against the unchanged schema. The time of the expiry, the cut, goes in the trade envelope:

    <Envelope>
      ...
      <AdditionalFields>
        <ExpiryCutTime>21:00Z</ExpiryCutTime>
      </AdditionalFields>
    </Envelope>

The cut has the form `HH:MM[:SS[.ffffff]]` with an optional `Z`, `+HH:MM` or `-HH:MM`, and no designator
means UTC. The intraday build applies it to every exercise date of the trade, and the default build ignores
it. Without a cut the intraday build moves an exercise date on the valuation day to 23:59:59 UTC, so the
option is still live during that day, and logs a warning. Exercise dates on other days stay at midnight, as
in the default build. Every trade in these examples that expires on the valuation day sets its cut.

The Intraday Options document (`Docs/intradayoptions.pdf`) describes the methodology of each example in
full.


## Inventory

The examples are grouped by how the volatility for today's expiry behaves through the day: flat in
`FlatIntraday`, and set by intraday time weighting in `TimeWeighted`.

| Example | Demonstrates | Main output |
|---------|--------------|-------------|
| `FlatIntraday/AtmGreeksToExpiry` | An ATM option valued through its last day, with greeks checked against the closed form | `Output/atm_greeks.csv` |
| `TimeWeighted/TimeWeightedVersusFlat` | Pricing from an external series of time-weighted intraday vols, compared with a flat vol | `Output/time_weighted_vs_flat.csv`, `Output/intraday_vols.csv` |
| `FlatIntraday/SameDayInstruments` | FX, commodity, digital, barrier, touch and American options up to their cut, checked against closed forms | `Output/same_day_instruments.csv` |
| `TimeWeighted/MultipleCutsFx` | Three EURUSD cuts on one day, each priced off its own time-weighted vol pillar | `Output/multiple_cuts_fx.csv`, `Output/published_vols.csv` |
| `TimeWeighted/MultipleCutsEquity` | Two SP5 cuts on one day, each priced off its own time-weighted vol pillar | `Output/multiple_cuts_equity.csv`, `Output/published_vols.csv` |
| `TimeWeighted/MultipleCutsCommodity` | Two gold cuts on one day, each priced off its own time-weighted vol pillar | `Output/multiple_cuts_commodity.csv`, `Output/published_vols.csv` |

Each example folder has the same layout:

- `run.py` runs the example and is what the examples test suite calls.
- `run_<name>.py` holds the steps of the example.
- `Input/` holds `ore.xml`, a template the script copies once per asof, and the example's own configuration.
  Shared inputs come from `Examples/Input`.
- `ExpectedOutput/` holds the reference results.

Run an example from its folder with `python run.py`. Point it at the intraday build with the `ORE_EXE`
environment variable if needed.


## FlatIntraday/AtmGreeksToExpiry

**Demonstrates.** With day resolution, an option expiring today has a time to expiry of zero, so it is
treated as expired or priced at intrinsic value with zero gamma. With intraday resolution ORE prices it
with the remaining time, and its greeks behave as they should into the cut.

**What it does.** An ATM SP5 call and put expire at 16:00 UTC. The script values them with NPV,
additional results and sensitivity analytics at asof times from 09:00 to 15:59. For each asof it writes a
market snapshot stamped with that time and runs ORE into `Output/T<hhmm>`. It then writes
`Output/atm_greeks.csv`, with the NPV, delta, gamma and vega of the call from ORE next to the Black-Scholes
closed form, computed from the forward, volatility and discount factors in ORE's additional results. A
two year call with a date only expiry is a control.

**What to look for.**

- Gamma grows like one over the square root of the time left, from 0.026 at 09:00 to 0.535 at 15:59.
- NPV, delta and vega agree with the closed form to five digits or more.
- At 15:59 the finite difference gamma is 0.7% below the closed form, because a one basis point spot
  shift is already a third of the one minute standard deviation.


## TimeWeighted/TimeWeightedVersusFlat

**Demonstrates.** Variance does not arrive evenly through the trading day: it concentrates after the open
and around busy periods. A volatility keyed in the morning for an expiry later the same day covers all the
remaining hours, so as the busy ones pass the volatility that prices the rest of the day falls. Measuring
time in weighted time to capture this is intraday time weighting; trading desks often call the resulting fall
volatility decay. ORE does not model it. A process outside ORE generates the time-weighted volatilities and
publishes them as market data, and ORE prices from them. In this example the script plays that process.

**What it does.**

- The SP5 session runs from 14:30 to 21:00 UTC, and today's cut is the close at 21:00 UTC. A volatility of
  16% is keyed for today's cut at the open. Volatilities beyond today, 12.71% at 1M and 25% at 6M, are keyed
  as usual. The weights are in `Input/intraday_weights.json`.
- The weight of an instant falls linearly from 2.0 at the open to 0.5 at the cut. The volatility for the
  remaining time is the keyed volatility times the square root of the average weight still to come relative
  to the average weight of the session. The script writes the series to `Output/intraday_vols.csv`.
- For each asof the script writes a market snapshot with the time-weighted volatility as a quote whose expiry is
  today's cut, `EQUITY_OPTION/RATE_LNVOL/SP5/USD/2016-02-05T21:00/ATM/AtmFwd`, and the keyed 1M and 6M quotes.
  The SP5 volatility curve in `Input/curveconfig.xml` takes its pillars from the snapshot.
- ORE runs at each asof twice, with the time-weighted volatility and with the keyed volatility held flat.
  `Output/time_weighted_vs_flat.csv` compares an ATM call expiring at today's cut and a control call expiring at
  next week's cut.

**What to look for.**

- The volatility the engine reports equals the time-weighted quote at every asof.
- The NPV of today's option is lower with time weighting, 30% lower at 20:00.
- Gamma is higher with time weighting, by the ratio of the volatilities: 1.14 at 17:00.
- The control option moves by up to 0.34%, because its expiry is interpolated in total variance between
  today's pillar and the 1M pillar.
- The sensitivity analysis does not simulate the SP5 volatilities, because a simulated surface cannot hold
  a pillar at today's cut. Gamma is therefore right, but there is no vega.


## TimeWeighted/MultipleCutsFx, MultipleCutsEquity and MultipleCutsCommodity

**Demonstrates.** A market can have several cuts on one day, and options expiring at different cuts see
different amounts of the day's variance. A time weighting process publishes one time-weighted volatility per cut, each as
a pillar at that cut's datetime. ORE prices an option at a cut off its own pillar, for FX, equity and
commodity volatilities alike, and ignores the pillar of a cut that has passed.

**What it does.**

- One example per asset class, with the cuts on 2016-02-05 in UTC:
  - `MultipleCutsFx`: EURUSD at 06:00 (Tokyo), 13:30 (08:30 New York, US data) and 15:00 (New York).
  - `MultipleCutsEquity`: SP5 at 14:30 (the open, as for AM settled index options) and 21:00 (the close).
  - `MultipleCutsCommodity`: spot gold at 10:30 and 15:00, the LBMA Gold Price auctions.
- The shared driver `TimeWeighted/multiple_cuts.py` plays the time weighting process, with the market data in each
  example's `Input/cuts.json`. Variance accrues at a piecewise constant intraday weight, averaging one over
  the day. The weight is highest early and falls in steps to its lowest before the last cut. The remaining
  variance to a cut is the keyed volatility squared times the weighted time to the cut. The published
  volatility is that variance per clock time, the average weight still to come. As the busy hours pass,
  that average falls, so each cut's volatility falls through the day. Because the weighted time is
  cumulative, the total variance never decreases from one cut to the next.
- For each asof the script writes a market snapshot stamped with the asof. It has the time-weighted volatility of
  each cut, for example `FX_OPTION/RATE_LNVOL/EUR/USD/2016-02-05T13:30Z/ATM`, and keyed tenor volatilities
  beyond today. Once a cut has passed, the snapshot keeps its last published volatility, as a vendor might.
  The curve configurations take the pillars from the snapshot with the wildcard expiry `*`.
  `Output/published_vols.csv` lists the published volatilities.
- The portfolio has an ATM call per cut, with a plain `ExerciseDate` and the cut in
  `Envelope/AdditionalFields/ExpiryCutTime`, and a control call at a cut next week. The base currency is
  USD. There is no sensitivity analysis.
- `Output/multiple_cuts_<asset>.csv` has one row per asof and trade: the volatility published for the
  trade's cut, the volatility the engine reports, the NPV, and Black on the forward, volatility and discount
  factor in the additional results.

**What to look for.**

- The engine volatility equals the quote of the trade's own cut at every asof, to the 10 decimals the
  quotes are written with. At 05:55 the three EURUSD calls price at 13.47%, 12.13% and 11.79%.
- The NPV agrees with Black to the 6 decimals of the NPV report.
- Each cut's volatility falls from one asof to the next up to its cut. The EURUSD 15:00 cut is quoted at
  12.53% at 03:00 and 8.25% at 14:55.
- After a cut, its trade drops out of the NPV report as matured, and its stale pillar is not used. The log
  has `expiry ... is not after the asof, ignored` for an FX pillar. An equity or commodity option quote whose
  expiry is not after the asof is rejected when the market data is loaded, with a warning in the log.
- The control call is interpolated in total variance between today's last pillar and the keyed tenors.
- The FX curve is an ATM curve. FX option quotes and the `Expiries` of an FX volatility configuration take
  a date or, in this build, a datetime wherever they take a tenor, for the ATM, delta, BFRR and absolute
  surfaces. A tenor keeps its usual pillar, the calendar advanced from the asof. The optional
  `EnforceMonotoneVariance` in the configuration fails the build when the ATM total variance decreases from
  one pillar to the next. By default ORE builds the curve and logs a warning naming the two pillars.


## FlatIntraday/SameDayInstruments

**Demonstrates.** The other instruments of a same day options report, each expiring today at the cut of
its market, price with the remaining time and give greeks that agree with their closed forms into the cut.
Past its cut a trade drops out of the report.

**What it does.**

- One trade of each type expires on 2016-02-05. The EURUSD vanilla call, cash-or-nothing digital call and
  one-touch expire at the New York cut, 10:00 New York or 15:00 UTC. The touch barrier is 1.138, 0.5% above
  spot, and pays at expiry. The gold call expires at the LBMA Gold Price PM auction, 15:00 London or 15:00
  UTC. The SP5 cash-or-nothing digital call, up-and-out call with its barrier 1% above spot, and American
  put expire at the close, 16:00 New York or 21:00 UTC. A European put is the American put's twin.
- For each asof from 09:00 to 20:55 the script writes a market snapshot stamped with that time and runs ORE
  into `Output/T<hhmm>` with NPV, additional results and sensitivity analytics. Spot shifts are 1bp
  relative. For gold a single 1D shift tenor moves the whole price curve, so its delta and gamma are to the
  forward. The base currency is USD, so the simulated FX pair is EURUSD and the shift applies to it.
- The three FX trades are priced off a volatility pillar at their own cut, 15:00Z. Every snapshot publishes
  it as `FX_OPTION/RATE_LNVOL/EUR/USD/2016-02-05T15:00Z/ATM`, and the script writes `Output/curveconfig.xml`,
  the shared curve configuration with that cut added to the EURUSD `Expiries`. The example does not time-weight
  volatilities, so the pillar is flat at today's level, the 12.1699% of the 1M pillar. From 15:00 the pillar
  is not after the asof and ORE ignores it.
- The sensitivity analysis does not simulate the FX volatilities, because a simulated surface is resampled
  onto tenors from the asof and loses the pillar at the cut. There is therefore no FX vega.
- `Output/same_day_instruments.csv` has one row per asof and trade. It checks `MaturityTime` against the
  time to the cut, and compares the NPV, delta and gamma from ORE with a closed form computed from the
  forward, volatility and discount factors in the additional results:
  - Garman-Kohlhagen for the FX call, Black-76 for gold, Black-Scholes for the European put.
  - The cash-or-nothing formula for both digitals.
  - Reiner-Rubinstein for the up-and-out call, with the bound 0 <= NPV <= vanilla call.
  - For the one-touch, the discounted probability that the spot reaches the barrier before the cut, with
    the bound 0 <= NPV <= payoff x discount factor.
  - The Black-Scholes European put for the American put, and the American minus European NPV.
- Each greek is compared twice: with the exact derivative of the closed form, and with the closed form
  under the same 1bp shift. The first shows the error of the shift itself, the second any pricing
  difference.

**What to look for.**

- `MaturityTime` matches the time to the cut for every live trade. The NPV report uses Actual/Actual
  (ISDA), a 366 day year in 2016. The engines use Actual/365 Fixed.
- Under the same shift, ORE and the closed forms agree to 2e-4 or better in delta and gamma for every
  trade and asof. NPVs agree to the 6 decimals of the NPV report, except the American put, which is
  compared with the European closed form.
- Against the exact derivative the 1bp shift is coarse in the last minutes. At 14:55 it is 0.27 standard
  deviations of EURUSD, and the FX digital gamma is 3.7% smaller in magnitude than the exact value. Gold, struck at the quoted
  spot and 1.4 standard deviations out of the money at 14:55, shows 5.7% in delta. The equity digital
  gamma is 1.4% off at 20:55.
- The American put is worth at most 3e-5 more than its European twin. The finite difference engine
  needs its own settings for this, see `Input/pricingengine.xml`. With the usual settings a single time
  step overprices the put by 23% at seven hours, and undamped Crank-Nicolson steps make its gamma 60% too
  high at twelve hours.
- At the 15:00 cut and after, ORE removes the FX and gold trades from the NPV and sensitivity reports. The
  log has a `Trade is Matured` warning for each. An in the money cash settled option is therefore not
  reported between the cut and its payment, as noted under out of scope in the document.
- The gold forward for today's cut is about 1155.14, below the spot quote of 1155.593. The curve places the spot
  quote at its T+2 settlement date and extrapolates back to today along the slope to the first futures
  pillar. A day resolution run does the same.
- The touch is worth 117 at 09:00 and 0.12 at 14:00. At 14:55 the barrier is 13 standard deviations away,
  its value is zero, and the sensitivity report leaves it out because the values fall below the output
  threshold.

