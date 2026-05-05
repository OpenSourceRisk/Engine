/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

#include <qle/instruments/indexcdsoption.hpp>
#include <qle/pricingengines/indexcdsoptionbaseengine.hpp>
#include <qle/pricingengines/blackindexcdsoptionengine.hpp>
#include <ql/exercise.hpp>
#include <ql/instruments/payoffs.hpp>
#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/math/solvers1d/brent.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <boost/make_shared.hpp>

using namespace QuantLib;

namespace QuantExt {

namespace {

class ImpliedVolHelper {
public:
    ImpliedVolHelper(const IndexCdsOption& cdsoption,
        Real targetValue,
        QuantLib::ext::shared_ptr<IndexCdsOptionBaseEngine> engine,
        QuantLib::Handle<QuantLib::SimpleQuote> volatility)
        : engine_(std::move(engine)), targetValue_(targetValue), vol_(*volatility)
    {
        cdsoption.setupArguments(engine_->getArguments());
        results_ = dynamic_cast<const Instrument::results*>(engine_->getResults());
    }

    Real operator()(Volatility x) const {
        vol_->setValue(x);
        engine_->calculate();
        return results_->value - targetValue_;
    }

    Real error() const {
        return results_->value - targetValue_;
    }

private:
    QuantLib::ext::shared_ptr<PricingEngine> engine_;
    Real targetValue_;
    QuantLib::ext::shared_ptr<SimpleQuote> vol_;
    const Instrument::results* results_;
};

}

IndexCdsOption::IndexCdsOption(const QuantLib::ext::shared_ptr<IndexCreditDefaultSwap>& swap,
                               const QuantLib::ext::shared_ptr<Exercise>& exercise, Real strike,
                               CdsOption::StrikeType strikeType, const Settlement::Type settlementType,
                               Real tradeDateNtl, Real realisedFep, const QuantLib::Period& indexTerm)
    : Option(QuantLib::ext::make_shared<NullPayoff>(), exercise), swap_(swap), strike_(strike), strikeType_(strikeType),
      settlementType_(settlementType), tradeDateNtl_(tradeDateNtl), realisedFep_(realisedFep),
      indexTerm_(indexTerm), riskyAnnuity_(0.0), fepAdjustedForwardPrice_(0.0), fepAdjustedForwardSpread_(0.0) {
    registerWith(swap_);
}

bool IndexCdsOption::isExpired() const {
    return detail::simple_event(exercise_->dates().back()).hasOccurred();
}

void IndexCdsOption::setupExpired() const {
    Instrument::setupExpired();
    riskyAnnuity_ = 0.0;
    fepAdjustedForwardPrice_ = 0.0;
    fepAdjustedForwardSpread_ = 0.0;
}

void IndexCdsOption::setupArguments(PricingEngine::arguments* args) const {
    Option::setupArguments(args);

    IndexCdsOption::arguments* arguments = dynamic_cast<IndexCdsOption::arguments*>(args);
    QL_REQUIRE(arguments, "Internal error IndexCdsOption: cannot cast arguments to IndexCdsOption::arguments.");
    arguments->swap = swap_;
    arguments->strike = strike_;
    arguments->strikeType = strikeType_;
    arguments->settlementType = settlementType_;
    arguments->tradeDateNtl = tradeDateNtl_;
    arguments->realisedFep = realisedFep_;
    arguments->indexTerm = indexTerm_;
}

void IndexCdsOption::fetchResults(const PricingEngine::results* r) const {
    Option::fetchResults(r);
    const IndexCdsOption::results* results = dynamic_cast<const IndexCdsOption::results*>(r);
    QL_ENSURE(results, "Internal error IndexCdsOption: cannot cast results to IndexCdsOption::results.");
    riskyAnnuity_ = results->riskyAnnuity;
    fepAdjustedForwardPrice_ = results->fepAdjustedForwardPrice;
    fepAdjustedForwardSpread_ = results->fepAdjustedForwardSpread;
}

Rate IndexCdsOption::atmRate() const {
    return swap_->fairSpreadClean();
}

Real IndexCdsOption::riskyAnnuity() const {
    calculate();
    QL_REQUIRE(riskyAnnuity_ != Null<Real>(), "Internal error IndexCdsOption: risky annuity not provided.");
    return riskyAnnuity_;
}

Real IndexCdsOption::fepAdjustedForwardPrice() const {
    calculate();
    QL_REQUIRE(fepAdjustedForwardPrice_ != Null<Real>(),
        "Internal error IndexCdsOption: FEP adjusted forward price not provided.");
    return fepAdjustedForwardPrice_;
}

Spread IndexCdsOption::fepAdjustedForwardSpread() const {
    calculate();
    QL_REQUIRE(fepAdjustedForwardSpread_ != Null<QuantLib::Spread>(),
        "Internal error IndexCdsOption: FEP adjusted forward spread not provided.");
    return fepAdjustedForwardSpread_;
}

Volatility IndexCdsOption::impliedVolatility(
    Real targetValue,
    const Handle<YieldTermStructure>& termStructureSwapCurrency,
    const Handle<YieldTermStructure>& termStructureTradeCollateral,
    const Handle<DefaultProbabilityTermStructure>& probability,
    Real recoveryRate, Real accuracy, Size maxEvaluations, Volatility minVol,
    Volatility maxVol) const
{
    Solver1DOptions options;
    options.initialGuess = 0.10;
    options.accuracy = accuracy;
    options.maxEvaluations = maxEvaluations;
    options.minMax = {minVol, maxVol};

    auto volPtr = ext::make_shared<SimpleQuote>(0.0);
    Handle<BlackVolTermStructure> volTs(ext::make_shared<BlackConstantVol>(0, NullCalendar(),
        Handle<Quote>(volPtr), Actual365Fixed()));
    Handle<CreditVolCurve> volCurve(ext::make_shared<CreditVolCurveWrapper>(volTs));
    auto engine = ext::make_shared<QuantExt::BlackIndexCdsOptionEngine>(probability, recoveryRate,
        termStructureSwapCurrency, termStructureTradeCollateral, volCurve);

    auto result = impliedVolatility(targetValue, engine, Handle<SimpleQuote>(volPtr), options);

    return result.volatility;
}

IndexCdsOption::ImpliedVolatilityResult IndexCdsOption::impliedVolatility(Real price,
    ext::shared_ptr<IndexCdsOptionBaseEngine> engine, Handle<SimpleQuote> volatility,
    Solver1DOptions solverOptions) const
{
    calculate();
    QL_REQUIRE(!isExpired(), "Internal error IndexCdsOption::impliedVolatility: option has expired.");
    QL_REQUIRE(engine, "Internal error IndexCdsOption::impliedVolatility: engine was not provided.");
    QL_REQUIRE(!volatility.empty(), "Internal error IndexCdsOption::impliedVolatility: volatility was not provided.");

    ImpliedVolHelper f(*this, price, engine, volatility);
    auto [solver, useMinMax] = createSolver1D<Brent>(solverOptions);
    ImpliedVolatilityResult result;
    try
    {
        if (useMinMax) {
            auto [minVol, maxVol] = solverOptions.minMax;
            result.volatility = solver.solve(f, solverOptions.accuracy, solverOptions.initialGuess, minVol, maxVol);
        } else {
            result.volatility = solver.solve(f, solverOptions.accuracy, solverOptions.initialGuess, solverOptions.step);
        }
        result.success = true;
    }
    catch (const QuantLib::Error& e)
    {
        // Set result to the final volatility tried.
        result.volatility = volatility->value();
        result.success = false;
        result.errorMessage = e.what();
    }

    result.error = f.error();

    return result;
}

void IndexCdsOption::setStrike(QuantLib::Real strike)
{
    strike_ = strike;
    // After the update, we need to mark as not calculated.
    update();
}

void IndexCdsOption::arguments::validate() const {
    Option::arguments::validate();
    QL_REQUIRE(swap, "Internal error IndexCdsOption: underlying index CDS is not provided.");
}

void IndexCdsOption::results::reset() {
    Option::results::reset();
    riskyAnnuity = Null<Real>();
    fepAdjustedForwardPrice = Null<Real>();
    fepAdjustedForwardSpread = Null<Spread>();
}

} // namespace QuantExt
