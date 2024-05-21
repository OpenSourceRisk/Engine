/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file fdmblackscholesmesher.cpp
    \brief 1-d mesher for the Black-Scholes process (in ln(S))
*/

#include <qle/methods/fdmblackscholesmesher.hpp>

#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/methods/finitedifferences/meshers/concentrating1dmesher.hpp>
#include <ql/methods/finitedifferences/meshers/uniform1dmesher.hpp>
#include <ql/methods/finitedifferences/utilities/fdmquantohelper.hpp>
#include <ql/processes/blackscholesprocess.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/yield/quantotermstructure.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>

namespace QuantExt {

FdmBlackScholesMesher::FdmBlackScholesMesher(Size size, const ext::shared_ptr<GeneralizedBlackScholesProcess>& process,
                                             Time maturity, Real strike, Real xMinConstraint, Real xMaxConstraint,
                                             Real eps, Real scaleFactor,
                                             const std::vector<QuantLib::ext::tuple<Real, Real, bool>>& cPoints,
                                             const DividendSchedule& dividendSchedule,
                                             const ext::shared_ptr<FdmQuantoHelper>& fdmQuantoHelper,
                                             Real spotAdjustment)
    : Fdm1dMesher(size) {

    const Real S = process->x0();
    QL_REQUIRE(S > 0.0, "negative or null underlying given");

    std::vector<std::pair<Time, Real>> intermediateSteps;
    for (Size i = 0; i < dividendSchedule.size(); ++i) {
        const Time t = process->time(dividendSchedule[i]->date());
        if (t <= maturity && t >= 0.0)
            intermediateSteps.push_back(
                std::make_pair(process->time(dividendSchedule[i]->date()), dividendSchedule[i]->amount()));
    }

    const Size intermediateTimeSteps = std::max<Size>(2, Size(24.0 * maturity));
    for (Size i = 0; i < intermediateTimeSteps; ++i)
        intermediateSteps.push_back(std::make_pair((i + 1) * (maturity / intermediateTimeSteps), 0.0));

    std::sort(intermediateSteps.begin(), intermediateSteps.end());

    const Handle<YieldTermStructure> rTS = process->riskFreeRate();

    const Handle<YieldTermStructure> qTS =
        (fdmQuantoHelper)
            ? Handle<YieldTermStructure>(ext::make_shared<QuantoTermStructure>(
                  process->dividendYield(), process->riskFreeRate(), Handle<YieldTermStructure>(fdmQuantoHelper->fTS_),
                  process->blackVolatility(), strike, Handle<BlackVolTermStructure>(fdmQuantoHelper->fxVolTS_),
                  fdmQuantoHelper->exchRateATMlevel_, fdmQuantoHelper->equityFxCorrelation_))
            : process->dividendYield();

    Time lastDivTime = 0.0;
    Real fwd = S + spotAdjustment;
    Real mi = fwd, ma = fwd;

    for (Size i = 0; i < intermediateSteps.size(); ++i) {
        const Time divTime = intermediateSteps[i].first;
        const Real divAmount = intermediateSteps[i].second;

        fwd = fwd / rTS->discount(divTime) * rTS->discount(lastDivTime) * qTS->discount(divTime) /
              qTS->discount(lastDivTime);

        mi = std::min(mi, fwd);
        ma = std::max(ma, fwd);

        fwd -= divAmount;

        mi = std::min(mi, fwd);
        ma = std::max(ma, fwd);

        lastDivTime = divTime;
    }

    // Set the grid boundaries, avoid too narrow grid due to vol close to zero (floor vol at 1%)
    const Real normInvEps = InverseCumulativeNormal()(1 - eps);
    const Real sigmaSqrtT =
        std::max(1.0E-2, process->blackVolatility()->blackVol(maturity, strike) * std::sqrt(maturity));

    Real xMin = std::log(mi) - sigmaSqrtT * normInvEps * scaleFactor;
    Real xMax = std::log(ma) + sigmaSqrtT * normInvEps * scaleFactor;

    if (xMinConstraint != Null<Real>()) {
        xMin = xMinConstraint;
    }
    if (xMaxConstraint != Null<Real>()) {
        xMax = xMaxConstraint;
    }

    // filter cPoints on [xmin, xmax]
    std::vector<QuantLib::ext::tuple<Real, Real, bool>> cPointsEff;
    for (auto const& c : cPoints) {
        if (QuantLib::ext::get<0>(c) == Null<Real>() || QuantLib::ext::get<0>(c) < xMin || QuantLib::ext::get<0>(c) > xMax)
            continue;
        cPointsEff.push_back(c);
    }

    ext::shared_ptr<Fdm1dMesher> helper;
    if (cPointsEff.empty()) {
        helper = ext::shared_ptr<Fdm1dMesher>(new Uniform1dMesher(xMin, xMax, size));
    } else if (cPointsEff.size() == 1) {
        helper = ext::shared_ptr<Fdm1dMesher>(new Concentrating1dMesher(
            xMin, xMax, size, std::make_pair(QuantLib::ext::get<0>(cPointsEff[0]), QuantLib::ext::get<1>(cPointsEff[0])),
            QuantLib::ext::get<2>(cPointsEff[0])));
    } else {
        helper = ext::shared_ptr<Fdm1dMesher>(new Concentrating1dMesher(xMin, xMax, size, cPointsEff));
    }

    locations_ = helper->locations();
    for (Size i = 0; i < locations_.size(); ++i) {
        dplus_[i] = helper->dplus(i);
        dminus_[i] = helper->dminus(i);
    }
}

ext::shared_ptr<GeneralizedBlackScholesProcess>
FdmBlackScholesMesher::processHelper(const Handle<Quote>& s0, const Handle<YieldTermStructure>& rTS,
                                     const Handle<YieldTermStructure>& qTS, Volatility vol) {

    return ext::make_shared<GeneralizedBlackScholesProcess>(
        s0, qTS, rTS,
        Handle<BlackVolTermStructure>(ext::shared_ptr<BlackVolTermStructure>(
            new BlackConstantVol(rTS->referenceDate(), Calendar(), vol, rTS->dayCounter()))));
}
} // namespace QuantExt
