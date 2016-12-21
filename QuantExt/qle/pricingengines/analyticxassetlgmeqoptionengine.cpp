/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

#include <qle/pricingengines/analyticXASSETlgmEQoptionengine.hpp>
#include <qle/models/crossassetanalytics.hpp>

#include <ql/pricingengines/blackcalculator.hpp>

namespace QuantExt {

using namespace CrossAssetAnalytics;

AnalyticXAssetLgmEquityOptionEngine::AnalyticXAssetLgmEquityOptionEngine(
    const boost::shared_ptr<CrossAssetModel>& model,
    const Size eqName,
    const Size EqCcy)
    : model_(model), eqIdx_(eqName), ccyIdx_(EqCcy), 
      cacheEnabled_(false), cacheDirty_(true) {}

Real AnalyticXAssetLgmEquityOptionEngine::value(
    const Time t0, const Time t, 
    const boost::shared_ptr<StrikedTypePayoff> payoff,
    const Real discount, const Real eqForward) const {

    QL_FAIL("AnalyticXAssetLgmEquityOptionEngine not yet implemented");
}

void AnalyticXAssetLgmEquityOptionEngine::calculate() const {

    QL_REQUIRE(arguments_.exercise->type() == Exercise::European, "only European options are allowed");

    boost::shared_ptr<StrikedTypePayoff> payoff = boost::dynamic_pointer_cast<StrikedTypePayoff>(arguments_.payoff);
    QL_REQUIRE(payoff != NULL, "only striked payoff is allowed");

    Date expiry = arguments_.exercise->lastDate();

    QL_FAIL("AnalyticXAssetLgmEquityOptionEngine::calculate() not yet implemented");

} // calculate()

} // namespace QuantExt
