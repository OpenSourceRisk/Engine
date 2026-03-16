# ---
# jupyter:
#   jupytext:
#     formats: py:percent
#     text_representation:
#       extension: .py
#       format_name: percent
#       format_version: '1.3'
#       jupytext_version: 1.4.2
#   kernelspec:
#     display_name: Python 3
#     language: python
#     name: python3
# ---

# %% [markdown]
# # Interest-rate swaps
#
# Copyright (&copy;) 2004, 2005, 2006, 2007 StatPro Italia srl
#
# This file is part of QuantLib, a free-software/open-source library
# for financial quantitative analysts and developers - https://www.quantlib.org/
#
# QuantLib is free software: you can redistribute it and/or modify it under the
# terms of the QuantLib license.  You should have received a copy of the
# license along with this program; if not, please email
# <quantlib-dev@lists.sf.net>. The license is also available online at
# <https://www.quantlib.org/license.shtml>.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the license for more details.

# %%
import ORE as ql

# %% [markdown]
# ### Global data

# %%
calendar = ql.TARGET()
todaysDate = ql.Date(6, ql.November, 2001)
ql.Settings.instance().evaluationDate = todaysDate
settlementDate = ql.Date(8, ql.November, 2001)

# %% [markdown]
# ### Market quotes

# %%
deposits = {
    (3, ql.Months): 0.0363,
}

# %%
FRAs = {(3, 6): 0.037125, (6, 9): 0.037125, (9, 12): 0.037125}

# %%
futures = {
    ql.Date(19, 12, 2001): 96.2875,
    ql.Date(20, 3, 2002): 96.7875,
    ql.Date(19, 6, 2002): 96.9875,
    ql.Date(18, 9, 2002): 96.6875,
    ql.Date(18, 12, 2002): 96.4875,
    ql.Date(19, 3, 2003): 96.3875,
    ql.Date(18, 6, 2003): 96.2875,
    ql.Date(17, 9, 2003): 96.0875,
}

# %%
swaps = {
    (2, ql.Years): 0.037125,
    (3, ql.Years): 0.0398,
    (5, ql.Years): 0.0443,
    (10, ql.Years): 0.05165,
    (15, ql.Years): 0.055175,
}

# %% [markdown]
# We'll convert them to `Quote` objects...

# %%
for n, unit in deposits.keys():
    deposits[(n, unit)] = ql.SimpleQuote(deposits[(n, unit)])
for n, m in FRAs.keys():
    FRAs[(n, m)] = ql.SimpleQuote(FRAs[(n, m)])
for d in futures.keys():
    futures[d] = ql.SimpleQuote(futures[d])
for n, unit in swaps.keys():
    swaps[(n, unit)] = ql.SimpleQuote(swaps[(n, unit)])

# %% [markdown]
# ...and build rate helpers.

# %%
dayCounter = ql.Actual360()
settlementDays = 2
depositHelpers = [
    ql.DepositRateHelper(
        ql.QuoteHandle(deposits[(n, unit)]),
        ql.Period(n, unit),
        settlementDays,
        calendar,
        ql.ModifiedFollowing,
        False,
        dayCounter,
    )
    for n, unit in deposits.keys()
]

# %%
dayCounter = ql.Actual360()
settlementDays = 2
fraHelpers = [
    ql.FraRateHelper(
        ql.QuoteHandle(FRAs[(n, m)]), n, m, settlementDays, calendar, ql.ModifiedFollowing, False, dayCounter
    )
    for n, m in FRAs.keys()
]

# %%
dayCounter = ql.Actual360()
months = 3
futuresHelpers = [
    ql.FuturesRateHelper(
        ql.QuoteHandle(futures[d]),
        d,
        months,
        calendar,
        ql.ModifiedFollowing,
        True,
        dayCounter,
        ql.QuoteHandle(ql.SimpleQuote(0.0)),
    )
    for d in futures.keys()
]

# %% [markdown]
# The discount curve for the swaps will come from elsewhere. A real application would use some kind of risk-free curve; here we're using a flat one for convenience.

# %%
discountTermStructure = ql.YieldTermStructureHandle(
    ql.FlatForward(settlementDate, 0.04, ql.Actual360()))

# %%
settlementDays = 2
fixedLegFrequency = ql.Annual
fixedLegTenor = ql.Period(1, ql.Years)
fixedLegAdjustment = ql.Unadjusted
fixedLegDayCounter = ql.Thirty360(ql.Thirty360.BondBasis)
floatingLegFrequency = ql.Quarterly
floatingLegTenor = ql.Period(3, ql.Months)
floatingLegAdjustment = ql.ModifiedFollowing
swapHelpers = [
    ql.SwapRateHelper(
        ql.QuoteHandle(swaps[(n, unit)]),
        ql.Period(n, unit),
        calendar,
        fixedLegFrequency,
        fixedLegAdjustment,
        fixedLegDayCounter,
        ql.Euribor3M(),
        ql.QuoteHandle(),
        ql.Period("0D"),
        discountTermStructure,
    )
    for n, unit in swaps.keys()
]

