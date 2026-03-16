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
# # Gaussian 1D models
#
# Copyright (&copy;) 2018 Angus Lee
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

# %% [markdown]
# ### Setup

# %%
import ORE as ql
import pandas as pd

# %%
interactive = "get_ipython" in globals()


def show(x):
    if not interactive:
        print(x)
    return x


# %%
def basket_data(basket):
    data = []
    for helper in basket:
        h = ql.as_swaption_helper(helper)
        data.append(
            (
                h.swaptionExpiryDate().to_date(),
                h.swaptionMaturityDate().to_date(),
                h.swaptionNominal(),
                h.volatility().value(),
                h.swaptionStrike(),
            )
        )
    return pd.DataFrame(data, columns=["Expiry", "Maturity", "Nominal", "Rate", "Market vol"])


# %%
def calibration_data(basket, volatilities):
    data = []
    for helper, sigma in zip(basket, volatilities):
        h = ql.as_swaption_helper(helper)
        modelValue = h.modelValue()
        data.append(
            (
                h.swaptionExpiryDate().to_date(),
                sigma,
                modelValue,
                h.marketValue(),
                h.impliedVolatility(modelValue, 1e-6, 1000, 0.0, 2.0),
                h.volatility().value(),
            )
        )
    return pd.DataFrame(
        data, columns=["Expiry", "Model sigma", "Model price", "Market price", "Model imp.vol", "Market imp.vol"]
    )


# %% [markdown]
# ### Calculations

# %% [markdown]
# This exercise tries to replicate the Quantlib C++ `Gaussian1dModel` example on how to use the GSR and Markov Functional model.

# %%
refDate = ql.Date(30, 4, 2014)
ql.Settings.instance().evaluationDate = refDate

# %% [markdown]
# We assume a multicurve setup, for simplicity with flat yield term structures.
#
# The discounting curve is an Eonia curve at a level of 2% and the forwarding curve is an Euribor 6m curve at a level of 2.5%.
#
# For the volatility we assume a flat swaption volatility at 20%.

# %%
forward6mQuote = ql.QuoteHandle(ql.SimpleQuote(0.025))
oisQuote = ql.QuoteHandle(ql.SimpleQuote(0.02))
volQuote = ql.QuoteHandle(ql.SimpleQuote(0.2))

# %%
dc = ql.Actual365Fixed()
yts6m = ql.FlatForward(refDate, forward6mQuote, dc)
ytsOis = ql.FlatForward(refDate, oisQuote, dc)
yts6m.enableExtrapolation()
ytsOis.enableExtrapolation()
hyts6m = ql.RelinkableYieldTermStructureHandle(yts6m)
t0_curve = ql.YieldTermStructureHandle(yts6m)
t0_Ois = ql.YieldTermStructureHandle(ytsOis)
euribor6m = ql.Euribor6M(hyts6m)
swaptionVol = ql.ConstantSwaptionVolatility(0, ql.TARGET(), ql.ModifiedFollowing, volQuote, ql.Actual365Fixed())

# %%
effectiveDate = ql.TARGET().advance(refDate, ql.Period('2D'))
maturityDate = ql.TARGET().advance(effectiveDate, ql.Period('10Y'))

# %%
fixedSchedule = ql.Schedule(effectiveDate,
                            maturityDate,
                            ql.Period('1Y'),
                            ql.TARGET(),
                            ql.ModifiedFollowing,
                            ql.ModifiedFollowing,
                            ql.DateGeneration.Forward, False)

# %%
floatSchedule = ql.Schedule(effectiveDate,
                            maturityDate,
                            ql.Period('6M'),
                            ql.TARGET(),
                            ql.ModifiedFollowing,
                            ql.ModifiedFollowing,
                            ql.DateGeneration.Forward, False)

# %% [markdown]
# We consider a standard 10-years Bermudan payer swaption with yearly exercises at a strike of 4%.

# %%
fixedNominal    = [1]*(len(fixedSchedule)-1)
floatingNominal = [1]*(len(floatSchedule)-1)
strike          = [0.04]*(len(fixedSchedule)-1)
gearing         = [1]*(len(floatSchedule)-1)
spread          = [0]*(len(floatSchedule)-1)

# %%
underlying = ql.NonstandardSwap(
    ql.Swap.Payer,
    fixedNominal, floatingNominal, fixedSchedule, strike,
    ql.Thirty360(ql.Thirty360.BondBasis), floatSchedule,
    euribor6m, gearing, spread, ql.Actual360(), False, False, ql.ModifiedFollowing)

