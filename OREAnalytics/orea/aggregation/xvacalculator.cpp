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

#include <orea/aggregation/xvacalculator.hpp>
#include <orea/app/structuredanalyticserror.hpp>

#include <ored/portfolio/trade.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/vectorutils.hpp>

#include <ql/errors.hpp>

using namespace std;
using namespace QuantLib;

// using namespace boost::accumulators;

namespace ore {
namespace analytics {

ValueAdjustmentCalculator::ValueAdjustmentCalculator(
    const QuantLib::ext::shared_ptr<Portfolio> portfolio,
    const QuantLib::ext::shared_ptr<Market> market,
    const string& configuration,
    const string& baseCurrency,
    const string& dvaName,
    const string& fvaBorrowingCurve,
    const string& fvaLendingCurve,
    const bool applyDynamicInitialMargin,
    const QuantLib::ext::shared_ptr<DynamicInitialMarginCalculator> dimCalculator,
    const QuantLib::ext::shared_ptr<NPVCube> tradeExposureCube,
    const QuantLib::ext::shared_ptr<NPVCube> nettingSetExposureCube,
    const Size tradeEpeIndex, const Size tradeEneIndex,
    const Size nettingSetEpeIndex, const Size nettingSetEneIndex, 
    const bool flipViewXVA, const string& flipViewBorrowingCurvePostfix, const string& flipViewLendingCurvePostfix)
    : portfolio_(portfolio), market_(market), configuration_(configuration),
      baseCurrency_(baseCurrency), dvaName_(dvaName),
      fvaBorrowingCurve_(fvaBorrowingCurve), fvaLendingCurve_(fvaLendingCurve),
      applyDynamicInitialMargin_(applyDynamicInitialMargin),
      dimCalculator_(dimCalculator),
      tradeExposureCube_(tradeExposureCube),
      nettingSetExposureCube_(nettingSetExposureCube),
      tradeEpeIndex_(tradeEpeIndex), tradeEneIndex_(tradeEneIndex), nettingSetEpeIndex_(nettingSetEpeIndex),
      nettingSetEneIndex_(nettingSetEneIndex), flipViewXVA_(flipViewXVA),
      flipViewBorrowingCurvePostfix_(flipViewBorrowingCurvePostfix), flipViewLendingCurvePostfix_(flipViewLendingCurvePostfix) {

    QL_REQUIRE(portfolio_, "portfolio is null");

    for (const auto& [tradeId, trade] : portfolio_->trades()) {
        string nettingSetId = trade->envelope().nettingSetId();
        if (nettingSetCpty_.find(nettingSetId) == nettingSetCpty_.end()) {
            nettingSetCpty_[nettingSetId] = trade->envelope().counterparty();
        }
    }

    

    QL_REQUIRE(tradeExposureCube_->numIds() == portfolio_->trades().size(),
        "number of trades in tradeExposureCube and portfolio mismatch ("
        << tradeExposureCube_->numIds() << " vs " << portfolio_->trades().size() << ")");

    QL_REQUIRE(nettingSetExposureCube_->numIds() == nettingSetCpty_.size(),
        "number of netting sets in nettingSetExposureCube and nettingSetCpty map mismatch ("
        << nettingSetExposureCube_->numIds() << " vs " << nettingSetCpty_.size() << ")");

    QL_REQUIRE(tradeExposureCube_->numDates() == nettingSetExposureCube_->numDates(),
        "number of dates in tradeExposureCube and nettingSetExposureCube mismatch ("
        << tradeExposureCube_->numDates() << " vs " << nettingSetExposureCube_->numDates() << ")");

    for (Size i = 0; i < tradeExposureCube_->numDates(); i++) {
        QL_REQUIRE(tradeExposureCube_->dates()[i] == nettingSetExposureCube_->dates()[i],
            "date at " << i << " in tradeExposureCube and nettingSetExposureCube mismatch ("
            << tradeExposureCube_->dates()[i] << " vs " << nettingSetExposureCube_->dates()[i] << ")");
    }

    QL_REQUIRE(tradeEpeIndex < tradeExposureCube_->depth(), "tradeEpeIndex("
        << tradeEpeIndex << ") exceeds depth of tradeExposureCube("
        << tradeExposureCube_->depth() << ")");

    QL_REQUIRE(tradeEneIndex < tradeExposureCube_->depth(), "tradeEneIndex("
        << tradeEneIndex << ") exceeds depth of tradeExposureCube("
        << tradeExposureCube_->depth() << ")");

    QL_REQUIRE(nettingSetEpeIndex < nettingSetExposureCube_->depth(), "nettingSetEpeIndex("
        << nettingSetEpeIndex << ") exceeds depth of nettingSetExposureCube("
        << nettingSetExposureCube_->depth() << ")");

    QL_REQUIRE(nettingSetEneIndex < nettingSetExposureCube_->depth(), "nettingSetEneIndex("
        << nettingSetEneIndex << ") exceeds depth of nettingSetExposureCube("
        << nettingSetExposureCube_->depth() << ")");
    
}

const map<string, Real>& ValueAdjustmentCalculator::tradeCva() {
    return tradeCva_;
}

const map<string, Real>& ValueAdjustmentCalculator::tradeDva() {
    return tradeDva_;
}

const map<string, Real>& ValueAdjustmentCalculator::nettingSetCva() {
    return nettingSetCva_;
}

const map<string, Real>& ValueAdjustmentCalculator::nettingSetDva() {
    return nettingSetDva_;
}

const map<string, Real>& ValueAdjustmentCalculator::nettingSetSumCva() {
    return nettingSetSumCva_;
}

const map<string, Real>& ValueAdjustmentCalculator::nettingSetSumDva() {
    return nettingSetSumDva_;
}

const Real& ValueAdjustmentCalculator::tradeCva(const std::string& trade) {
    if (tradeCva_.find(trade) != tradeCva_.end())
        return tradeCva_[trade];
    else
        QL_FAIL("trade " << trade << " not found in expected CVA results");
}

const Real& ValueAdjustmentCalculator::tradeDva(const std::string& trade) {
    if (tradeDva_.find(trade) != tradeDva_.end())
        return tradeDva_[trade];
    else
        QL_FAIL("trade " << trade << " not found in expected DVA results");
}

const Real& ValueAdjustmentCalculator::tradeFba(const std::string& trade) {
    if (tradeFba_.find(trade) != tradeFba_.end())
        return tradeFba_[trade];
    else
        QL_FAIL("trade " << trade << " not found in expected FBA results");
}

const Real& ValueAdjustmentCalculator::tradeFba_exOwnSp(const std::string& trade) {
    if (tradeFba_exOwnSp_.find(trade) != tradeFba_exOwnSp_.end())
        return tradeFba_exOwnSp_[trade];
    else
        QL_FAIL("trade " << trade << " not found in expected FBA ex own sp results");
}

const Real& ValueAdjustmentCalculator::tradeFba_exAllSp(const std::string& trade) {
    if (tradeFba_exAllSp_.find(trade) != tradeFba_exAllSp_.end())
        return tradeFba_exAllSp_[trade];
    else
        QL_FAIL("trade " << trade << " not found in expected FBA ex all sp results");
}

const Real& ValueAdjustmentCalculator::tradeFca(const std::string& trade) {
    if (tradeFca_.find(trade) != tradeFca_.end())
        return tradeFca_[trade];
    else
        QL_FAIL("trade " << trade << " not found in expected FCA results");
}

const Real& ValueAdjustmentCalculator::tradeFca_exOwnSp(const std::string& trade) {
    if (tradeFca_exOwnSp_.find(trade) != tradeFca_exOwnSp_.end())
        return tradeFca_exOwnSp_[trade];
    else
        QL_FAIL("trade " << trade << " not found in expected FCA ex own sp results");
}

const Real& ValueAdjustmentCalculator::tradeFca_exAllSp(const std::string& trade) {
    if (tradeFca_exAllSp_.find(trade) != tradeFca_exAllSp_.end())
        return tradeFca_exAllSp_[trade];
    else
        QL_FAIL("trade " << trade << " not found in expected FCA ex all sp results");
}

const Real& ValueAdjustmentCalculator::tradeMva(const std::string& trade) {
    if (tradeMva_.find(trade) != tradeMva_.end())
        return tradeMva_[trade];
    else
        QL_FAIL("trade " << trade << " not found in expected MVA results");
}

const Real& ValueAdjustmentCalculator::nettingSetSumCva(const std::string& nettingSet) {
    if (nettingSetSumCva_.find(nettingSet) != nettingSetSumCva_.end())
        return nettingSetSumCva_[nettingSet];
    else
        QL_FAIL("netting set " << nettingSet << " not found in expected CVA results");
}

const Real& ValueAdjustmentCalculator::nettingSetSumDva(const std::string& nettingSet) {
    if (nettingSetSumDva_.find(nettingSet) != nettingSetSumDva_.end())
        return nettingSetSumDva_[nettingSet];
    else
        QL_FAIL("netting set " << nettingSet << " not found in expected DVA results");
}

const Real& ValueAdjustmentCalculator::nettingSetCva(const std::string& nettingSet) {
    if (nettingSetCva_.find(nettingSet) != nettingSetCva_.end())
        return nettingSetCva_[nettingSet];
    else
        QL_FAIL("netting set " << nettingSet << " not found in expected CVA results");
}

const Real& ValueAdjustmentCalculator::nettingSetDva(const std::string& nettingSet) {
    if (nettingSetDva_.find(nettingSet) != nettingSetDva_.end())
        return nettingSetDva_[nettingSet];
    else
        QL_FAIL("netting set " << nettingSet << " not found in expected DVA results");
}

const Real& ValueAdjustmentCalculator::nettingSetFba(const std::string& nettingSet) {
    if (nettingSetFba_.find(nettingSet) != nettingSetFba_.end())
        return nettingSetFba_[nettingSet];
    else
        QL_FAIL("netting set " << nettingSet << " not found in expected FBA results");
}

const Real& ValueAdjustmentCalculator::nettingSetFba_exOwnSp(const std::string& nettingSet) {
    if (nettingSetFba_exOwnSp_.find(nettingSet) != nettingSetFba_exOwnSp_.end())
        return nettingSetFba_exOwnSp_[nettingSet];
    else
        QL_FAIL("netting set " << nettingSet << " not found in expected FBA ex own sp results");
}

const Real& ValueAdjustmentCalculator::nettingSetFba_exAllSp(const std::string& nettingSet) {
    if (nettingSetFba_exAllSp_.find(nettingSet) != nettingSetFba_exAllSp_.end())
        return nettingSetFba_exAllSp_[nettingSet];
    else
        QL_FAIL("netting set " << nettingSet << " not found in expected FBA ex all sp results");
}

const Real& ValueAdjustmentCalculator::nettingSetFca(const std::string& nettingSet) {
    if (nettingSetFca_.find(nettingSet) != nettingSetFca_.end())
        return nettingSetFca_[nettingSet];
    else
        QL_FAIL("netting set " << nettingSet << " not found in expected FBA results");
}

const Real& ValueAdjustmentCalculator::nettingSetFca_exOwnSp(const std::string& nettingSet) {
    if (nettingSetFca_exOwnSp_.find(nettingSet) != nettingSetFca_exOwnSp_.end())
        return nettingSetFca_exOwnSp_[nettingSet];
    else
        QL_FAIL("netting set " << nettingSet << " not found in expected FBA ex own sp results");
}

const Real& ValueAdjustmentCalculator::nettingSetFca_exAllSp(const std::string& nettingSet) {
    if (nettingSetFca_exAllSp_.find(nettingSet) != nettingSetFca_exAllSp_.end())
        return nettingSetFca_exAllSp_[nettingSet];
    else
        QL_FAIL("netting set " << nettingSet << " not found in expected FBA ex all sp results");
}

const Real& ValueAdjustmentCalculator::nettingSetMva(const std::string& nettingSet) {
    if (nettingSetMva_.find(nettingSet) != nettingSetMva_.end())
        return nettingSetMva_[nettingSet];
    else
        QL_FAIL("netting set " << nettingSet << " not found in expected MVA results");
}

void ValueAdjustmentCalculator::build() {
    const auto& numDates = dates().size();
    const auto& today = asof();

    Handle<YieldTermStructure> borrowingCurve, lendingCurve, oisCurve;
    if (baseCurrency_ != "")
        oisCurve = market_->discountCurve(baseCurrency_, configuration_);

    string origDvaName = dvaName_;
    // Trade XVA
    for (const auto& [tid, trade] : portfolio_->trades()) {
        try {
            LOG("Update XVA for trade " << tid
                                        << (flipViewXVA_ ? ", inverted (flipViewXVA = Y)"
                                                         : ", regular (flipViewXVA = N)"));
            string cid;
            if (flipViewXVA_) {
                cid = origDvaName;
                dvaName_ = trade->envelope().counterparty();
                fvaBorrowingCurve_ = dvaName_ + flipViewBorrowingCurvePostfix_;
                fvaLendingCurve_ = dvaName_ + flipViewLendingCurvePostfix_;
            } else {
                cid = trade->envelope().counterparty();
            }
            if (fvaBorrowingCurve_ != "")
                borrowingCurve = market_->yieldCurve(fvaBorrowingCurve_, configuration_);
            if (fvaLendingCurve_ != "")
                lendingCurve = market_->yieldCurve(fvaLendingCurve_, configuration_);

            if (!borrowingCurve.empty() || !lendingCurve.empty()) {
                QL_REQUIRE(baseCurrency_ != "", "baseCurrency required for FVA calculation");
            }
            string nid = trade->envelope().nettingSetId();
            Real cvaRR = market_->recoveryRate(cid, configuration_)->value();
            Real dvaRR = 0.0;
            if (dvaName_ != "")
                dvaRR = market_->recoveryRate(dvaName_, configuration_)->value();

            tradeCva_[tid] = 0.0;
            tradeDva_[tid] = 0.0;
            tradeFca_[tid] = 0.0;
            tradeFca_exOwnSp_[tid] = 0.0;
            tradeFca_exAllSp_[tid] = 0.0;
            tradeFba_[tid] = 0.0;
            tradeFba_exOwnSp_[tid] = 0.0;
            tradeFba_exAllSp_[tid] = 0.0;
            tradeMva_[tid] = 0.0;
            for (Size j = 0; j < numDates; ++j) {

                // CVA / DVA
                Date d0 = j == 0 ? today : dates()[j - 1];
                Date d1 = dates()[j];
                Real cvaIncrement = calculateCvaIncrement(tid, cid, d0, d1, cvaRR);
                Real dvaIncrement = dvaName_ != "" ? calculateDvaIncrement(tid, d0, d1, dvaRR) : 0;
                tradeCva_[tid] += cvaIncrement;
                tradeDva_[tid] += dvaIncrement;

                // FCA
                if (!borrowingCurve.empty()) {
                    Real dcf = borrowingCurve->discount(d0) / borrowingCurve->discount(d1) -
                               oisCurve->discount(d0) / oisCurve->discount(d1);
                    Real fcaIncrement = calculateFcaIncrement(tid, cid, dvaName_, d0, d1, dcf);
                    Real fcaIncrement_exOwnSP = calculateFcaIncrement(tid, cid, "", d0, d1, dcf);
                    Real fcaIncrement_exAllSP = calculateFcaIncrement(tid, "", "", d0, d1, dcf);
                    tradeFca_[tid] += fcaIncrement;
                    tradeFca_exOwnSp_[tid] += fcaIncrement_exOwnSP;
                    tradeFca_exAllSp_[tid] += fcaIncrement_exAllSP;
                }

                // FBA
                if (!lendingCurve.empty()) {
                    Real dcf = lendingCurve->discount(d0) / lendingCurve->discount(d1) -
                               oisCurve->discount(d0) / oisCurve->discount(d1);
                    Real fbaIncrement = calculateFbaIncrement(tid, cid, dvaName_, d0, d1, dcf);
                    Real fbaIncrement_exOwnSP = calculateFbaIncrement(tid, cid, "", d0, d1, dcf);
                    Real fbaIncrement_exAllSP = calculateFbaIncrement(tid, "", "", d0, d1, dcf);
                    tradeFba_[tid] += fbaIncrement;
                    tradeFba_exOwnSp_[tid] += fbaIncrement_exOwnSP;
                    tradeFba_exAllSp_[tid] += fbaIncrement_exAllSP;
                }
            }
            if (nettingSetSumCva_.find(nid) == nettingSetSumCva_.end()) {
                nettingSetSumCva_[nid] = 0.0;
                nettingSetSumDva_[nid] = 0.0;
            }
            nettingSetSumCva_[nid] += tradeCva_[tid];
            nettingSetSumDva_[nid] += tradeDva_[tid];
        } catch (const std::exception& e) {
            StructuredAnalyticsErrorMessage("ValueAdjustmentCalculator", "Error processing trade.", e.what(),
                                            {{"tradeId", tid}})
                .log();
        }
    }

    // Netting Set XVA
    for (const auto& pair : nettingSetCpty_) {
        string nid = pair.first;
        LOG("Update XVA for netting set "
            << nid << (flipViewXVA_ ? ", inverted (flipViewXVA = Y)" : ", regular (flipViewXVA = N)"));
        try {
            string cid;
            if (flipViewXVA_) {
                cid = origDvaName;
                dvaName_ = pair.second;
                fvaBorrowingCurve_ = dvaName_ + flipViewBorrowingCurvePostfix_;
                fvaLendingCurve_ = dvaName_ + flipViewLendingCurvePostfix_;
            } else {
                cid = pair.second;
            }
            Real cvaRR = market_->recoveryRate(cid, configuration_)->value();
            Real dvaRR = 0.0;
            if (dvaName_ != "") {
                dvaRR = market_->recoveryRate(dvaName_, configuration_)->value();
            }
            if (fvaBorrowingCurve_ != "")
                borrowingCurve = market_->yieldCurve(fvaBorrowingCurve_, configuration_);
            if (fvaLendingCurve_ != "")
                lendingCurve = market_->yieldCurve(fvaLendingCurve_, configuration_);

            if (!borrowingCurve.empty() || !lendingCurve.empty()) {
                QL_REQUIRE(baseCurrency_ != "", "baseCurrency required for FVA calculation");
            }

            nettingSetCva_[nid] = 0.0;
            nettingSetDva_[nid] = 0.0;
            nettingSetFca_[nid] = 0.0;
            nettingSetFca_exOwnSp_[nid] = 0.0;
            nettingSetFca_exAllSp_[nid] = 0.0;
            nettingSetFba_[nid] = 0.0;
            nettingSetFba_exOwnSp_[nid] = 0.0;
            nettingSetFba_exAllSp_[nid] = 0.0;
            nettingSetMva_[nid] = 0.0;
            for (Size j = 0; j < numDates; ++j) {
                // CVA / DVA
                Date d0 = j == 0 ? today : dates()[j - 1];
                Date d1 = dates()[j];
                Real cvaIncrement = calculateNettingSetCvaIncrement(nid, cid, d0, d1, cvaRR);
                Real dvaIncrement = dvaName_ != "" ? calculateNettingSetDvaIncrement(nid, d0, d1, dvaRR) : 0;
                nettingSetCva_[nid] += cvaIncrement;
                nettingSetDva_[nid] += dvaIncrement;

                // FCA
                if (!borrowingCurve.empty()) {
                    Real dcf = borrowingCurve->discount(d0) / borrowingCurve->discount(d1) -
                               oisCurve->discount(d0) / oisCurve->discount(d1);
                    Real fcaIncrement = calculateNettingSetFcaIncrement(nid, cid, dvaName_, d0, d1, dcf);
                    Real fcaIncrement_exOwnSP = calculateNettingSetFcaIncrement(nid, cid, "", d0, d1, dcf);
                    Real fcaIncrement_exAllSP = calculateNettingSetFcaIncrement(nid, "", "", d0, d1, dcf);
                    nettingSetFca_[nid] += fcaIncrement;
                    nettingSetFca_exOwnSp_[nid] += fcaIncrement_exOwnSP;
                    nettingSetFca_exAllSp_[nid] += fcaIncrement_exAllSP;

                    // MVA
                    if (dimCalculator_) {
                        Real mvaIncrement = calculateNettingSetMvaIncrement(nid, cid, d0, d1, dcf);
                        nettingSetMva_[nid] += mvaIncrement;
                    }
                }

                // FBA
                if (!lendingCurve.empty()) {
                    Real dcf = lendingCurve->discount(d0) / lendingCurve->discount(d1) -
                               oisCurve->discount(d0) / oisCurve->discount(d1);
                    Real fbaIncrement = calculateNettingSetFbaIncrement(nid, cid, dvaName_, d0, d1, dcf);
                    Real fbaIncrement_exOwnSP = calculateNettingSetFbaIncrement(nid, cid, "", d0, d1, dcf);
                    Real fbaIncrement_exAllSP = calculateNettingSetFbaIncrement(nid, "", "", d0, d1, dcf);
                    nettingSetFba_[nid] += fbaIncrement;
                    nettingSetFba_exOwnSp_[nid] += fbaIncrement_exOwnSP;
                    nettingSetFba_exAllSp_[nid] += fbaIncrement_exAllSP;
                }
            }
        } catch (const std::exception& e) {
            StructuredAnalyticsErrorMessage("ValueAdjustmentCalculator", "Error processing netting set.", e.what(),
                                            {{"nettingSetId", nid}})
                .log();
        }
    }
}

} // namespace analytics
} // namespace ore