# %% [markdown]
# ### Term structure construction

# %%
forecastTermStructure = ql.RelinkableYieldTermStructureHandle()

# %%
helpers = depositHelpers + futuresHelpers + swapHelpers[1:]
depoFuturesSwapCurve = ql.PiecewiseFlatForward(settlementDate, helpers, ql.Actual360())

# %%
helpers = depositHelpers + fraHelpers + swapHelpers
depoFraSwapCurve = ql.PiecewiseFlatForward(settlementDate, helpers, ql.Actual360())

# %% [markdown]
# ### Swap pricing

# %%
swapEngine = ql.DiscountingSwapEngine(discountTermStructure)

# %%
nominal = 1000000
length = 5
maturity = calendar.advance(settlementDate, length, ql.Years)
payFixed = True

# %%
fixedLegFrequency = ql.Annual
fixedLegAdjustment = ql.Unadjusted
fixedLegDayCounter = ql.Thirty360(ql.Thirty360.BondBasis)
fixedRate = 0.04

# %%
floatingLegFrequency = ql.Quarterly
spread = 0.0
fixingDays = 2
index = ql.Euribor3M(forecastTermStructure)
floatingLegAdjustment = ql.ModifiedFollowing
floatingLegDayCounter = index.dayCounter()

# %%
fixedSchedule = ql.Schedule(
    settlementDate,
    maturity,
    fixedLegTenor,
    calendar,
    fixedLegAdjustment,
    fixedLegAdjustment,
    ql.DateGeneration.Forward,
    False,
)
floatingSchedule = ql.Schedule(
    settlementDate,
    maturity,
    floatingLegTenor,
    calendar,
    floatingLegAdjustment,
    floatingLegAdjustment,
    ql.DateGeneration.Forward,
    False,
)

# %% [markdown]
# We'll build a 5-years swap starting spot...

# %%
spot = ql.VanillaSwap(
    ql.Swap.Payer,
    nominal,
    fixedSchedule,
    fixedRate,
    fixedLegDayCounter,
    floatingSchedule,
    index,
    spread,
    floatingLegDayCounter,
)
spot.setPricingEngine(swapEngine)

# %% [markdown]
# ...and one starting 1 year forward.

# %%
forwardStart = calendar.advance(settlementDate, 1, ql.Years)
forwardEnd = calendar.advance(forwardStart, length, ql.Years)
fixedSchedule = ql.Schedule(
    forwardStart,
    forwardEnd,
    fixedLegTenor,
    calendar,
    fixedLegAdjustment,
    fixedLegAdjustment,
    ql.DateGeneration.Forward,
    False,
)
floatingSchedule = ql.Schedule(
    forwardStart,
    forwardEnd,
    floatingLegTenor,
    calendar,
    floatingLegAdjustment,
    floatingLegAdjustment,
    ql.DateGeneration.Forward,
    False,
)

# %%
forward = ql.VanillaSwap(
    ql.Swap.Payer,
    nominal,
    fixedSchedule,
    fixedRate,
    fixedLegDayCounter,
    floatingSchedule,
    index,
    spread,
    floatingLegDayCounter,
)
forward.setPricingEngine(swapEngine)

# %% [markdown]
# We'll price them both on the bootstrapped curves.
#
# This is the quoted 5-years market rate; we expect the fair rate of the spot swap to match it.


# %%
print(swaps[(5, ql.Years)].value())


# %%
def show(swap):
    print("NPV         = %.2f" % swap.NPV())
    #print("fixedRate   = %.6f" % swap.fixedRate())
    #print("spread      = %.6f" % swap.spread())
    #print("fixedLegBPS = %.6f" % swap.fixedLegBPS())
    #print("floatLegBPS = %.6f" % swap.floatingLegBPS())
    print("Fair spread = %.4f %%" % (swap.fairSpread()*100))
    print("Fair rate   =  %.4f %%" % (swap.fairRate()*100))


# %% [markdown]
# These are the results for the 5-years spot swap on the deposit/futures/swap curve...

# %%
forecastTermStructure.linkTo(depoFuturesSwapCurve)
show(spot)

# %% [markdown]
# ...and these are on the deposit/fra/swap curve.

# %%
forecastTermStructure.linkTo(depoFraSwapCurve)
show(spot)

# %% [markdown]
# The same goes for the 1-year forward swap, except for the fair rate not matching the spot rate.

# %%
forecastTermStructure.linkTo(depoFuturesSwapCurve)
show(forward)

# %%
forecastTermStructure.linkTo(depoFraSwapCurve)
show(forward)

# %% [markdown]
# Modifying the 5-years swap rate and repricing will change the results:

# %%
swaps[(5, ql.Years)].setValue(0.046)

# %%
forecastTermStructure.linkTo(depoFuturesSwapCurve)

# %%
show(spot)

# %%
show(forward)

# %%
forecastTermStructure.linkTo(depoFraSwapCurve)

# %%
show(spot)

# %%
show(forward)
