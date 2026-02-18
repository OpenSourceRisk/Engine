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

#include <qle/models/crossassetanalytics.hpp>
#include <qle/pricingengines/analyticdkcpicapfloorengine.hpp>
#include <qle/utilities/inflation.hpp>
#include <ql/pricingengines/blackformula.hpp>

namespace QuantExt {

using namespace CrossAssetAnalytics;

AnalyticDkCpiCapFloorEngine::AnalyticDkCpiCapFloorEngine(const QuantLib::ext::shared_ptr<CrossAssetModel>& model,
                                                         const Size index, const Real baseCPI)
    : model_(model), index_(index) {}

void AnalyticDkCpiCapFloorEngine::calculate() const {

    bool interpolate = arguments_.observationInterpolation == CPI::Linear;

    //Date startDate = inflationPeriod(arguments_.startDate - arguments_.observationLag, arguments_.index->frequency()).first;
    auto zits = model_->infdk(index_)->termStructure();
    Size irIdx = model_->ccyIndex(model_->infdk(index_)->currency());
    auto yts = model_->irlgm1f(irIdx)->termStructure();
    
    Date fixingDate = inflationPeriod(arguments_.fixDate, arguments_.index->frequency()).first;

    Date baseDate = inflationPeriod(arguments_.startDate - arguments_.observationLag, arguments_.index->frequency()).first;

    Real t_strike = inflationYearFraction(arguments_.index->frequency(), interpolate,
                                   zits->dayCounter(),
                                   baseDate, fixingDate);
    
    Real t = yts->dayCounter().yearFraction(yts->referenceDate(), fixingDate + simulationLag(zits));

    
    if (t_strike <= 0.0) {
        // option is expired, we do not value any possibly non settled
        // flows, i.e. set the npv to zero in this case
        results_.value = 0.0;
        return;
    }
    Real baseCPI_t0 = arguments_.index->fixing(zits->baseDate());
    Real t_curve = zits->dayCounter().yearFraction(zits->baseDate(), fixingDate);
    // Book, 13.37, 13.38

    Real k = std::pow(1.0 + arguments_.strike, t_strike);
    Real kTilde = k * arguments_.baseCPI;
    Real nTilde = arguments_.nominal / arguments_.baseCPI;

    std::cout << "AnalyticDkCpiCapFloorEngine: arguments_.startDate = " << arguments_.startDate << "\n"
              << "arguments_.fixDate = " << arguments_.fixDate << "\n"
              << "fixingDate = " << fixingDate << "\n"
              << "baseDate = " << baseDate << "\n"
              << "t_strike = " << t_strike << "\n"
              << "t = " << t << "\n"
              << "t_curve = " << t_curve << "\n"
              << "k = " << k << "\n"
              << "kTilde = " << kTilde << "\n"
              << "nTilde = " << nTilde << "\n"
                << "baseCPI_t0 = " << baseCPI_t0 << "\n"
                <<" baseCPI = " << arguments_.baseCPI << "\n"
                << "zits->zeroRate(arguments_.fixDate) = " << zits->zeroRate(arguments_.fixDate) << "\n";

              

    Real m = baseCPI_t0 *
             std::pow(1.0 + zits->zeroRate(arguments_.fixDate), t_curve);
    std::cout << "baseCPI_t0 = " << baseCPI_t0 << "\n"
              << "zits->zeroRate(arguments_.fixDate) = " << zits->zeroRate(arguments_.fixDate) << "\n"
              << "m = " << m << "\n";
    m = ZeroInflation::cpiFixing(arguments_.index, arguments_.fixDate, 0*Days, interpolate);
    std::cout << "m (from index fixing) = " << m << "\n";
    Real Ht = Hy(index_).eval(*model_, t);
    Real v = Ht * Ht * zetay(index_).eval(*model_, t) -
             2.0 * Ht * integral(*model_, P(Hy(index_), ay(index_), ay(index_)), 0.0, t) +
             integral(*model_, P(Hy(index_), Hy(index_), ay(index_), ay(index_)), 0.0, t);

    Real discount = yts->discount(arguments_.payDate);

    results_.value = nTilde * blackFormula(arguments_.type, kTilde, m, std::sqrt(v), discount);
    std::cout << "v = " << v << "\n"
              << "discount = " << discount << "\n"
              << "results_.value = " << results_.value << "\n";

} // calculate()

} // namespace QuantExt
