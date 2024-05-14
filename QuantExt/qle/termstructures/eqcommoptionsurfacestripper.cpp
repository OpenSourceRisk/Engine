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

#include <qle/termstructures/eqcommoptionsurfacestripper.hpp>
#include <boost/make_shared.hpp>
#include <ql/instruments/impliedvolatility.hpp>
#include <ql/math/solver1d.hpp>
#include <ql/pricingengines/vanilla/analyticeuropeanengine.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <qle/pricingengines/baroneadesiwhaleyengine.hpp>
#include <qle/termstructures/blackvariancesurfacesparse.hpp>
#include <qle/termstructures/pricetermstructureadapter.hpp>

using std::function;
using std::map;
using std::pair;
using std::set;
using std::vector;
using namespace QuantLib;

namespace {

// Utility method to create the list of options to be used at an expiry date for stripping.
function<bool(Real,Real)> comp = [](Real a, Real b) { return !close(a, b) && a < b; };

map<Real, Option::Type, decltype(comp)> createStrikes(Real forward, const vector<Real>& cStrikes,
    const vector<Real>& pStrikes, bool preferOutOfTheMoney) {

    // Firstly create the restricted vector of call and put strikes.
    vector<Real> rcStks;
    copy_if(cStrikes.begin(), cStrikes.end(), back_inserter(rcStks),[forward, preferOutOfTheMoney](Real stk) {
        return (preferOutOfTheMoney && stk >= forward) || (!preferOutOfTheMoney && stk <= forward); });
    vector<Real> rpStks;
    copy_if(pStrikes.begin(), pStrikes.end(), back_inserter(rpStks), [forward, preferOutOfTheMoney](Real stk) {
        return (preferOutOfTheMoney && stk <= forward) || (!preferOutOfTheMoney && stk >= forward); });

    // Create the empty map.
    map<Real, Option::Type, decltype(comp)> res(comp);

    // If both restricted vectors are empty, return an empty map
    if (rcStks.empty() && rpStks.empty())
        return res;

    // At least one of the restricted strike vectors are non empty so populate the map.
    if (!rcStks.empty() && !rpStks.empty()) {
        // Most common case hopefully. Use both sets of strikes.
        // Could have the fwd strike in both restricted sets from the logic above. Favour Call here via overwrite.
        for (Real stk : rpStks)
            res[stk] = Option::Put;
        for (Real stk : rcStks)
            res[stk] = Option::Call;
    } else if (rpStks.empty()) {
        // If restricted put strikes are empty, use all the call strikes
        for (Real stk : cStrikes)
            res[stk] = Option::Call;
    } else if (rcStks.empty()) {
        // If restricted call strikes are empty, use all the put strikes
        for (Real stk : pStrikes)
            res[stk] = Option::Put;
    }

    return res;
}

}

