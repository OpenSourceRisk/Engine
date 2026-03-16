/*
 Copyright (C) 2005, 2006 Theo Boafo
 Copyright (C) 2006, 2007 StatPro Italia srl
 Copyright (C) 2020 Quaternion Risk Managment Ltd

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

#include <qle/pricingengines/binomialconvertibleengine.hpp>

#include <ql/methods/lattices/binomialtree.hpp>

namespace QuantExt {

template <class T>
BinomialConvertibleEngine<T>::BinomialConvertibleEngine(
    const QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>& process, const Handle<YieldTermStructure>& referenceCurve,
    const Handle<Quote>& creditSpread, const Handle<DefaultProbabilityTermStructure>& defaultCurve,
    const Handle<Quote>& recoveryRate, Size timeSteps)
    : process_(process), referenceCurve_(referenceCurve), creditSpread_(creditSpread), defaultCurve_(defaultCurve),
      recoveryRate_(recoveryRate), timeSteps_(timeSteps) {

    QL_REQUIRE(timeSteps > 0, "timeSteps must be positive, " << timeSteps << " not allowed");
    registerWith(process_);
    registerWith(referenceCurve_);
    registerWith(creditSpread_);
    registerWith(defaultCurve_);
}

template <class T> void BinomialConvertibleEngine<T>::calculate() const {

    DayCounter rfdc = process_->riskFreeRate()->dayCounter();
    DayCounter divdc = process_->dividendYield()->dayCounter();
    DayCounter voldc = process_->blackVolatility()->dayCounter();
    Calendar volcal = process_->blackVolatility()->calendar();

    Real s0 = process_->x0();
    QL_REQUIRE(s0 > 0.0, "negative or null underlying");
    Volatility v = process_->blackVolatility()->blackVol(arguments_.maturityDate, s0);
    Date maturityDate = arguments_.maturityDate;
    Rate riskFreeRate = process_->riskFreeRate()->zeroRate(maturityDate, rfdc, Continuous, NoFrequency);
    Rate q = process_->dividendYield()->zeroRate(maturityDate, divdc, Continuous, NoFrequency);
    Date referenceDate = process_->riskFreeRate()->referenceDate();

    // subtract dividends
    Size i;
    for (i = 0; i < arguments_.dividends.size(); i++) {
        if (arguments_.dividends[i]->date() >= referenceDate)
            s0 -=
                arguments_.dividends[i]->amount() * process_->riskFreeRate()->discount(arguments_.dividends[i]->date());
    }
    QL_REQUIRE(s0 > 0.0, "negative value after subtracting dividends");

    results_.additionalResults["securitySpread"] = creditSpread_.empty() ? 0.0 : creditSpread_->value();
    results_.additionalResults["maturityTime"] = process_->riskFreeRate()->timeFromReference(maturityDate);
    results_.additionalResults["riskFreeRate"] = riskFreeRate;
    results_.additionalResults["dividendYield"] = q;
    results_.additionalResults["equitySpot"] = s0;
    results_.additionalResults["equityVol"] = v;
    if (maturityDate > referenceCurve_->referenceDate()) {
        results_.additionalResults["maturityDiscountFactor"] = referenceCurve_->discount(maturityDate);
        results_.additionalResults["maturitySurvivalProbability"] =
            defaultCurve_.empty() ? 1.0 : defaultCurve_->survivalProbability(maturityDate);
    }

    // binomial trees with constant coefficient
    Handle<Quote> underlying(QuantLib::ext::shared_ptr<Quote>(new SimpleQuote(s0)));
    Handle<YieldTermStructure> flatRiskFree(
        QuantLib::ext::shared_ptr<YieldTermStructure>(new FlatForward(referenceDate, riskFreeRate, rfdc)));
    Handle<YieldTermStructure> flatDividends(
        QuantLib::ext::shared_ptr<YieldTermStructure>(new FlatForward(referenceDate, q, divdc)));
    Handle<BlackVolTermStructure> flatVol(
        QuantLib::ext::shared_ptr<BlackVolTermStructure>(new BlackConstantVol(referenceDate, volcal, v, voldc)));

    QuantLib::ext::shared_ptr<PlainVanillaPayoff> payoff = QuantLib::ext::dynamic_pointer_cast<PlainVanillaPayoff>(arguments_.payoff);
    QL_REQUIRE(payoff, "non-plain payoff given");

    Time maturity = rfdc.yearFraction(arguments_.settlementDate, maturityDate);

    QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess> bs(
        new GeneralizedBlackScholesProcess(underlying, flatDividends, flatRiskFree, flatVol));
    QuantLib::ext::shared_ptr<T> tree(new T(bs, maturity, timeSteps_, payoff->strike()));

    // the lattice uses the eq process risk free rate, we use a credit spread over this rate which comprises
    // - the credit spread itself
    // - the spread between the eq risk free rate and the bond reference curve rate
    // - an effective spread from the default curve taking into account the recovery via a first order approximation

    QL_REQUIRE(!referenceCurve_.empty(), "BinomialConvertibleEngine::calculate(): empty reference curve");
    Real referenceCurveRate = referenceCurve_->zeroRate(maturityDate, rfdc, Continuous, NoFrequency);
    Real defaultRate = 0.0, creditRate = 0.0, recRate = 0.0;
    if (!defaultCurve_.empty()) {
        // default curve is optional
        defaultRate = -std::log(defaultCurve_->survivalProbability(maturityDate)) /
                      rfdc.yearFraction(defaultCurve_->referenceDate(), maturityDate);
    }
    if (!creditSpread_.empty()) {
        // credit spread is optional
        creditRate = creditSpread_->value();
    }
    if (!recoveryRate_.empty()) {
        // recovery rate is optional
        recRate = recoveryRate_->value();
    }

    Real creditSpread = creditRate + (referenceCurveRate - riskFreeRate) + defaultRate * (1.0 - recRate);

    QuantLib::ext::shared_ptr<Lattice> lattice(
        new TsiveriotisFernandesLattice<T>(tree, riskFreeRate, maturity, timeSteps_, creditSpread, v, q));

    QuantExt::DiscretizedConvertible convertible(
        arguments_, bs, Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(creditSpread)), TimeGrid(maturity, timeSteps_));

    convertible.initialize(lattice, maturity);
    convertible.rollback(0.0);
    results_.value = convertible.presentValue();
    QL_ENSURE(results_.value < std::numeric_limits<Real>::max(), "floating-point overflow on tree grid");
}

// template instantiation, add here if you want to use others

template class BinomialConvertibleEngine<CoxRossRubinstein>;

} // namespace QuantExt
