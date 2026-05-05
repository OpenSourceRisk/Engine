/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

#include <iostream>
#include <qle/cashflows/commoditycashflow.hpp>
#include <qle/cashflows/commodityindexedaveragecashflow.hpp>
#include <qle/cashflows/commodityindexedcashflow.hpp>
#include <qle/pricingengines/commoditybachelierspreadoptionengine.hpp>
#include <ql/pricingengines/blackformula.hpp>

namespace QuantExt {

CommodityBachelierSpreadOptionAnalyticalEngine::CommodityBachelierSpreadOptionAnalyticalEngine(
    const Handle<YieldTermStructure>& discountCurve, const QuantLib::Handle<QuantLib::BlackVolTermStructure>& normalSpreadVolTS)
    : discountCurve_(discountCurve), normalSpreadVolTS_(normalSpreadVolTS) {
    QL_REQUIRE(!normalSpreadVolTS_.empty(), "normalSpreadVolTS not set");
    registerWith(discountCurve_);
    registerWith(normalSpreadVolTS_);

}

void CommodityBachelierSpreadOptionAnalyticalEngine::calculate() const {
    // Populate some additional results that don't change
    auto& mp = results_.additionalResults;
    QL_REQUIRE(arguments_.exercise->type() == QuantLib::Exercise::European, "Only European Spread Option supported");
    QL_REQUIRE(arguments_.longAssetFlow && arguments_.shortAssetFlow, "flows can not be null");
    Date today = Settings::instance().evaluationDate();
    Date exerciseDate = arguments_.exercise->lastDate();
    Date paymentDate = arguments_.paymentDate;
    if (paymentDate == Date())
        paymentDate = std::max(arguments_.longAssetFlow->date(), arguments_.shortAssetFlow->date());
    QL_REQUIRE(paymentDate >= exerciseDate, "Payment date needs to be on or after exercise date");
    if (exerciseDate < today){
        results_.value = std::max((arguments_.type == Option::Call ? 1.0 : -1.0) *
                                      (arguments_.longAssetFlow->fixing() - arguments_.shortAssetFlow->fixing() -
                                       arguments_.strikePrice),
                                  0.0) *
                         discountCurve_->discount(paymentDate);
        return;
    }     
    double df = discountCurve_->discount(paymentDate);
    Time ttp = discountCurve_->timeFromReference(paymentDate);
    Time tte = discountCurve_->timeFromReference(exerciseDate);
    double forwardLong = arguments_.longAssetFlow->fixing();
    double forwardShort = arguments_.shortAssetFlow->fixing();
    double spread = forwardLong - forwardShort;
    double stdDev = std::sqrt(normalSpreadVolTS_->blackVariance(exerciseDate, arguments_.effectiveStrike));
    mp["tte"] = tte;
    mp["ttp"] = ttp;
    mp["f1"] = forwardLong;
    mp["f2"] = forwardShort;
    mp["spread"] = spread;
    mp["stdDev"] = stdDev;
    mp["df"] = df;
    mp["effectiveStrike"] = arguments_.effectiveStrike;
    mp["absStrike"] = arguments_.strikePrice;
    mp["vol"] = normalSpreadVolTS_->blackVol(exerciseDate, arguments_.effectiveStrike);
    results_.value = arguments_.quantity *  bachelierBlackFormula(arguments_.type, arguments_.strikePrice, spread, stdDev, df);
}

} // namespace QuantExt
