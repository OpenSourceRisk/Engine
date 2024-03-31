/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of ORE, a free-software/open-source library
 for transparent pricing and risk analysis - http://opensourcerisk.org

 ORE is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program.
 The license is also available online at <http://opensourcerisk.org>

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

#include <ql/instruments/impliedvolatility.hpp>
#include <ql/instruments/vanillaoption.hpp>
#include <ql/math/solvers1d/brent.hpp>
#include <ql/pricingengines/blackformula.hpp>
#include <ql/processes/blackscholesprocess.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <qle/pricingengines/baroneadesiwhaleyengine.hpp>
#include <qle/termstructures/equityforwardcurvestripper.hpp>

using std::set;
using std::vector;
using namespace QuantLib;

namespace {

class PriceError {
public:
    PriceError(const VanillaOption& option, SimpleQuote& vol, Real targetValue);
    Real operator()(Volatility x) const;

private:
    const VanillaOption& option_;
    SimpleQuote& vol_;
    Real targetValue_;
};

PriceError::PriceError(const VanillaOption& option, SimpleQuote& vol, Real targetValue)
    : option_(option), vol_(vol), targetValue_(targetValue){};

Real PriceError::operator()(Volatility x) const {
    vol_.setValue(x);
    Real npv;
    // Barone Adesi Whaley fails for very small variance, so wrap in a try/catch
    try {
        npv = option_.NPV();
    } catch (...) {
        npv = 0.0;
    }
    return npv - targetValue_;
}

} // namespace