# %%
exerciseDates = [ql.TARGET().advance(x, -ql.Period('2D')) for x in fixedSchedule]
exerciseDates = exerciseDates[1:-1]
exercise = ql.BermudanExercise(exerciseDates)
swaption = ql.NonstandardSwaption(underlying,exercise,ql.Settlement.Physical)

# %% [markdown]
# The model is a one factor Hull White model with piecewise volatility adapted to our exercise dates.
#
# The reversion is just kept constant at a level of 1%.

# %%
stepDates = exerciseDates[:-1]
sigmas = [ql.QuoteHandle(ql.SimpleQuote(0.01)) for x in range(1, 10)]
reversion = [ql.QuoteHandle(ql.SimpleQuote(0.01))]

# %% [markdown]
# The model's curve is set to the 6m forward curve. Note that the model adapts automatically to other curves where appropriate (e.g. if an index requires a different forwarding curve) or where explicitly specified (e.g. in a swaption pricing engine).

# %%
gsr = ql.Gsr(t0_curve, stepDates, sigmas, reversion)
swaptionEngine = ql.Gaussian1dSwaptionEngine(gsr, 64, 7.0, True, False, t0_Ois)
nonstandardSwaptionEngine = ql.Gaussian1dNonstandardSwaptionEngine(
    gsr, 64, 7.0, True, False, ql.QuoteHandle(ql.SimpleQuote(0)), t0_Ois)

# %%
swaption.setPricingEngine(nonstandardSwaptionEngine)

# %%
swapBase = ql.EuriborSwapIsdaFixA(ql.Period('10Y'), t0_curve, t0_Ois)
basket = swaption.calibrationBasket(swapBase, swaptionVol, 'Naive')

# %%
for basket_i in basket:
    ql.as_black_helper(basket_i).setPricingEngine(swaptionEngine)

# %%
method = ql.LevenbergMarquardt()
ec = ql.EndCriteria(1000, 10, 1e-8, 1e-8, 1e-8)

# %%
gsr.calibrateVolatilitiesIterative(basket, method, ec)


# %% [markdown]
# The engine can generate a calibration basket in two modes.
#
# The first one is called Naive and generates ATM swaptions adapted to the exercise dates of the swaption and its maturity date. The resulting basket looks as follows:

# %%
show(basket_data(basket))

# %% [markdown]
# Let's calibrate our model to this basket. We use a specialized calibration method calibrating the sigma function one by one to the calibrating vanilla swaptions. The result of this is as follows:

# %%
show(calibration_data(basket, gsr.volatility()))

# %% [markdown]
# Bermudan swaption NPV (ATM calibrated GSR):

# %%
print(swaption.NPV())

# %% [markdown]
# There is another mode to generate a calibration basket called `MaturityStrikeByDeltaGamma`. This means that the maturity, the strike and the nominal of the calibrating swaptions are obtained matching the NPV, first derivative and second derivative of the swap you will exercise into at at each bermudan call date. The derivatives are taken with respect to the model's state variable.
#
# Let's try this in our case.

# %%
basket = swaption.calibrationBasket(swapBase, swaptionVol, 'MaturityStrikeByDeltaGamma')
show(basket_data(basket))

# %%
for basket_i in basket:
    ql.as_black_helper(basket_i).setPricingEngine(swaptionEngine)

# %% [markdown]
# The calibrated nominal is close to the exotics nominal. The expiries and maturity dates of the vanillas are the same as in the case above. The difference is the strike which is now equal to the exotics strike.
#
# Let's see how this affects the exotics NPV. The recalibrated model is:

# %%
gsr.calibrateVolatilitiesIterative(basket, method, ec)
show(calibration_data(basket, gsr.volatility()))

# %% [markdown]
# Bermudan swaption NPV (deal strike calibrated GSR):

# %%
print(swaption.NPV())

# %% [markdown]
# We can do more complicated things.  Let's e.g. modify the nominal schedule to be linear amortizing and see what the effect on the generated calibration basket is:

# %%
for i in range(0,len(fixedSchedule)-1):
    tmp = 1 - i/ (len(fixedSchedule)-1)
    fixedNominal[i]        = tmp
    floatingNominal[i*2]   = tmp
    floatingNominal[i*2+1] = tmp

# %%
underlying2 = ql.NonstandardSwap(ql.Swap.Payer,
                            fixedNominal, floatingNominal, fixedSchedule, strike,
                            ql.Thirty360(ql.Thirty360.BondBasis), floatSchedule,
                            euribor6m, gearing, spread, ql.Actual360(), False, False, ql.ModifiedFollowing)

# %%
swaption2 = ql.NonstandardSwaption(underlying2,exercise,ql.Settlement.Physical)