namespace QuantExt {

OptionSurfaceStripper::OptionSurfaceStripper(
    const QuantLib::ext::shared_ptr<OptionInterpolatorBase>& callSurface,
    const QuantLib::ext::shared_ptr<OptionInterpolatorBase>& putSurface,
    const Calendar& calendar,
    const DayCounter& dayCounter,
    Exercise::Type type,
    bool lowerStrikeConstExtrap,
    bool upperStrikeConstExtrap,
    bool timeFlatExtrapolation,
    bool preferOutOfTheMoney,
    Solver1DOptions solverOptions)
    : callSurface_(callSurface),
      putSurface_(putSurface),
      calendar_(calendar),
      dayCounter_(dayCounter),
      type_(type),
      lowerStrikeConstExtrap_(lowerStrikeConstExtrap),
      upperStrikeConstExtrap_(upperStrikeConstExtrap),
      timeFlatExtrapolation_(timeFlatExtrapolation),
      preferOutOfTheMoney_(preferOutOfTheMoney),
      solverOptions_(solverOptions),
      havePrices_(QuantLib::ext::dynamic_pointer_cast<OptionPriceSurface>(callSurface_)) {

    QL_REQUIRE(callSurface_->referenceDate() == putSurface_->referenceDate(),
        "Mismatch between Call and Put reference dates in OptionSurfaceStripper");

    registerWith(Settings::instance().evaluationDate());

    // Set up that is only needed if we have price based surfaces and we are stripping volatilities.
    if (havePrices_) {

        // Check that there is also a put price surface
        QL_REQUIRE(QuantLib::ext::dynamic_pointer_cast<OptionPriceSurface>(putSurface_),
            "OptionSurfaceStripper: call price surface provided but no put price surface.");

        setUpSolver();
    }
}

OptionSurfaceStripper::PriceError::PriceError(const VanillaOption& option, SimpleQuote& volatility, Real targetPrice)
    : option_(option), volatility_(volatility), targetPrice_(targetPrice) {}

Real OptionSurfaceStripper::PriceError::PriceError::operator()(Volatility x) const {

    volatility_.setValue(x);

    // Barone Adesi Whaley fails for very small variance, so wrap in a try catch
    Real npv;
    try {
        npv = option_.NPV();
    } catch (...) {
        npv = 0.0;
    }

    return npv - targetPrice_;
}

void OptionSurfaceStripper::performCalculations() const {

    // Create a set of all expiries
    auto tmp = callSurface_->expiries();
    set<Date> allExpiries(tmp.begin(), tmp.end());
    tmp = putSurface_->expiries();
    allExpiries.insert(tmp.begin(), tmp.end());

    QuantLib::ext::shared_ptr<BlackVarianceSurfaceSparse> callVolSurface;
    QuantLib::ext::shared_ptr<BlackVarianceSurfaceSparse> putVolSurface;

    // Switch based on whether surface is direct volatilities or prices to be stripped.
    QuantLib::ext::shared_ptr<PricingEngine> engine;
    QuantLib::ext::shared_ptr<SimpleQuote> volQuote = QuantLib::ext::make_shared<SimpleQuote>(0.1);
    if (havePrices_) {

        // a black scholes process
        QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess> gbsp = process(volQuote);

        // hard code the engines here
        if (type_ == Exercise::American) {
            engine = QuantLib::ext::make_shared<QuantExt::BaroneAdesiWhaleyApproximationEngine>(gbsp);
        } else if (type_ == Exercise::European) {
            engine = QuantLib::ext::make_shared<QuantExt::AnalyticEuropeanEngine>(gbsp);
        } else {
            QL_FAIL("Unsupported exercise type for option stripping");
        }

    } else {
        // we have variance surfaces, explicitly cast so we can look up vol later
        callVolSurface = QuantLib::ext::dynamic_pointer_cast<BlackVarianceSurfaceSparse>(callSurface_);
        putVolSurface = QuantLib::ext::dynamic_pointer_cast<BlackVarianceSurfaceSparse>(putSurface_);
    }

    // Need to populate these below to feed to BlackVarianceSurfaceSparse
    vector<Real> volStrikes;
    vector<Real> volData;
    vector<Date> volExpiries;

    // Loop over each expiry
    for (const Date& expiry : allExpiries) {

        // Get the forward price at expiry
        Real fwd = forward(expiry);

        // Get the call and put strikes at the expiry date. Each may be empty.
        vector<Real> callStrikes = strikes(expiry, true);
        vector<Real> putStrikes = strikes(expiry, false);

        // We want a set of prices both sides of ATM forward
        // If preferOutOfTheMoney_ is false, we take calls where strike < atm and puts where strike > atm.
        // If preferOutOfTheMoney_ is true, we take calls where strike > atm and puts where strike < atm.
        auto relevantStrikes = createStrikes(fwd, callStrikes, putStrikes, preferOutOfTheMoney_);
        for (const auto& kv : relevantStrikes) {
            if (havePrices_) {
                // Only use the volatility if the root finding was successful
                Real v = implyVol(expiry, kv.first, kv.second, engine, *volQuote);
                if (v != Null<Real>()) {
                    volExpiries.push_back(expiry);
                    volStrikes.push_back(kv.first);
                    volData.push_back(v);
                }
            } else {
                volExpiries.push_back(expiry);
                volStrikes.push_back(kv.first);
                Real v = kv.second == Option::Call ? callVolSurface->blackVol(expiry, kv.first) :
                    putVolSurface->blackVol(expiry, kv.first);
                volData.push_back(v);
            }
        }
    }

    // Populate the variance surface.
    volSurface_ = QuantLib::ext::make_shared<BlackVarianceSurfaceSparse>(
        callSurface_->referenceDate(), calendar_, volExpiries, volStrikes, volData, dayCounter_,
        lowerStrikeConstExtrap_, upperStrikeConstExtrap_, timeFlatExtrapolation_);
}

vector<Real> OptionSurfaceStripper::strikes(const Date& expiry, bool isCall) const {

    const QuantLib::ext::shared_ptr<OptionInterpolatorBase>& surface = isCall ? callSurface_ : putSurface_;
    auto expiries = surface->expiries();
    auto it = find(expiries.begin(), expiries.end(), expiry);

    if (it != expiries.end()) {
        return surface->strikes().at(distance(expiries.begin(), it));
    } else {
        return {};
    }

}

Real OptionSurfaceStripper::implyVol(Date expiry, Real strike, Option::Type type,
    QuantLib::ext::shared_ptr<PricingEngine> engine, SimpleQuote& volQuote) const {

    // Create the option instrument used in the solver.
    QuantLib::ext::shared_ptr<StrikedTypePayoff> payoff = QuantLib::ext::make_shared<PlainVanillaPayoff>(type, strike);
    QuantLib::ext::shared_ptr<Exercise> exercise;
    if (type_ == Exercise::American) {
        exercise = QuantLib::ext::make_shared<AmericanExercise>(expiry);
    } else if (type_ == Exercise::European) {
        exercise = QuantLib::ext::make_shared<EuropeanExercise>(expiry);
    } else {
        QL_FAIL("OptionSurfaceStripper: unsupported exercise type for option stripping.");
    }
    VanillaOption option(payoff, exercise);
    option.setPricingEngine(engine);

    // Get the target price from the surface.
    Real targetPrice = type == Option::Call ? callSurface_->getValue(expiry, strike)
        : putSurface_->getValue(expiry, strike);

    // Attempt to calculate the implied volatility.
    Real vol = Null<Real>();
    try {
        PriceError f(option, volQuote, targetPrice);
        vol = solver_(f);
    } catch (const Error&) {
    }

    return vol;
}

void OptionSurfaceStripper::setUpSolver() {

    // Check that enough solver options have been provided.
    const Real& guess = solverOptions_.initialGuess;
    QL_REQUIRE(guess != Null<Real>(), "OptionSurfaceStripper: need a valid initial " <<
        "guess for a price based surface.");

    const Real& accuracy = solverOptions_.accuracy;
    QL_REQUIRE(accuracy != Null<Real>(), "OptionSurfaceStripper: need a valid accuracy " <<
        "for a price based surface.");

    // Set maximum evaluations if provided.
    if (solverOptions_.maxEvaluations != Null<Size>())
        brent_.setMaxEvaluations(solverOptions_.maxEvaluations);

    // Check and set the lower bound and upper bound
    if (solverOptions_.lowerBound != Null<Real>() && solverOptions_.upperBound != Null<Real>()) {
        QL_REQUIRE(solverOptions_.lowerBound < solverOptions_.upperBound, "OptionSurfaceStripper: lowerBound (" <<
            solverOptions_.lowerBound << ") should be less than upperBound (" << solverOptions_.upperBound << ")");
    }

    if (solverOptions_.lowerBound != Null<Real>())
        brent_.setLowerBound(solverOptions_.lowerBound);
    if (solverOptions_.upperBound != Null<Real>())
        brent_.setUpperBound(solverOptions_.upperBound);

    // Choose a min/max or step solver based on parameters provided, favouring the min/max based version.
    const Real& min = solverOptions_.minMax.first;
    const Real& max = solverOptions_.minMax.second;
    const Real& step = solverOptions_.step;
    using std::placeholders::_1;
    if (min != Null<Real>() && max != Null<Real>()) {
        typedef Real (Brent::* MinMaxSolver)(const PriceError&, Real, Real, Real, Real) const;
        solver_ = std::bind(static_cast<MinMaxSolver>(&Brent::solve), &brent_, _1, accuracy, guess, min, max);
    } else if (step != Null<Real>()) {
        typedef Real(Brent::* StepSolver)(const PriceError&, Real, Real, Real) const;
        solver_ = std::bind(static_cast<StepSolver>(&Brent::solve), &brent_, _1, accuracy, guess, step);
    } else {
        QL_FAIL("OptionSurfaceStripper: need a valid step size or (min, max) pair for a price based surface.");
    }

}

QuantLib::ext::shared_ptr<BlackVolTermStructure> OptionSurfaceStripper::volSurface() {
    calculate();
    return volSurface_;
}

EquityOptionSurfaceStripper::EquityOptionSurfaceStripper(
    const Handle<QuantExt::EquityIndex2>& equityIndex,
    const QuantLib::ext::shared_ptr<OptionInterpolatorBase>& callSurface,
    const QuantLib::ext::shared_ptr<OptionInterpolatorBase>& putSurface,
    const Calendar& calendar,
    const DayCounter& dayCounter,
    Exercise::Type type,
    bool lowerStrikeConstExtrap,
    bool upperStrikeConstExtrap,
    bool timeFlatExtrapolation,
    bool preferOutOfTheMoney,
    Solver1DOptions solverOptions)
    : OptionSurfaceStripper(callSurface, putSurface, calendar, dayCounter, type, lowerStrikeConstExtrap,
        upperStrikeConstExtrap, timeFlatExtrapolation, preferOutOfTheMoney, solverOptions), equityIndex_(equityIndex) {
    registerWith(equityIndex_);
}

QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess> EquityOptionSurfaceStripper::process(
    const QuantLib::ext::shared_ptr<QuantLib::SimpleQuote>& volatilityQuote) const {

    Handle<BlackVolTermStructure> vts(QuantLib::ext::make_shared<BlackConstantVol>(
        callSurface_->referenceDate(), calendar_, Handle<Quote>(volatilityQuote), dayCounter_));

    return QuantLib::ext::make_shared<BlackScholesMertonProcess>(equityIndex_->equitySpot(),
        equityIndex_->equityDividendCurve(), equityIndex_->equityForecastCurve(), vts);
}

Real EquityOptionSurfaceStripper::forward(const Date& date) const {
    return equityIndex_->forecastFixing(date);
}

CommodityOptionSurfaceStripper::CommodityOptionSurfaceStripper(
    const Handle<PriceTermStructure>& priceCurve,
    const Handle<YieldTermStructure>& discountCurve,
    const QuantLib::ext::shared_ptr<OptionInterpolatorBase>& callSurface,
    const QuantLib::ext::shared_ptr<OptionInterpolatorBase>& putSurface,
    const Calendar& calendar,
    const DayCounter& dayCounter,
    Exercise::Type type,
    bool lowerStrikeConstExtrap,
    bool upperStrikeConstExtrap,
    bool timeFlatExtrapolation,
    bool preferOutOfTheMoney,
    Solver1DOptions solverOptions)
    : OptionSurfaceStripper(callSurface, putSurface, calendar, dayCounter, type, lowerStrikeConstExtrap,
        upperStrikeConstExtrap, timeFlatExtrapolation, preferOutOfTheMoney, solverOptions),
        priceCurve_(priceCurve), discountCurve_(discountCurve) {
    registerWith(priceCurve_);
    registerWith(discountCurve_);
}

QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess> CommodityOptionSurfaceStripper::process(
    const QuantLib::ext::shared_ptr<QuantLib::SimpleQuote>& volatilityQuote) const {

    QL_REQUIRE(!priceCurve_.empty(), "CommodityOptionSurfaceStripper: price curve is empty");
    QL_REQUIRE(!discountCurve_.empty(), "CommodityOptionSurfaceStripper: discount curve is empty");

    // Volatility term structure for the process
    Handle<BlackVolTermStructure> vts(QuantLib::ext::make_shared<BlackConstantVol>(
        callSurface_->referenceDate(), calendar_, Handle<Quote>(volatilityQuote), dayCounter_));

    // Generate "spot" and "yield" curve for the process.
    Handle<Quote> spot(QuantLib::ext::make_shared<DerivedPriceQuote>(priceCurve_));
    Handle<YieldTermStructure> yield(QuantLib::ext::make_shared<PriceTermStructureAdapter>(*priceCurve_, *discountCurve_));
    yield->enableExtrapolation();

    return QuantLib::ext::make_shared<QuantLib::GeneralizedBlackScholesProcess>(spot, yield, discountCurve_, vts);
}

Real CommodityOptionSurfaceStripper::forward(const Date& date) const {
    QL_REQUIRE(!priceCurve_.empty(), "CommodityOptionSurfaceStripper: price curve is empty");
    return priceCurve_->price(date);
}

}
