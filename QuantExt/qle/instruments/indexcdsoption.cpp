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
    ImpliedVolHelper(const IndexCdsOption& cdsoption, const Handle<DefaultProbabilityTermStructure>& probability,
                     Real recoveryRate, const Handle<YieldTermStructure>& termStructure, Real targetValue)
        : targetValue_(targetValue) {

        vol_ = boost::shared_ptr<SimpleQuote>(new SimpleQuote(0.0));
        Handle<BlackVolTermStructure> h(
            boost::make_shared<BlackConstantVol>(0, NullCalendar(), Handle<Quote>(vol_), Actual365Fixed()));
        engine_ = boost::make_shared<QuantExt::BlackIndexCdsOptionEngine>(
            probability, recoveryRate, termStructure,
            Handle<CreditVolCurve>(boost::make_shared<CreditVolCurveWrapper>(h)));
        cdsoption.setupArguments(engine_->getArguments());

        results_ = dynamic_cast<const Instrument::results*>(engine_->getResults());
    }
    Real operator()(Volatility x) const {
        vol_->setValue(x);
        engine_->calculate();
        return results_->value - targetValue_;
    }

private:
    boost::shared_ptr<PricingEngine> engine_;
    Real targetValue_;
    boost::shared_ptr<SimpleQuote> vol_;
    const Instrument::results* results_;
};

}

IndexCdsOption::IndexCdsOption(const boost::shared_ptr<IndexCreditDefaultSwap>& swap,
                               const boost::shared_ptr<Exercise>& exercise, Real strike,
                               CdsOption::StrikeType strikeType, Real tradeDateNtl, Real realisedFep, bool knocksOut,
                               const QuantLib::Period& indexTerm)
    : Option(boost::shared_ptr<Payoff>(new NullPayoff), exercise), swap_(swap), strike_(strike),
      strikeType_(strikeType), tradeDateNtl_(tradeDateNtl), realisedFep_(realisedFep), knocksOut_(knocksOut),
      indexTerm_(indexTerm), riskyAnnuity_(0.0) {
    registerWith(swap_);
}

bool IndexCdsOption::isExpired() const { return detail::simple_event(exercise_->dates().back()).hasOccurred(); }

void IndexCdsOption::setupExpired() const {
    Instrument::setupExpired();
    riskyAnnuity_ = 0.0;
}

void IndexCdsOption::setupArguments(PricingEngine::arguments* args) const {
    swap_->setupArguments(args);
    Option::setupArguments(args);

    IndexCdsOption::arguments* arguments = dynamic_cast<IndexCdsOption::arguments*>(args);

    QL_REQUIRE(arguments != 0, "wrong argument type");

    arguments->swap = swap_;
    arguments->strike = strike_;
    arguments->strikeType = strikeType_;
    arguments->tradeDateNtl = tradeDateNtl_;
    arguments->realisedFep = realisedFep_;
    arguments->knocksOut = knocksOut_;
    arguments->indexTerm = indexTerm_;
}

void IndexCdsOption::fetchResults(const PricingEngine::results* r) const {
    Option::fetchResults(r);
    const IndexCdsOption::results* results = dynamic_cast<const IndexCdsOption::results*>(r);
    QL_ENSURE(results != 0, "wrong results type");
    riskyAnnuity_ = results->riskyAnnuity;
}

Rate IndexCdsOption::atmRate() const { return swap_->fairSpreadClean(); }

Real IndexCdsOption::riskyAnnuity() const {
    calculate();
    QL_REQUIRE(riskyAnnuity_ != Null<Real>(), "risky annuity not provided");
    return riskyAnnuity_;
}

Volatility IndexCdsOption::impliedVolatility(Real targetValue, const Handle<YieldTermStructure>& termStructure,
                                             const Handle<DefaultProbabilityTermStructure>& probability,
                                             Real recoveryRate, Real accuracy, Size maxEvaluations, Volatility minVol,
                                             Volatility maxVol) const {
    calculate();
    QL_REQUIRE(!isExpired(), "instrument expired");

    Volatility guess = 0.10;

    ImpliedVolHelper f(*this, probability, recoveryRate, termStructure, targetValue);
    Brent solver;
    solver.setMaxEvaluations(maxEvaluations);
    return solver.solve(f, accuracy, guess, minVol, maxVol);
}

void IndexCdsOption::arguments::validate() const {
    IndexCreditDefaultSwap::arguments::validate();
    Option::arguments::validate();
    QL_REQUIRE(swap, "CDS not set");
    QL_REQUIRE(exercise, "exercise not set");
}

void IndexCdsOption::results::reset() {
    Option::results::reset();
    riskyAnnuity = Null<Real>();
}
} // namespace QuantExt