# %%
swaption2.setPricingEngine(nonstandardSwaptionEngine)
basket = swaption2.calibrationBasket(swapBase, swaptionVol, 'MaturityStrikeByDeltaGamma')

# %%
show(basket_data(basket))

# %% [markdown]
# The notional is weighted over the underlying exercised into and the maturity is adjusted downwards. The rate, on the other hand, is not affected.

# %% [markdown]
# You can also price exotic bond's features. If you have e.g. a Bermudan callable fixed bond you can set up the call right as a swaption to enter into a one leg swap with notional reimbursement at maturity. The exercise should then be written as a rebated exercise paying the notional in case of exercise. The calibration basket looks like this:

# %%
fixedNominal2    = [1]*(len(fixedSchedule)-1)
floatingNominal2 = [0]*(len(floatSchedule)-1) #null the second leg

# %%
underlying3 = ql.NonstandardSwap(ql.Swap.Receiver,
                            fixedNominal2, floatingNominal2, fixedSchedule, strike,
                            ql.Thirty360(ql.Thirty360.BondBasis), floatSchedule,
                            euribor6m, gearing, spread, ql.Actual360(), False, True, ql.ModifiedFollowing)

# %%
rebateAmount = [-1]*len(exerciseDates)
exercise2 = ql.RebatedExercise(exercise, rebateAmount, 2, ql.TARGET())
swaption3 = ql.NonstandardSwaption(underlying3,exercise2,ql.Settlement.Physical)

# %%
oas0 = ql.SimpleQuote(0)
oas100 = ql.SimpleQuote(0.01)
oas = ql.RelinkableQuoteHandle(oas0)

# %%
nonstandardSwaptionEngine2 = ql.Gaussian1dNonstandardSwaptionEngine(
    gsr, 64, 7.0, True, False, oas, t0_curve) # Change discounting to 6m

# %%
swaption3.setPricingEngine(nonstandardSwaptionEngine2)
basket = swaption3.calibrationBasket(swapBase, swaptionVol, 'MaturityStrikeByDeltaGamma')

# %%
show(basket_data(basket))

# %% [markdown]
# Note that nominals are not exactly 1.0 here. This is because we do our bond discounting on 6m level while the swaptions are still discounted on OIS level. (You can try this by changing the OIS level to the 6m level, which will produce nominals near 1.0).
#
# The NPV of the call right is (after recalibrating the model):

# %%
for basket_i in basket:
    ql.as_black_helper(basket_i).setPricingEngine(swaptionEngine)

# %%
gsr.calibrateVolatilitiesIterative(basket, method, ec)

# %%
print(swaption3.NPV())

# %% [markdown]
# Up to now, no credit spread is included in the pricing. We can do so by specifying an oas in the pricing engine. Let's set the spread level to 100bp and regenerate the calibration basket.

# %%
oas.linkTo(oas100)
basket = swaption3.calibrationBasket(swapBase, swaptionVol, 'MaturityStrikeByDeltaGamma')
show(basket_data(basket))

# %% [markdown]
# The adjusted basket takes the credit spread into account. This is consistent to a hedge where you would have a margin on the float leg around 100bp,too.

# %%
for basket_i in basket:
    ql.as_black_helper(basket_i).setPricingEngine(swaptionEngine)

# %%
gsr.calibrateVolatilitiesIterative(basket, method, ec)

# %%
print(swaption3.NPV())

# %% [markdown]
# The next instrument we look at is a CMS 10Y vs Euribor 6M swaption. The maturity is again 10 years and the option is exercisable on a yearly basis.

# %%
CMSNominal     = [1]*(len(fixedSchedule)-1)
CMSgearing     = [1]*(len(fixedSchedule)-1)
CMSspread      = [0]*(len(fixedSchedule)-1)
EuriborNominal = [1]*(len(floatSchedule)-1)
Euriborgearing = [1]*(len(floatSchedule)-1)
Euriborspread  = [0.001]*(len(floatSchedule)-1)
underlying4 = ql.FloatFloatSwap(ql.Swap.Payer,
                                CMSNominal, EuriborNominal,
                                fixedSchedule, swapBase, ql.Thirty360(ql.Thirty360.BondBasis),
                                floatSchedule, euribor6m, ql.Actual360(),
                                False, False, CMSgearing, CMSspread, [], [],
                                Euriborgearing, Euriborspread)

# %%
swaption4 = ql.FloatFloatSwaption(underlying4, exercise)
floatSwaptionEngine = ql.Gaussian1dFloatFloatSwaptionEngine(
    gsr, 64, 7.0, True, False, ql.QuoteHandle(ql.SimpleQuote(0)), t0_Ois, True)
swaption4.setPricingEngine(floatSwaptionEngine)

