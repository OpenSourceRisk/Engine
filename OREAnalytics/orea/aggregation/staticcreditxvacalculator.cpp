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

#include <orea/aggregation/staticcreditxvacalculator.hpp>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/error_of_mean.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>

using namespace std;
using namespace QuantLib;

using namespace boost::accumulators;

namespace ore {
namespace analytics {

StaticCreditXvaCalculator::StaticCreditXvaCalculator(
    const QuantLib::ext::shared_ptr<Portfolio> portfolio, const QuantLib::ext::shared_ptr<Market> market,
    const string& configuration, const string& baseCurrency, const string& dvaName,
    const string& fvaBorrowingCurve, const string& fvaLendingCurve,
    const bool applyDynamicInitialMargin,
    const QuantLib::ext::shared_ptr<DynamicInitialMarginCalculator> dimCalculator,
    const QuantLib::ext::shared_ptr<NPVCube> tradeExposureCube,
    const QuantLib::ext::shared_ptr<NPVCube> nettingSetExposureCube,
    const Size tradeEpeIndex, const Size tradeEneIndex, 
    const Size nettingSetEpeIndex, const Size nettingSetEneIndex,
    const bool flipViewXVA, const string& flipViewBorrowingCurvePostfix, const string& flipViewLendingCurvePostfix)
    : ValueAdjustmentCalculator(portfolio, market, configuration, baseCurrency, dvaName,
                                fvaBorrowingCurve, fvaLendingCurve, applyDynamicInitialMargin,
                                dimCalculator, tradeExposureCube, nettingSetExposureCube, tradeEpeIndex, tradeEneIndex, 
                                nettingSetEpeIndex, nettingSetEneIndex, 
                                flipViewXVA, flipViewBorrowingCurvePostfix, flipViewLendingCurvePostfix) {
    for (Size i = 0; i < dates().size(); i++) {
        dateIndexMap_.emplace(dates()[i], i);
    }
}


const Real StaticCreditXvaCalculator::calculateCvaIncrement(
    const string& tid, const string& cid, const Date& d0, const Date& d1, const Real& rr) {
    Handle<DefaultProbabilityTermStructure> dts = market_->defaultCurve(cid, configuration_)->curve();
    QL_REQUIRE(!dts.empty(), "Default curve missing for counterparty " << cid);
    Real increment = 0.0;
    Real s0 = dts->survivalProbability(d0);
    Real s1 = dts->survivalProbability(d1);
    Real epe = tradeExposureCube_->get(tid, d1, 0, tradeEpeIndex_);
    increment = (1.0 - rr) * (s0 - s1) * epe;
    return increment;
}

const Real StaticCreditXvaCalculator::calculateDvaIncrement(
    const string& tid, const Date& d0, const Date& d1, const Real& rr) {
    Handle<DefaultProbabilityTermStructure> dts = market_->defaultCurve(dvaName_, configuration_)->curve();
    QL_REQUIRE(!dts.empty(), "Default curve missing for dvaName " << dvaName_);
    Real increment = 0.0;
    Real s0 = dts->survivalProbability(d0);
    Real s1 = dts->survivalProbability(d1);
    Real ene = tradeExposureCube_->get(tid, d1, 0, tradeEneIndex_);
    increment = (1.0 - rr) * (s0 - s1) * ene;
    return increment;
}

const Real StaticCreditXvaCalculator::calculateNettingSetCvaIncrement(
    const string& nid, const string& cid, const Date& d0, const Date& d1, const Real& rr) {
    Handle<DefaultProbabilityTermStructure> dts = market_->defaultCurve(cid, configuration_)->curve();
    QL_REQUIRE(!dts.empty(), "Default curve missing for counterparty " << cid);
    Real increment = 0.0;
    Real s0 = dts->survivalProbability(d0);
    Real s1 = dts->survivalProbability(d1);
    Real epe = nettingSetExposureCube_->get(nid, d1, 0, nettingSetEpeIndex_);
    increment = (1.0 - rr) * (s0 - s1) * epe;
    return increment;
}

const Real StaticCreditXvaCalculator::calculateNettingSetDvaIncrement(
    const string& nid, const Date& d0, const Date& d1, const Real& rr) {
    Handle<DefaultProbabilityTermStructure> dts = market_->defaultCurve(dvaName_, configuration_)->curve();
    QL_REQUIRE(!dts.empty(), "Default curve missing for dvaName " << dvaName_);
    Real increment = 0.0;
    Real s0 = dts->survivalProbability(d0);
    Real s1 = dts->survivalProbability(d1);
    Real ene = nettingSetExposureCube_->get(nid, d1, 0, nettingSetEneIndex_);
    increment = (1.0 - rr) * (s0 - s1) * ene;
    return increment;
}

const Real StaticCreditXvaCalculator::calculateFbaIncrement(
    const string& tid, const string& cid, const string& dvaName,
    const Date& d0, const Date& d1, const Real& dcf) {
    Handle<DefaultProbabilityTermStructure> dts_cid;
    Handle<DefaultProbabilityTermStructure> dts_dvaName;
    if (cid != "") {
        dts_cid = market_->defaultCurve(cid, configuration_)->curve();
        QL_REQUIRE(!dts_cid.empty(), "Default curve missing for counterparty " << cid);
    }
    if (dvaName != "") {
        dts_dvaName = market_->defaultCurve(dvaName, configuration_)->curve();
        QL_REQUIRE(!dts_dvaName.empty(), "Default curve missing for dvaName " << dvaName);
    }

    Real increment = 0.0;
    Real s0 = cid == "" ? 1.0 : dts_cid->survivalProbability(d0);
    Real s1 = dvaName == "" ? 1.0 : dts_dvaName->survivalProbability(d0);
    Real ene = tradeExposureCube_->get(tid, d1, 0, tradeEneIndex_);
    increment = s0 * s1 * ene * dcf;
    return increment;
}

const Real StaticCreditXvaCalculator::calculateFcaIncrement(
    const string& tid, const string& cid, const string& dvaName,
    const Date& d0, const Date& d1, const Real& dcf) {
    Handle<DefaultProbabilityTermStructure> dts_cid;
    Handle<DefaultProbabilityTermStructure> dts_dvaName;
    if (cid != "") {
        dts_cid = market_->defaultCurve(cid, configuration_)->curve();
        QL_REQUIRE(!dts_cid.empty(), "Default curve missing for counterparty " << cid);
    }
    if (dvaName != "") {
        dts_dvaName = market_->defaultCurve(dvaName, configuration_)->curve();
        QL_REQUIRE(!dts_dvaName.empty(), "Default curve missing for dvaName " << dvaName);
    }

    Real increment = 0.0;
    Real s0 = cid == "" ? 1.0 : dts_cid->survivalProbability(d0);
    Real s1 = dvaName == "" ? 1.0 : dts_dvaName->survivalProbability(d0);
    Real epe = tradeExposureCube_->get(tid, d1, 0, tradeEpeIndex_);
    increment = s0 * s1 * epe * dcf;
    return increment;
}

const Real StaticCreditXvaCalculator::calculateNettingSetFbaIncrement(
    const string& nid, const string& cid, const string& dvaName,
    const Date& d0, const Date& d1, const Real& dcf) {
    Handle<DefaultProbabilityTermStructure> dts_cid;
    Handle<DefaultProbabilityTermStructure> dts_dvaName;
    if (cid != "") {
        dts_cid = market_->defaultCurve(cid, configuration_)->curve();
        QL_REQUIRE(!dts_cid.empty(), "Default curve missing for counterparty " << cid);
    }
    if (dvaName != "") {
        dts_dvaName = market_->defaultCurve(dvaName, configuration_)->curve();
        QL_REQUIRE(!dts_dvaName.empty(), "Default curve missing for dvaName " << dvaName);
    }

    Real increment = 0.0;
    Real s0 = cid == "" ? 1.0 : dts_cid->survivalProbability(d0);
    Real s1 = dvaName == "" ? 1.0 : dts_dvaName->survivalProbability(d0);
    Real ene = nettingSetExposureCube_->get(nid, d1, 0, nettingSetEneIndex_);
    increment = s0 * s1 * ene * dcf;
    return increment;
}

const Real StaticCreditXvaCalculator::calculateNettingSetFcaIncrement(
    const string& nid, const string& cid, const string& dvaName,
    const Date& d0, const Date& d1, const Real& dcf) {
    Handle<DefaultProbabilityTermStructure> dts_cid;
    Handle<DefaultProbabilityTermStructure> dts_dvaName;
    if (cid != "") {
        dts_cid = market_->defaultCurve(cid, configuration_)->curve();
        QL_REQUIRE(!dts_cid.empty(), "Default curve missing for counterparty " << cid);
    }
    if (dvaName != "") {
        dts_dvaName = market_->defaultCurve(dvaName, configuration_)->curve();
        QL_REQUIRE(!dts_dvaName.empty(), "Default curve missing for dvaName " << dvaName);
    }

    Real increment = 0.0;
    Real s0 = cid == "" ? 1.0 : dts_cid->survivalProbability(d0);
    Real s1 = dvaName == "" ? 1.0 : dts_dvaName->survivalProbability(d0);
    Real epe = nettingSetExposureCube_->get(nid, d1, 0, nettingSetEpeIndex_);
    increment = s0 * s1 * epe * dcf;
    return increment;
}

const Real StaticCreditXvaCalculator::calculateNettingSetMvaIncrement(
    const string& nid, const string& cid, const Date& d0, const Date& d1, const Real& dcf) {
    Handle<DefaultProbabilityTermStructure> dts_cid = market_->defaultCurve(cid, configuration_)->curve();
    QL_REQUIRE(cid == "" || !dts_cid.empty(), "Default curve missing for counterparty " << cid);
    Handle<DefaultProbabilityTermStructure> dts_dvaName = market_->defaultCurve(dvaName_, configuration_)->curve();
    QL_REQUIRE(dvaName_ == "" || !dts_dvaName.empty(), "Default curve missing for dvaName " << dvaName_);

    Real increment = 0.0;
    Real s0 = cid == "" ? 1.0 : dts_cid->survivalProbability(d0);
    Real s1 = dvaName_ == "" ? 1.0 : dts_dvaName->survivalProbability(d0);
    increment = s0 * s1 * dimCalculator_->expectedIM(nid)[dateIndexMap_[d1]] * dcf;
    return increment;
}

} // namespace analytics
} // namespace ore
