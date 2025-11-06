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

#include <orea/aggregation/dynamiccreditxvacalculator.hpp>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/error_of_mean.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>

using namespace std;
using namespace QuantLib;

using namespace boost::accumulators;

namespace ore {
namespace analytics {

DynamicCreditXvaCalculator::DynamicCreditXvaCalculator(
    //! Driving portfolio consistent with the cube below
    const QuantLib::ext::shared_ptr<Portfolio> portfolio, const QuantLib::ext::shared_ptr<Market> market,
    const string& configuration, const string& baseCurrency, const string& dvaName,
    const string& fvaBorrowingCurve, const string& fvaLendingCurve,
    const bool applyDynamicInitialMargin,
    const QuantLib::ext::shared_ptr<DynamicInitialMarginCalculator> dimCalculator,
    const QuantLib::ext::shared_ptr<NPVCube> tradeExposureCube,
    const QuantLib::ext::shared_ptr<NPVCube> nettingSetExposureCube,
    const QuantLib::ext::shared_ptr<NPVCube>& cptyCube,
    const Size tradeEpeIndex, const Size tradeEneIndex,
    const Size nettingSetEpeIndex, const Size nettingSetEneIndex, const Size cptySpIndex,
    const bool flipViewXVA, const string& flipViewBorrowingCurvePostfix, const string& flipViewLendingCurvePostfix)
    : ValueAdjustmentCalculator(portfolio, market, configuration, baseCurrency, dvaName,
                                fvaBorrowingCurve, fvaLendingCurve, applyDynamicInitialMargin,
                                dimCalculator, tradeExposureCube, nettingSetExposureCube, tradeEpeIndex, tradeEneIndex, 
                                nettingSetEpeIndex, nettingSetEneIndex, 
                                flipViewXVA, flipViewBorrowingCurvePostfix, flipViewLendingCurvePostfix),
      cptyCube_(cptyCube), cptySpIndex_(cptySpIndex) {
    // check consistency of input

    QL_REQUIRE(tradeExposureCube_->numDates() == cptyCube->numDates(),
        "number of dates in tradeExposureCube and cptyCube mismatch ("
        << tradeExposureCube_->numDates() << " vs " << cptyCube->numDates() << ")");

    QL_REQUIRE(cptySpIndex < cptyCube->depth(), "cptySpIndex("
        << cptySpIndex << ") exceeds depth of cptyCube("
        << cptyCube->depth() << ")");
    
    for (Size i = 0; i < tradeExposureCube_->numDates(); i++) {
        QL_REQUIRE(tradeExposureCube_->dates()[i] == cptyCube->dates()[i],
            "date at " << i << " in tradeExposureCube and cptyCube mismatch ("
            << tradeExposureCube_->dates()[i] << " vs " << cptyCube->dates()[i] << ")");
    }
}


const Real DynamicCreditXvaCalculator::calculateCvaIncrement(
    const string& tid, const string& cid, const Date& d0, const Date& d1, const Real& rr) {
    Real increment = 0.0;
    std::size_t cidx = cptyCube_->getTradeIndex(cid);
    std::size_t tidx = tradeExposureCube_->getTradeIndex(tid);
    std::size_t d0idx = d0 == asof() ? 0 : cptyCube_->getDateIndex(d0);
    std::size_t d1idx = cptyCube_->getDateIndex(d1);
    for (Size k = 0; k < tradeExposureCube_->samples(); ++k) {
        Real s0 = d0 == asof() ? 1.0 : cptyCube_->get(cidx, d0idx, k, cptySpIndex_);
        Real s1 = cptyCube_->get(cidx, d1idx, k, cptySpIndex_);
        Real epe = tradeExposureCube_->get(tidx, d1idx, k, tradeEpeIndex_);
        increment += (s0 - s1) * epe;
    }
    return (1.0 - rr) * increment / tradeExposureCube_->samples();
}

const Real DynamicCreditXvaCalculator::calculateDvaIncrement(
    const string& tid, const Date& d0, const Date& d1, const Real& rr) {
    Real increment = 0.0;
    std::size_t dvaidx = cptyCube_->getTradeIndex(dvaName_);
    std::size_t tidx = tradeExposureCube_->getTradeIndex(tid);
    std::size_t d0idx = d0 == asof() ? 0 : cptyCube_->getDateIndex(d0);
    std::size_t d1idx = cptyCube_->getDateIndex(d1);
    for (Size k = 0; k < tradeExposureCube_->samples(); ++k) {
        Real s0 = d0 == asof() ? 1.0 : cptyCube_->get(dvaidx, d0idx, k, cptySpIndex_);
        Real s1 = cptyCube_->get(dvaidx, d1idx, k, cptySpIndex_);
        Real ene = tradeExposureCube_->get(tidx, d1idx, k, tradeEneIndex_);
        increment += (s0 - s1) * ene;
    }
    return (1.0 - rr) * increment / tradeExposureCube_->samples();
}

const Real DynamicCreditXvaCalculator::calculateNettingSetCvaIncrement(
    const string& nid, const string& cid, const Date& d0, const Date& d1, const Real& rr) {
    Real increment = 0.0;
    std::size_t cidx = cptyCube_->getTradeIndex(cid);
    std::size_t nidx = nettingSetExposureCube_->getTradeIndex(nid);
    std::size_t d0idx = d0 == asof() ? 0 : cptyCube_->getDateIndex(d0);
    std::size_t d1idx = cptyCube_->getDateIndex(d1);
    for (Size k = 0; k < nettingSetExposureCube_->samples(); ++k) {
        Real s0 = d0 == asof() ? 1.0 : cptyCube_->get(cidx, d0idx, k, cptySpIndex_);
        Real s1 = cptyCube_->get(cidx, d1idx, k, cptySpIndex_);
        Real epe = nettingSetExposureCube_->get(nidx, d1idx, k, nettingSetEpeIndex_);
        increment += (s0 - s1) * epe;
    }
    return (1.0 - rr) * increment / nettingSetExposureCube_->samples();
}

const Real DynamicCreditXvaCalculator::calculateNettingSetDvaIncrement(
    const string& nid, const Date& d0, const Date& d1, const Real& rr) {
    Real increment = 0.0;
    std::size_t dvaidx = cptyCube_->getTradeIndex(dvaName_);
    std::size_t nidx = nettingSetExposureCube_->getTradeIndex(nid);
    std::size_t d0idx = d0 == asof() ? 0 : cptyCube_->getDateIndex(d0);
    std::size_t d1idx = cptyCube_->getDateIndex(d1);
    for (Size k = 0; k < nettingSetExposureCube_->samples(); ++k) {
        Real s0 = d0 == asof() ? 1.0 : cptyCube_->get(dvaidx, d0idx, k, cptySpIndex_);
        Real s1 = cptyCube_->get(dvaidx, d1idx, k, cptySpIndex_);
        Real ene = nettingSetExposureCube_->get(nidx, d1idx, k, nettingSetEneIndex_);
        increment += (s0 - s1) * ene;
    }
    return (1.0 - rr) * increment / nettingSetExposureCube_->samples();
}

const Real DynamicCreditXvaCalculator::calculateFbaIncrement(
    const string& tid, const string& cid, const string& dvaName,
    const Date& d0, const Date& d1, const Real& dcf) {
    Real increment = 0.0;
    std::size_t dvaidx = dvaName_.empty() ? 0 : cptyCube_->getTradeIndex(dvaName_);
    std::size_t cidx = cid.empty() ? 0 : cptyCube_->getTradeIndex(cid);
    std::size_t tidx = tradeExposureCube_->getTradeIndex(tid);
    std::size_t d0idx = d0 == asof() ? 0 : cptyCube_->getDateIndex(d0);
    std::size_t d1idx = tradeExposureCube_->getDateIndex(d1);
    for (Size k = 0; k < tradeExposureCube_->samples(); ++k) {
        Real s0 = (d0 == asof() || cid.empty()) ? 1.0 : cptyCube_->get(cidx, d0idx, k, cptySpIndex_);
        Real s1 = (d0 == asof() || dvaName.empty()) ? 1.0 : cptyCube_->get(dvaidx, d0idx, k, cptySpIndex_);
        Real ene = tradeExposureCube_->get(tidx, d1idx, k, tradeEneIndex_);
        increment += s0 * s1 * ene;
    }
    return increment * dcf / tradeExposureCube_->samples();
}

const Real DynamicCreditXvaCalculator::calculateFcaIncrement(
    const string& tid, const string& cid, const string& dvaName,
    const Date& d0, const Date& d1, const Real& dcf) {
    Real increment = 0.0;
    std::size_t dvaidx = dvaName_.empty() ? 0 : cptyCube_->getTradeIndex(dvaName_);
    std::size_t cidx = cid.empty() ? 0 : cptyCube_->getTradeIndex(cid);
    std::size_t tidx = tradeExposureCube_->getTradeIndex(tid);
    std::size_t d0idx = d0 == asof() ? 0 : cptyCube_->getDateIndex(d0);
    std::size_t d1idx = tradeExposureCube_->getDateIndex(d1);
    for (Size k = 0; k < tradeExposureCube_->samples(); ++k) {
        Real s0 = (d0 == asof() || cid.empty()) ? 1.0 : cptyCube_->get(cidx, d0idx, k, cptySpIndex_);
        Real s1 = (d0 == asof() || dvaName.empty()) ? 1.0 : cptyCube_->get(dvaidx, d0idx, k, cptySpIndex_);
        Real epe = tradeExposureCube_->get(tidx, d1idx, k, tradeEpeIndex_);
        increment += s0 * s1 * epe;
    }
    return increment * dcf / tradeExposureCube_->samples();
}

const Real DynamicCreditXvaCalculator::calculateNettingSetFbaIncrement(
    const string& nid, const string& cid, const string& dvaName,
    const Date& d0, const Date& d1, const Real& dcf) {
    Real increment = 0.0;
    std::size_t dvaidx = dvaName_.empty() ? 0 : cptyCube_->getTradeIndex(dvaName_);
    std::size_t cidx = cid.empty() ? 0 : cptyCube_->getTradeIndex(cid);
    std::size_t nidx = nettingSetExposureCube_->getTradeIndex(nid);
    std::size_t d0idx = d0 == asof() ? 0 : cptyCube_->getDateIndex(d0);
    std::size_t d1idx = nettingSetExposureCube_->getDateIndex(d1);
    for (Size k = 0; k < nettingSetExposureCube_->samples(); ++k) {
        Real s0 = (d0 == asof() || cid.empty()) ? 1.0 : cptyCube_->get(cidx, d0idx, k, cptySpIndex_);
        Real s1 = (d0 == asof() || dvaName.empty()) ? 1.0 : cptyCube_->get(dvaidx, d0idx, k, cptySpIndex_);
        Real ene = nettingSetExposureCube_->get(nidx, d1idx, k, nettingSetEneIndex_);
        increment += s0 * s1 * ene;
    }
    return increment * dcf / nettingSetExposureCube_->samples();
}

const Real DynamicCreditXvaCalculator::calculateNettingSetFcaIncrement(
    const string& nid, const string& cid, const string& dvaName,
    const Date& d0, const Date& d1, const Real& dcf) {
    Real increment = 0.0;
    std::size_t dvaidx = dvaName_.empty() ? 0 : cptyCube_->getTradeIndex(dvaName_);
    std::size_t cidx = cid.empty() ? 0 : cptyCube_->getTradeIndex(cid);
    std::size_t nidx = nettingSetExposureCube_->getTradeIndex(nid);
    std::size_t d0idx = d0 == asof() ? 0 : cptyCube_->getDateIndex(d0);
    std::size_t d1idx = nettingSetExposureCube_->getDateIndex(d1);
    for (Size k = 0; k < nettingSetExposureCube_->samples(); ++k) {
        Real s0 = (d0 == asof() || cid.empty()) ? 1.0 : cptyCube_->get(cidx, d0idx, k, cptySpIndex_);
        Real s1 = (d0 == asof() || dvaName.empty()) ? 1.0 : cptyCube_->get(dvaidx, d0idx, k, cptySpIndex_);
        Real epe = nettingSetExposureCube_->get(nidx, d1idx, k, nettingSetEpeIndex_);
        increment += s0 * s1 * epe;
    }
    return increment * dcf / nettingSetExposureCube_->samples();
}

const Real DynamicCreditXvaCalculator::calculateNettingSetMvaIncrement(
    const string& nid, const string& cid, const Date& d0, const Date& d1, const Real& dcf) {
    Real increment = 0.0;
    std::size_t dvaidx = dvaName_.empty() ? 0 : cptyCube_->getTradeIndex(dvaName_);
    std::size_t cidx = cid.empty() ? 0 : cptyCube_->getTradeIndex(cid);
    std::size_t nidx = nettingSetExposureCube_->getTradeIndex(nid);
    std::size_t d0idx = d0 == asof() ? 0 : cptyCube_->getDateIndex(d0);
    std::size_t d1idx = dimCalculator_->dimCube()->getDateIndex(d1);
    for (Size k = 0; k < nettingSetExposureCube_->samples(); ++k) {
        Real s0 = (d0 == asof() || cid.empty()) ? 1.0 : cptyCube_->get(cidx, d0idx, k, cptySpIndex_);
        Real s1 = (d0 == asof() || dvaName_.empty()) ? 1.0 : cptyCube_->get(dvaidx, d0idx, k, cptySpIndex_);
        Real im = dimCalculator_->dimCube()->get(nidx, d1idx, k);
        increment += s0 * s1 * im;
    }
    return increment * dcf / nettingSetExposureCube_->samples();
}

} // namespace analytics
} // namespace ore