namespace QuantExt {

EquityForwardCurveStripper::EquityForwardCurveStripper(const QuantLib::ext::shared_ptr<OptionPriceSurface>& callSurface,
                                                       const QuantLib::ext::shared_ptr<OptionPriceSurface>& putSurface,
                                                       Handle<YieldTermStructure>& forecastCurve,
                                                       Handle<QuantLib::Quote>& equitySpot, Exercise::Type type)
    : callSurface_(callSurface), putSurface_(putSurface), forecastCurve_(forecastCurve), equitySpot_(equitySpot),
      type_(type), forwards_(callSurface_->expiries().size()) {

    // the call and put surfaces should have the same expiries/strikes/reference date/day counters, some checks to
    // ensure this
    QL_REQUIRE(callSurface_->strikes() == putSurface_->strikes(),
               "Mismatch between Call and Put strikes in EquityForwardCurveStripper");
    QL_REQUIRE(callSurface_->expiries() == putSurface_->expiries(),
               "Mismatch between Call and Put expiries in EquityForwardCurveStripper");
    QL_REQUIRE(callSurface_->referenceDate() == putSurface_->referenceDate(),
               "Mismatch between Call and Put reference dates in EquityForwardCurveStripper");
    QL_REQUIRE(callSurface_->dayCounter() == putSurface_->dayCounter(),
               "Mismatch between Call and Put day counters in EquityForwardCurveStripper");

    // register with all market data
    registerWith(callSurface);
    registerWith(putSurface);
    registerWith(forecastCurve);
    registerWith(equitySpot);
    registerWith(Settings::instance().evaluationDate());
}

void EquityForwardCurveStripper::performCalculations() const {

    vector<vector<Real> > allStrikes = callSurface_->strikes();
    forwards_.resize(callSurface_->expiries().size());

    // at each option expiry time we calculate a forward
    for (Size i = 0; i < expiries().size(); i++) {
        Date expiry = expiries()[i];
        // get the relevant strikes at this expiry
        vector<Real> strikes = allStrikes[i];
        QL_REQUIRE(strikes.size() > 0, "No strikes for expiry " << expiry);

        // if we only have one strike we just use that to get the forward
        if (strikes.size() == 1) {
            forwards_[i] = forwardFromPutCallParity(expiry, strikes.front(), *callSurface_, *putSurface_);
            continue;
        }

        // we make a first guess at the forward price
        // strikes are ordered, lowest to highest, we take the first guess as midpoint of 2 strikes
        // where (C-P) goes from positive to negative
        Real forward = strikes.back();
        for (Size k = 0; k < strikes.size(); k++) {
            if (callSurface_->price(expiry, strikes[k]) <= putSurface_->price(expiry, strikes[k])) {
                if (k == 0)
                    forward = strikes.front();
                else
                    forward = (strikes[k] + strikes[k - 1]) / 2;
                break;
            }
        }

        // call and put surface to be used to find forward - updated for American
        auto callSurface = callSurface_;
        auto putSurface = putSurface_;

        Size maxIter = 100;
        Size j = 0;
        bool isForward = false;
        while (!isForward && j < maxIter) {

            if (type_ == Exercise::American) {

                // we take the strike either side of our forward guess, this is sufficient because
                // we construct new price surfaces but only ask for the price at the forward from these
                vector<Real> amerStrikes(2);
                auto it_lower = std::lower_bound(strikes.begin(), strikes.end(), forward);
                if (it_lower == strikes.end())
                    it_lower = std::prev(it_lower);
                amerStrikes[1] = *it_lower;
                amerStrikes[0] = (it_lower == strikes.begin()) ? *it_lower  : *std::prev(it_lower);

                // for American options we first get the implied vol from the American premiums
                // we use these to construct the European prices in order to apply put call parity

                // get date and daycounter from the prices surface
                Date asof = callSurface_->referenceDate();
                DayCounter dc = callSurface_->dayCounter();
                Calendar cal = callSurface_->calendar();
                Time t = dc.yearFraction(asof, expiry);

                // dividend rate from S_t = S * exp((r - q) * t)
                Real q = forecastCurve_->zeroRate(t, Continuous) - log(forward / equitySpot_->value()) / t;

                // term structures needed to get implied vol
                QuantLib::ext::shared_ptr<SimpleQuote> volQuote = QuantLib::ext::make_shared<SimpleQuote>(0.1);
                Handle<BlackVolTermStructure> volTs(
                    QuantLib::ext::make_shared<BlackConstantVol>(asof, cal, Handle<Quote>(volQuote), dc));
                Handle<YieldTermStructure> divTs(QuantLib::ext::make_shared<FlatForward>(asof, q, dc));

                // a black scholes process
                QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess> gbsp =
                    QuantLib::ext::make_shared<BlackScholesMertonProcess>(equitySpot_, divTs, forecastCurve_, volTs);
                QuantLib::ext::shared_ptr<PricingEngine> engine =
                    QuantLib::ext::make_shared<QuantExt::BaroneAdesiWhaleyApproximationEngine>(gbsp);

                vector<vector<Volatility> > vols(2, vector<Volatility>(amerStrikes.size()));
                vector<Option::Type> types;
                types.push_back(Option::Call);
                types.push_back(Option::Put);

                for (Size l = 0; l < types.size(); l++) {
                    for (Size k = 0; k < amerStrikes.size(); k++) {
                        // create an american option for current strike/expiry and type
                        QuantLib::ext::shared_ptr<StrikedTypePayoff> payoff(new PlainVanillaPayoff(types[l], amerStrikes[k]));
                        QuantLib::ext::shared_ptr<Exercise> exercise = QuantLib::ext::make_shared<AmericanExercise>(expiry);
                        VanillaOption option(payoff, exercise);
                        option.setPricingEngine(engine);

                        // option.setPricingEngine(engine);
                        Real targetPrice = types[l] == Option::Call ? callSurface_->price(expiry, amerStrikes[k])
                                                                    : putSurface_->price(expiry, amerStrikes[k]);

                        // calculate the implied volatility using a solver
                        try {
                            PriceError f(option, *volQuote, targetPrice);
                            Brent solver;
                            solver.setMaxEvaluations(100);
                            solver.setLowerBound(0.0001);
                            vols[l][k] = solver.solve(f, 0.0001, 0.2, 0.01);
                        } catch (...) {
                            vols[l][k] = 0.0;
                        }
                    }
                }

                vector<Real> newStrikes;
                vector<Date> dates;
                vector<Real> callPremiums, putPremiums;

                for (Size k = 0; k < amerStrikes.size(); k++) {
                    if (vols[0][k] != 0.0 && vols[1][k] != 0.0) {
                        // get the european option prices for each strike
                        Real call = blackFormula(Option::Call, amerStrikes[k], forward, vols[0][k] * sqrt(t),
                                                 forecastCurve_->discount(t));
                        Real put = blackFormula(Option::Put, amerStrikes[k], forward, vols[1][k] * sqrt(t),
                                                forecastCurve_->discount(t));

                        if (call != 0.0 && put != 0.0) {
                            newStrikes.push_back(amerStrikes[k]);
                            dates.push_back(expiry);
                            callPremiums.push_back(call);
                            putPremiums.push_back(put);
                        }
                    }
                }
                // throw away any strikes where the vol is zero for either put or call
                // must have at least one new strike otherwise continue with current price surfaces
                if (newStrikes.size() > 0) {
                    // build call/put price surfaces with the new European prices
                    callSurface = QuantLib::ext::make_shared<OptionPriceSurface>(asof, dates, newStrikes, callPremiums, dc);
                    putSurface = QuantLib::ext::make_shared<OptionPriceSurface>(asof, dates, newStrikes, putPremiums, dc);
                }
            }

            Real newForward = 0.0;
            // if our guess is below the first strike or after the last strike we just take the relevant strike
            if (forward <= strikes.front()) {
                newForward = forwardFromPutCallParity(expiry, strikes.front(), *callSurface, *putSurface);
                // if forward is still less than first strike we accept this
                isForward = newForward <= strikes.front();
            } else if (forward >= strikes.back()) {
                newForward = forwardFromPutCallParity(expiry, strikes.back(), *callSurface, *putSurface);
                // if forward is still greater than last strike we accept this
                isForward = newForward >= strikes.back();
            } else {
                newForward = forwardFromPutCallParity(expiry, forward, *callSurface, *putSurface);

                // check - has it moved by less that 0.1%
                isForward = fabs((newForward - forward) / forward) < 0.001;
            }
            forward = newForward;
            j++;
        }
        forwards_[i] = forward;
    }
}

Real EquityForwardCurveStripper::forwardFromPutCallParity(Date d, Real strike, const OptionPriceSurface& callSurface,
                                                          const OptionPriceSurface& putSurface) const {
    Real C = callSurface.price(d, strike);
    Real P = putSurface.price(d, strike);
    Real D = forecastCurve_->discount(d);

    return strike + (C - P) / D;
}

const vector<Date> EquityForwardCurveStripper::expiries() const {
    calculate();
    return callSurface_->expiries();
}

const vector<Real> EquityForwardCurveStripper::forwards() const {
    calculate();
    return forwards_;
}

} // namespace QuantExt