# %% [markdown]
# Since the underlying is quite exotic already, we start with pricing this using the `LinearTsrPricer` for CMS coupon estimation.

# %%
leg0 = underlying4.leg(0)
leg1 = underlying4.leg(1)
reversionQuote = ql.QuoteHandle(ql.SimpleQuote(0.01))
swaptionVolHandle = ql.SwaptionVolatilityStructureHandle(swaptionVol)
cmsPricer = ql.LinearTsrPricer(swaptionVolHandle, reversionQuote)
iborPricer = ql.BlackIborCouponPricer()

# %%
ql.setCouponPricer(leg0, cmsPricer)
ql.setCouponPricer(leg1, iborPricer)

# %%
swapPricer = ql.DiscountingSwapEngine(t0_Ois)
underlying4.setPricingEngine(swapPricer)

# %%
print("Underlying CMS Swap NPV = %f" % underlying4.NPV())
print("Underlying CMS Leg  NPV = %f" % underlying4.legNPV(0))
print("Underlying Euribor  NPV = %f" % underlying4.legNPV(1))

# %% [markdown]
# We generate a naive calibration basket and calibrate the GSR model to it:

# %%
basket = swaption4.calibrationBasket(swapBase, swaptionVol, 'Naive')

# %%
for basket_i in basket:
    ql.as_black_helper(basket_i).setPricingEngine(swaptionEngine)

# %%
gsr.calibrateVolatilitiesIterative(basket, method, ec)
show(basket_data(basket))

# %%
show(calibration_data(basket, gsr.volatility()))

# %% [markdown]
# The npv of the bermudan swaption is:

# %%
print(swaption4.NPV())

# %% [markdown]
# In this case it is also interesting to look at the underlying swap NPV in the GSR model.

# %%
print(swaption4.underlyingValue())

# %% [markdown]
# Not surprisingly, the underlying is priced differently compared to the `LinearTsrPricer`, since a different smile is implied by the GSR model.
#
# This is exactly where the Markov functional model comes into play, because it can calibrate to any given underlying smile (as long as it is arbitrage free). We try this now. Of course the usual use case is not to calibrate to a flat smile as in our simple example, still it should be possible, of course...

# %%
markovStepDates = exerciseDates
cmsFixingDates = markovStepDates
markovSimgas = [0.01]* (len(markovStepDates)+1)
tenors = [ql.Period('10Y')]*len(cmsFixingDates)
markov = ql.MarkovFunctional(t0_curve, reversionQuote.value(), markovStepDates, markovSimgas, swaptionVolHandle,
                             cmsFixingDates, tenors, swapBase)

# %%
swaptionEngineMarkov = ql.Gaussian1dSwaptionEngine(markov, 8, 5.0, True,
                                                   False, t0_Ois)

# %%
floatEngineMarkov = ql.Gaussian1dFloatFloatSwaptionEngine(
    markov, 16, 7.0, True, False, ql.QuoteHandle(ql.SimpleQuote(0)), t0_Ois, True)

# %% [markdown]
# The option npv is the markov model is:

# %%
swaption4.setPricingEngine(floatEngineMarkov)
print(swaption4.NPV())

# %% [markdown]
# This is not too far from the GSR price. More interesting is the question how well the Markov model did its job to match our input smile. For this we look at the underlying npv under the Markov model.

# %%
print(swaption4.underlyingValue())

# %% [markdown]
# This is closer to our terminal swap rate model price. A perfect match is not expected anyway, because the dynamics of the underlying rate in the linear model is different from the Markov model, of course.
#
# The Markov model can not only calibrate to the underlying smile, but has at the same time a sigma function (similar to the GSR model) which can be used to calibrate to a second instrument set. We do this here to calibrate to our coterminal ATM swaptions from above.
#
# This is a computationally demanding task, so depending on your machine, this may take a while now...

# %%
for basket_i in basket:
    ql.as_black_helper(basket_i).setPricingEngine(swaptionEngineMarkov)

# %%
markov.calibrate(basket, method, ec)
show(calibration_data(basket, markov.volatility()))

# %% [markdown]
# Now let's have a look again at the underlying pricing. It shouldn't have changed much, because the underlying smile is still matched.

# %%
print(swaption4.underlyingValue())

# %% [markdown]
# This is close to the previous value as expected.
#
# As a final remark we note that the calibration to coterminal swaptions is not particularly reasonable here, because the European call rights are not well represented by these swaptions. Secondly, our CMS swaption is sensitive to the correlation between the 10y swap rate and the Euribor 6M rate. Since the Markov model is one factor it will most probably underestimate the market value by construction.
#
# That was it. Thank you for running this demo. Bye.
