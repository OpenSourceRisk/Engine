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

#include <boost/make_shared.hpp>
#include <ql/instruments/impliedvolatility.hpp>
#include <ql/instruments/vanillaoption.hpp>
#include <ql/math/solvers1d/brent.hpp>
#include <ql/pricingengines/vanilla/analyticeuropeanengine.hpp>
#include <ql/processes/blackscholesprocess.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <qle/pricingengines/baroneadesiwhaleyengine.hpp>
#include <qle/termstructures/blackvariancesurfacesparse.hpp>
#include <qle/termstructures/equityoptionsurfacestripper.hpp>

using std::pair;
using std::vector;
using namespace QuantLib;

namespace {

class PriceError {
public:
    PriceError(const VanillaOption& option, SimpleQuote& vol, Real targetValue)
        : option_(option), vol_(vol), targetValue_(targetValue){};
    Real operator()(Volatility x) const;

private:
    const VanillaOption& option_;
    SimpleQuote& vol_;
    Real targetValue_;
};

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

EquityOptionSurfaceStripper::EquityOptionSurfaceStripper(const boost::shared_ptr<OptionInterpolatorBase>& callSurface,
                                                         const boost::shared_ptr<OptionInterpolatorBase>& putSurface,
                                                         const Handle<EquityIndex>& eqIndex, const Calendar& calendar,
                                                         const DayCounter& dayCounter, Exercise::Type type,
                                                         bool lowerStrikeConstExtrap, bool upperStrikeConstExtrap,
                                                         bool timeFlatExtrapolation)
    : callSurface_(callSurface), putSurface_(putSurface), eqIndex_(eqIndex), calendar_(calendar),
      dayCounter_(dayCounter), type_(type), lowerStrikeConstExtrap_(lowerStrikeConstExtrap),
      upperStrikeConstExtrap_(upperStrikeConstExtrap), timeFlatExtrapolation_(timeFlatExtrapolation) {

    // the call and put surfaces should have the same expiries/strikes/reference date/day counters, some checks to
    // ensure this
    QL_REQUIRE(callSurface_->referenceDate() == putSurface_->referenceDate(),
               "Mismatch between Call and Put reference dates in EquityOptionPremiumCurveStripper");

    // register with all market data
    registerWith(eqIndex);
    registerWith(Settings::instance().evaluationDate());
}

void EquityOptionSurfaceStripper::performCalculations() const {
    // create a set of all dates
    std::set<Date> allExpiries;
    std::vector<Date> callExpiries = callSurface_->expiries();
    std::vector<Date> putExpiries = putSurface_->expiries();
    for (auto expiry : callExpiries)
        allExpiries.insert(expiry);
    for (auto expiry : putExpiries)
        allExpiries.insert(expiry);

    boost::shared_ptr<SimpleQuote> volQuote;
    boost::shared_ptr<PricingEngine> engine;

    boost::shared_ptr<BlackVarianceSurfaceSparse> callVolSurface, putVolSurface;

    // if the surface is a price surface then we have premiums
    bool premiumSurfaces = boost::dynamic_pointer_cast<OptionPriceSurface>(callSurface_) != NULL;
    if (premiumSurfaces) {
        // first also check the put surface
        QL_REQUIRE(boost::dynamic_pointer_cast<OptionPriceSurface>(putSurface_) != NULL,
                   "Call price surface provided, but no put price surface");

        // Set up the engine for implying the vols
        // term structures needed to get implied vol
        volQuote = boost::make_shared<SimpleQuote>(0.1);
        Handle<BlackVolTermStructure> volTs(boost::make_shared<BlackConstantVol>(
            callSurface_->referenceDate(), calendar_, Handle<Quote>(volQuote), dayCounter_));

        // a black scholes process
        boost::shared_ptr<GeneralizedBlackScholesProcess> gbsp = boost::make_shared<BlackScholesMertonProcess>(
            eqIndex_->equitySpot(), eqIndex_->equityDividendCurve(), eqIndex_->equityForecastCurve(), volTs);

        // hard code the engines here
        // BaroneAdesiWhaley for American options - much faster than alternatives
        // Black for European
        if (type_ == Exercise::American) {
            engine = boost::make_shared<QuantExt::BaroneAdesiWhaleyApproximationEngine>(gbsp);
        } else if (type_ == Exercise::European) {
            engine = boost::make_shared<QuantExt::AnalyticEuropeanEngine>(gbsp);
        } else {
            QL_FAIL("Unsupported exercise type for option stripping");
        }
    } else {
        // we have variance surfaces, explicitly cast so we can look up vol later
        callVolSurface = boost::dynamic_pointer_cast<BlackVarianceSurfaceSparse>(callSurface_);
        putVolSurface = boost::dynamic_pointer_cast<BlackVarianceSurfaceSparse>(putSurface_);
    }

    vector<Real> volStrikes;
    vector<Real> volData;
    vector<Date> volExpiries;

    // loop over each expiry
    for (auto exp : allExpiries) {
        // get the forward price at time
        Real forward = eqIndex_->fixing(exp);

        vector<Real> callStrikes, putStrikes;
        auto itc = std::find(callExpiries.begin(), callExpiries.end(), exp);
        if (itc != callExpiries.end()) {
            auto pos = std::distance(callExpiries.begin(), itc);
            callStrikes = callSurface_->strikes().at(pos);
        }
        auto itp = std::find(putExpiries.begin(), putExpiries.end(), exp);
        if (itp != putExpiries.end()) {
            auto pos = std::distance(putExpiries.begin(), itp);
            putStrikes = putSurface_->strikes().at(pos);
        }

        // We want a set of prices both sides of ATM forward
        // We take calls where strike < atm, and puts where strike > atm

        bool haveCalls = false, havePuts = false;
        // check if calls in the correct range
        if (callStrikes.size() > 0 && callStrikes.front() < forward)
            haveCalls = true;

        // check if puts in the correct range
        if (putStrikes.size() > 0 && putStrikes.back() > forward)
            havePuts = true;

        for (auto cs : callStrikes) {
            if (!havePuts || cs < forward) {
                volStrikes.push_back(cs);
                volExpiries.push_back(exp);
                if (premiumSurfaces) {
                    volData.push_back(implyVol(exp, cs, Option::Call, engine, volQuote));
                } else {
                    volData.push_back(callVolSurface->blackVol(exp, cs));
                }
            }
        }
        for (auto ps : putStrikes) {
            if (!haveCalls || ps > forward) {
                volStrikes.push_back(ps);
                volExpiries.push_back(exp);
                if (premiumSurfaces) {
                    volData.push_back(implyVol(exp, ps, Option::Put, engine, volQuote));
                } else {
                    volData.push_back(putVolSurface->blackVol(exp, ps));
                }
            }
        }
    }
    volSurface_ = boost::make_shared<BlackVarianceSurfaceSparse>(
        callSurface_->referenceDate(), calendar_, volExpiries, volStrikes, volData, dayCounter_,
        lowerStrikeConstExtrap_, upperStrikeConstExtrap_, timeFlatExtrapolation_);
}

Real EquityOptionSurfaceStripper::implyVol(Date expiry, Real strike, Option::Type type,
                                           boost::shared_ptr<PricingEngine> engine,
                                           boost::shared_ptr<SimpleQuote> volQuote) const {

    // create an american option for current strike/expiry and type
    boost::shared_ptr<StrikedTypePayoff> payoff(new PlainVanillaPayoff(type, strike));
    boost::shared_ptr<Exercise> exercise;
    if (type_ == Exercise::American) {
        exercise = boost::make_shared<AmericanExercise>(expiry);
    } else if (type_ == Exercise::European) {
        exercise = boost::make_shared<EuropeanExercise>(expiry);
    } else {
        QL_FAIL("Unsupported exercise type for option stripping");
    }
    VanillaOption option(payoff, exercise);
    option.setPricingEngine(engine);

    // option.setPricingEngine(engine);
    Real targetPrice =
        type == Option::Call ? callSurface_->getValue(expiry, strike) : putSurface_->getValue(expiry, strike);

    // calculate the implied volatility using a solver
    Real vol;
    try {
        PriceError f(option, *volQuote, targetPrice);
        Brent solver;
        solver.setMaxEvaluations(100);
        solver.setLowerBound(0.0001);
        vol = solver.solve(f, 0.0001, 0.2, 0.01);
    } catch (...) {
        vol = 0.0;
    }
    return vol;
}

boost::shared_ptr<QuantLib::BlackVolTermStructure> EquityOptionSurfaceStripper::volSurface() {
    calculate();
    return volSurface_;
}

} // namespace QuantExt