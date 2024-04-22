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

#include <orea/aggregation/nettedexposurecalculator.hpp>

#include <ored/portfolio/trade.hpp>

#include <ql/time/date.hpp>
#include <ql/time/calendars/weekendsonly.hpp>

using namespace std;
using namespace QuantLib;

namespace ore {
namespace analytics {

NettedExposureCalculator::NettedExposureCalculator(
    const QuantLib::ext::shared_ptr<Portfolio>& portfolio, const QuantLib::ext::shared_ptr<Market>& market,
    const QuantLib::ext::shared_ptr<NPVCube>& cube, const string& baseCurrency, const string& configuration,
    const Real quantile, const CollateralExposureHelper::CalculationType calcType, const bool multiPath,
    const QuantLib::ext::shared_ptr<NettingSetManager>& nettingSetManager,
    const QuantLib::ext::shared_ptr<CollateralBalances>& collateralBalances,
    const map<string, vector<vector<Real>>>& nettingSetDefaultValue,
    const map<string, vector<vector<Real>>>& nettingSetCloseOutValue,
    const map<string, vector<vector<Real>>>& nettingSetMporPositiveFlow,
    const map<string, vector<vector<Real>>>& nettingSetMporNegativeFlow,
    const QuantLib::ext::shared_ptr<AggregationScenarioData>& scenarioData,
    const QuantLib::ext::shared_ptr<CubeInterpretation> cubeInterpretation, const bool applyInitialMargin,
    const QuantLib::ext::shared_ptr<DynamicInitialMarginCalculator>& dimCalculator, const bool fullInitialCollateralisation,
    const bool marginalAllocation, const Real marginalAllocationLimit,
    const QuantLib::ext::shared_ptr<NPVCube>& tradeExposureCube, const Size allocatedEpeIndex, const Size allocatedEneIndex,
    const bool flipViewXVA, const bool withMporStickyDate, const MporCashFlowMode mporCashFlowMode)
    : portfolio_(portfolio), market_(market), cube_(cube), baseCurrency_(baseCurrency), configuration_(configuration),
      quantile_(quantile), calcType_(calcType), multiPath_(multiPath), nettingSetManager_(nettingSetManager),
      collateralBalances_(collateralBalances),
      nettingSetDefaultValue_(nettingSetDefaultValue), nettingSetCloseOutValue_(nettingSetCloseOutValue),
      nettingSetMporPositiveFlow_(nettingSetMporPositiveFlow), nettingSetMporNegativeFlow_(nettingSetMporNegativeFlow),
      scenarioData_(scenarioData), cubeInterpretation_(cubeInterpretation), applyInitialMargin_(applyInitialMargin),
      dimCalculator_(dimCalculator), fullInitialCollateralisation_(fullInitialCollateralisation),
      marginalAllocation_(marginalAllocation), marginalAllocationLimit_(marginalAllocationLimit),
      tradeExposureCube_(tradeExposureCube), allocatedEpeIndex_(allocatedEpeIndex),
      allocatedEneIndex_(allocatedEneIndex), flipViewXVA_(flipViewXVA), withMporStickyDate_(withMporStickyDate),
      mporCashFlowMode_(mporCashFlowMode) {

    set<string> nettingSetIds;
    for (auto nettingSet : nettingSetDefaultValue) {
        nettingSetIds.insert(nettingSet.first);
        if (flipViewXVA_) {
            if (nettingSetManager_->get(nettingSet.first)->activeCsaFlag()) {
                nettingSetManager_->get(nettingSet.first)->csaDetails()->invertCSA();
            }
        }
    }

    nettedCube_= QuantLib::ext::make_shared<SinglePrecisionInMemoryCube>(
            market_->asofDate(), nettingSetIds, cube->dates(),
            cube->samples()); // Exposure after collateral
    if (multiPath) {
        exposureCube_ = QuantLib::ext::make_shared<SinglePrecisionInMemoryCubeN>(
            market_->asofDate(), nettingSetIds, cube->dates(),
            cube->samples(), EXPOSURE_CUBE_DEPTH); // EPE, ENE
    } else {
        exposureCube_ = QuantLib::ext::make_shared<DoublePrecisionInMemoryCubeN>(
            market_->asofDate(), nettingSetIds, cube->dates(),
            1, EXPOSURE_CUBE_DEPTH); // EPE, ENE
    }
};

void NettedExposureCalculator::build() {
    LOG("Compute netting set exposure profiles");

    const Date today = market_->asofDate();
    const DayCounter dc = ActualActual(ActualActual::ISDA);

    vector<Real> times = vector<Real>(cube_->dates().size(), 0.0);
    for (Size i = 0; i < cube_->dates().size(); i++)
        times[i] = dc.yearFraction(today, cube_->dates()[i]);
    
    map<string, Real> nettingSetValueToday;
    map<string, Date> nettingSetMaturity;
    map<string, Size> nettingSetSize;
    Size cubeIndex = 0;
    for (auto tradeIt = portfolio_->trades().begin(); tradeIt != portfolio_->trades().end(); ++tradeIt, ++cubeIndex) {
        const auto& trade = tradeIt->second;
        string tradeId = tradeIt->first;
        string nettingSetId = trade->envelope().nettingSetId();
        string cp = trade->envelope().counterparty();
        if (counterpartyMap_.find(nettingSetId) == counterpartyMap_.end())
            counterpartyMap_[nettingSetId] = trade->envelope().counterparty();
        else {
            QL_REQUIRE(counterpartyMap_[nettingSetId] == cp, "counterparty name is not unique within the netting set");
        }
        Real npv;
        if (flipViewXVA_) {
            npv = -cube_->getT0(cubeIndex);
        } else {
            npv = cube_->getT0(cubeIndex);
        }

        if (nettingSetValueToday.find(nettingSetId) == nettingSetValueToday.end()) {
            nettingSetValueToday[nettingSetId] = 0.0;
            nettingSetMaturity[nettingSetId] = today;
            nettingSetSize[nettingSetId] = 0;
        }

        nettingSetValueToday[nettingSetId] += npv;

        if (trade->maturity() > nettingSetMaturity[nettingSetId])
            nettingSetMaturity[nettingSetId] = trade->maturity();
        nettingSetSize[nettingSetId]++;
    }

    vector<vector<Real>> averagePositiveAllocation(portfolio_->size(), vector<Real>(cube_->dates().size(), 0.0));
    vector<vector<Real>> averageNegativeAllocation(portfolio_->size(), vector<Real>(cube_->dates().size(), 0.0));

    Size nettingSetCount = 0;
    for (auto n : nettingSetDefaultValue_) {
        string nettingSetId = n.first;
        vector<vector<Real>> data = n.second;
        QuantLib::ext::shared_ptr<NettingSetDefinition> netting = nettingSetManager_->get(nettingSetId);

        // retrieve collateral balances object, if possible
        QuantLib::ext::shared_ptr<CollateralBalance> balance = nullptr;
        if (collateralBalances_ && collateralBalances_->has(nettingSetId)) {
            balance = collateralBalances_->get(nettingSetId);
            DLOG("got collateral balances for netting set " << nettingSetId);
        }
        
        //only for active CSA and calcType == NoLag close-out value is relevant
        if (netting->activeCsaFlag() && calcType_ == CollateralExposureHelper::CalculationType::NoLag) 
            data = nettingSetCloseOutValue_[nettingSetId];
        
        vector<vector<Real>> nettingSetMporPositiveFlow = nettingSetMporPositiveFlow_[nettingSetId];
        vector<vector<Real>> nettingSetMporNegativeFlow = nettingSetMporNegativeFlow_[nettingSetId];

        LOG("Aggregate exposure for netting set " << nettingSetId);
        // Get the collateral account balance paths for the netting set.
        // The pointer may remain empty if there is no CSA or if it is inactive.
        QuantLib::ext::shared_ptr<vector<QuantLib::ext::shared_ptr<CollateralAccount>>> collateral =
            collateralPaths(nettingSetId,
                            nettingSetValueToday[nettingSetId],
                            nettingSetDefaultValue_[nettingSetId],
                            nettingSetMaturity[nettingSetId]);

	// Get the CSA index for Eonia Floor calculation below
        colva_[nettingSetId] = 0.0;
        collateralFloor_[nettingSetId] = 0.0;
        string csaIndexName;
        Handle<IborIndex> csaIndex;
        bool applyInitialMargin = false;
        CSA::Type initialMarginType = CSA::Bilateral;
        if (netting->activeCsaFlag()) {
            csaIndexName = netting->csaDetails()->index();
            if (csaIndexName != "") {
                csaIndex = market_->iborIndex(csaIndexName);
                QL_REQUIRE(scenarioData_->has(AggregationScenarioDataType::IndexFixing, csaIndexName),
                           "scenario data does not provide index values for " << csaIndexName);
            }
            QL_REQUIRE(netting->csaDetails(), "active CSA for netting set " << nettingSetId
                    << ", but CSA details not initialised");
            applyInitialMargin = netting->csaDetails()->applyInitialMargin() && applyInitialMargin_;
            initialMarginType = netting->csaDetails()->initialMarginType();
            LOG("ApplyInitialMargin=" << applyInitialMargin << " for netting set " << nettingSetId 
                << ", CSA IM=" << netting->csaDetails()->applyInitialMargin()
                << ", CSA IM Type=" << initialMarginType
                << ", Analytics DIM=" << applyInitialMargin_);
            if (applyInitialMargin_ && !netting->csaDetails()->applyInitialMargin())
                ALOG("ApplyInitialMargin deactivated at netting set level " << nettingSetId);
            if (!applyInitialMargin_ && netting->csaDetails()->applyInitialMargin())
                ALOG("ApplyInitialMargin deactivated in analytics, but active at netting set level " << nettingSetId);
        }

        // Retrieve the constant independent amount from the CSA data and the VM balance
        // This is used below to reduce the exposure across all paths and time steps.
        // See below for the conversion to base currency.
        Real initialVM = 0, initialVMbase = 0;
        Real initialIM = 0, initialIMbase = 0;
        string csaCurrency = "";
        if (netting->activeCsaFlag() && balance) {
            initialVM = balance->variationMargin();
            initialIM = balance->initialMargin();
            double fx = 1.0;
            if (baseCurrency_ != balance->currency())
                fx = market_->fxSpot(balance->currency() + baseCurrency_)->value();
            initialVMbase = fx * initialVM;
            initialIMbase = fx * initialIM;
            DLOG("Netting set " << nettingSetId << ", initial VM: " << initialVMbase << " " << baseCurrency_);
            DLOG("Netting set " << nettingSetId << ", initial IM: " << initialIMbase << " " << baseCurrency_);
        }
        else {
            DLOG("Netting set " << nettingSetId << ", IA base = VM base = 0");
        }
        
        Handle<YieldTermStructure> curve = market_->discountCurve(baseCurrency_, configuration_);
        vector<Real> epe(cube_->dates().size() + 1, 0.0);
        vector<Real> ene(cube_->dates().size() + 1, 0.0);
        vector<Real> ee_b(cube_->dates().size() + 1, 0.0);
        vector<Real> eee_b(cube_->dates().size() + 1, 0.0);
        vector<Real> eee_b_kva_1(cube_->dates().size() + 1, 0.0);
        vector<Real> eee_b_kva_2(cube_->dates().size() + 1, 0.0);
        vector<Real> eepe_b_kva_1(cube_->dates().size() + 1, 0.0);
        vector<Real> eepe_b_kva_2(cube_->dates().size() + 1, 0.0);
        vector<Real> eab(cube_->dates().size() + 1, 0.0);
        vector<Real> pfe(cube_->dates().size() + 1, 0.0);
        vector<Real> colvaInc(cube_->dates().size() + 1, 0.0);
        vector<Real> eoniaFloorInc(cube_->dates().size() + 1, 0.0);
        Real npv = nettingSetValueToday[nettingSetId];
        if ((fullInitialCollateralisation_) & (netting->activeCsaFlag())) {
            // This assumes that the collateral at t=0 is the same as the npv at t=0.
            epe[0] = 0;
            ene[0] = 0;
            pfe[0] = 0;
        } else {
            epe[0] = std::max(npv - initialVMbase - initialIMbase, 0.0);
            ene[0] = std::max(-npv + initialVMbase, 0.0);
            pfe[0] = std::max(npv - initialVMbase - initialIMbase, 0.0);
        }
        // The fullInitialCollateralisation flag doesn't affect the eab, which feeds into the "ExpectedCollateral"
        // column of the 'exposure_nettingset_*' reports.  We always assume the full collateral here.
        eab[0] = npv;
        ee_b[0] = epe[0];
        eee_b[0] = ee_b[0];
        nettedCube_->setT0(npv, nettingSetCount);
        exposureCube_->setT0(epe[0], nettingSetCount, ExposureIndex::EPE);
        exposureCube_->setT0(ene[0], nettingSetCount, ExposureIndex::ENE);

        for (Size j = 0; j < cube_->dates().size(); ++j) {

            Date date = cube_->dates()[j];
            Date prevDate = j > 0 ? cube_->dates()[j - 1] : today;
            vector<Real> distribution(cube_->samples(), 0.0);
            for (Size k = 0; k < cube_->samples(); ++k) {
                Real balance = 0.0;
                if (collateral) {
                    balance = collateral->at(k)->accountBalance(date);
                    if (netting->csaDetails()->csaCurrency() != baseCurrency_) {
                        // Convert from CSACurrency to baseCurrency
                        double fxRate = scenarioData_->get(j, k, AggregationScenarioDataType::FXSpot,
                                                           netting->csaDetails()->csaCurrency());
                        balance *= fxRate;
                    }
                }
                
                eab[j + 1] += balance / cube_->samples();
                
                Real mporCashFlow = 0;
                // If ActualDate is active, then the cash flows over mpor can be configured.
                // Otherwise (StickyDate is active), it is assumed that no cash flow over mpor is paid out.
                if (!withMporStickyDate_) {
                    if (mporCashFlowMode_ == MporCashFlowMode::BothPay) {
                        // in cube generation -actual date- the (+/-) cashflows over mpor are
                        // payed out, i.e. are not part of the exposure .
                        mporCashFlow = 0;
                    } else if (mporCashFlowMode_ == MporCashFlowMode::NonePay) {
                        // +/- cashflows is to be incorporated in the exposure
                        mporCashFlow = (nettingSetMporPositiveFlow[j][k] + nettingSetMporNegativeFlow[j][k]);
                    } else if (mporCashFlowMode_ ==
                               MporCashFlowMode::WePay) { 
                        // only positive cash flows (i.e. cp's cashflows) is to be
                        // incorporated in the exposure, since cp does not pay out cash
                        // flows
                        mporCashFlow = nettingSetMporPositiveFlow[j][k];
                    } else if (mporCashFlowMode_ ==
                               MporCashFlowMode::TheyPay) { // onyl negative cash flows (i.e. our cashflows)  is to be
                        // incorporated in the exposure,  ince we do not pay out cash
                        // flows
                        mporCashFlow = nettingSetMporNegativeFlow[j][k];
                    }
                }
                Real exposure = data[j][k] - balance + mporCashFlow;
                Real dim = 0.0;
                if (applyInitialMargin && collateral) { // don't apply initial margin without VM, i.e. inactive CSA
                    // Initial Margin
                    // Use IM to reduce exposure
                    // Size dimIndex = j == 0 ? 0 : j - 1;
                    Size dimIndex = j;
                    dim = dimCalculator_->dynamicIM(nettingSetId)[dimIndex][k];
                    QL_REQUIRE(dim >= 0, "negative DIM for set " << nettingSetId << ", date " << j << ", sample " << k
                                                                 << ": " << dim);
                }
                Real dim_epe = 0;
                Real dim_ene = 0;
                if (initialMarginType != CSA::Type::PostOnly)
                    dim_epe = dim;
                if (initialMarginType != CSA::Type::CallOnly)
                    dim_ene = dim;
                
                // dim here represents the held IM, and is expressed as a positive number
                epe[j + 1] += std::max(exposure - dim_epe, 0.0) / cube_->samples(); 
                // dim here represents the posted IM, and is expressed as a positive number
                ene[j + 1] += std::max(-exposure - dim_ene, 0.0) / cube_->samples(); 
                distribution[k] = exposure - dim_epe;
                nettedCube_->set(exposure, nettingSetCount, j, k);
                
                Real epeIncrement = std::max(exposure - dim_epe, 0.0) / cube_->samples();
                DLOG("sample " << k << " date " << j << fixed << showpos << setprecision(2)
                     << ": VM "  << setw(15) << balance
                     << ": NPV " << setw(15) << data[j][k]
                     << ": NPV-C " << setw(15) << distribution[k]
                     << ": EPE " << setw(15) << epeIncrement);
                
                if (multiPath_) {
                    exposureCube_->set(std::max(exposure - dim_epe, 0.0), nettingSetCount, j, k, ExposureIndex::EPE);
                    exposureCube_->set(std::max(-exposure - dim_ene, 0.0), nettingSetCount, j, k, ExposureIndex::ENE);
                }
 
                if (netting->activeCsaFlag()) {
                    Real indexValue = 0.0;
                    DayCounter dc = ActualActual(ActualActual::ISDA);
                    if (csaIndexName != "") {
                        indexValue = scenarioData_->get(j, k, AggregationScenarioDataType::IndexFixing, csaIndexName);
                        dc = csaIndex->dayCounter();
                    }
                    Real dcf = dc.yearFraction(prevDate, date);
                    Real collateralSpread = (balance >= 0.0 ? netting->csaDetails()->collatSpreadRcv() : netting->csaDetails()->collatSpreadPay());
                    Real numeraire = scenarioData_->get(j, k, AggregationScenarioDataType::Numeraire);
                    Real colvaDelta = -balance * collateralSpread * dcf / numeraire / cube_->samples();
                    // intuitive floorDelta including collateralSpread would be:
                    // -balance * (max(indexValue - collateralSpread,0) - (indexValue - collateralSpread)) * dcf /
                    // samples
                    Real floorDelta = -balance * std::max(-(indexValue - collateralSpread), 0.0) * dcf / numeraire / cube_->samples();
                    colvaInc[j + 1] += colvaDelta;
                    colva_[nettingSetId] += colvaDelta;
                    eoniaFloorInc[j + 1] += floorDelta;
                    collateralFloor_[nettingSetId] += floorDelta;
                }

                if (marginalAllocation_) {
                    Size i = 0;
                    for (auto tradeIt = portfolio_->trades().begin(); tradeIt != portfolio_->trades().end();
                         ++tradeIt, ++i) {
                        const auto& trade = tradeIt->second;
                        string nid = trade->envelope().nettingSetId();
                        if (nid != nettingSetId)
                            continue;
                        
                        Real allocation = 0.0;
                        if (balance == 0.0)
                            allocation = cubeInterpretation_->getDefaultNpv(cube_, i, j, k);
                        // else if (data[j][k] == 0.0)
                        else if (fabs(data[j][k]) <= marginalAllocationLimit_)
                            allocation = exposure / nettingSetSize[nid];
                        else
                            allocation = exposure * cubeInterpretation_->getDefaultNpv(cube_, i, j, k) / data[j][k];

                        if (multiPath_) {
                            if (exposure > 0.0)
                                tradeExposureCube_->set(allocation, i, j, k, allocatedEpeIndex_);
                            else
                                tradeExposureCube_->set(-allocation, i, j, k, allocatedEneIndex_);
                        } else {
                            if (exposure > 0.0)
                                averagePositiveAllocation[i][j] += allocation / cube_->samples();
                            else
                                averageNegativeAllocation[i][j] -= allocation / cube_->samples();
                        }
                    }
                }
            }
            if (!multiPath_) {
                exposureCube_->set(epe[j + 1], nettingSetCount, j, 0, ExposureIndex::EPE);
                exposureCube_->set(ene[j + 1], nettingSetCount, j, 0, ExposureIndex::ENE);
            }
            ee_b[j + 1] = epe[j + 1] / curve->discount(cube_->dates()[j]);
            eee_b[j + 1] = std::max(eee_b[j], ee_b[j + 1]);
            std::sort(distribution.begin(), distribution.end());
            Size index = Size(floor(quantile_ * (cube_->samples() - 1) + 0.5));
            pfe[j + 1] = std::max(distribution[index], 0.0);
        }
        ee_b_[nettingSetId] = ee_b;
        eee_b_[nettingSetId] = eee_b;
        pfe_[nettingSetId] = pfe;
        expectedCollateral_[nettingSetId] = eab;
        colvaInc_[nettingSetId] = colvaInc;
        eoniaFloorInc_[nettingSetId] = eoniaFloorInc;

        nettingSetCount++;

        Real epe_b = 0;
        Real eepe_b = 0;

        Size t = 0;
        Calendar cal = WeekendsOnly();
        Date maturity = std::min(cal.adjust(today + 1 * Years + 4 * Days), nettingSetMaturity[nettingSetId]);
        QuantLib::Real maturityTime = dc.yearFraction(today, maturity);

        while (t < cube_->dates().size() && times[t] <= maturityTime)
            ++t;

        if (t > 0) {
            vector<double> weights(t);
            weights[0] = times[0];
            for (Size k = 1; k < t; k++)
                weights[k] = times[k] - times[k - 1];
            double totalWeights = std::accumulate(weights.begin(), weights.end(), 0.0);
            for (Size k = 0; k < t; k++)
                weights[k] /= totalWeights;

            for (Size k = 0; k < t; k++) {
                epe_b += ee_b[k] * weights[k];
                eepe_b += eee_b[k] * weights[k];
            }
        }
        epe_b_[nettingSetId] = epe_b;
        eepe_b_[nettingSetId] = eepe_b;
    }
                
    if (marginalAllocation_ && !multiPath_) {
        for (Size i = 0; i < portfolio_->trades().size(); ++i) {
            for (Size j = 0; j < cube_->dates().size(); ++j) {
                tradeExposureCube_->set(averagePositiveAllocation[i][j], i, j, 0, allocatedEpeIndex_);
                tradeExposureCube_->set(averageNegativeAllocation[i][j], i, j, 0, allocatedEneIndex_);
            }
        }
    }
}

QuantLib::ext::shared_ptr<vector<QuantLib::ext::shared_ptr<CollateralAccount>>>
NettedExposureCalculator::collateralPaths(
    const string& nettingSetId,
    const Real& nettingSetValueToday,
    const vector<vector<Real>>& nettingSetValue,
    const Date& nettingSetMaturity) {

    QuantLib::ext::shared_ptr<vector<QuantLib::ext::shared_ptr<CollateralAccount>>> collateral;

    if (!nettingSetManager_->has(nettingSetId) || !nettingSetManager_->get(nettingSetId)->activeCsaFlag()) {
        LOG("CSA missing or inactive for netting set " << nettingSetId);
        return collateral;
    }

    // retrieve collateral balances object, if possible
    QuantLib::ext::shared_ptr<CollateralBalance> balance = nullptr;
    if (collateralBalances_ && collateralBalances_->has(nettingSetId)) {
        balance = collateralBalances_->get(nettingSetId);
        LOG("got collateral balances for netting set " << nettingSetId);
    }
    
    LOG("Build collateral account balance paths for netting set " << nettingSetId);
    QuantLib::ext::shared_ptr<NettingSetDefinition> netting = nettingSetManager_->get(nettingSetId);
    string csaFxPair = netting->csaDetails()->csaCurrency() + baseCurrency_;
    Real csaFxRateToday = 1.0;
    if (netting->csaDetails()->csaCurrency() != baseCurrency_)
        csaFxRateToday = market_->fxRate(csaFxPair, configuration_)->value();
    LOG("CSA FX rate for pair " << csaFxPair << " = " << csaFxRateToday);

    // Don't use Settings::instance().evaluationDate() here, this has moved to simulation end date.
    Date today = market_->asofDate();
    string csaIndexName = netting->csaDetails()->index();
    // avoid thrown errors of the index fixing here on holidays of the index, instead take the preceding date then.
    if (!market_->iborIndex(csaIndexName, configuration_)->isValidFixingDate(today)) {
        today = market_->iborIndex(csaIndexName, configuration_)->fixingCalendar().adjust(today, Preceding);
    }
    Real csaRateToday = market_->iborIndex(csaIndexName, configuration_)->fixing(today);
    LOG("CSA compounding rate for index " << csaIndexName << " = " << setprecision(8) << csaRateToday << " as of " << today);

    // Copy scenario data to keep the collateral exposure helper unchanged
    vector<vector<Real>> csaScenFxRates(cube_->dates().size(), vector<Real>(cube_->samples(), 0.0));
    vector<vector<Real>> csaScenRates(cube_->dates().size(), vector<Real>(cube_->samples(), 0.0));
    if (netting->csaDetails()->csaCurrency() != baseCurrency_) {
        QL_REQUIRE(scenarioData_->has(AggregationScenarioDataType::FXSpot, netting->csaDetails()->csaCurrency()),
                   "scenario data does not provide FX rates for " << csaFxPair);
    }
    if (csaIndexName != "") {
        QL_REQUIRE(scenarioData_->has(AggregationScenarioDataType::IndexFixing, csaIndexName),
                   "scenario data does not provide index values for " << csaIndexName);
    }
    for (Size j = 0; j < cube_->dates().size(); ++j) {
        for (Size k = 0; k < cube_->samples(); ++k) {
	  if (netting->csaDetails()->csaCurrency() != baseCurrency_)
              csaScenFxRates[j][k] = cubeInterpretation_->getDefaultAggregationScenarioData(
                  AggregationScenarioDataType::FXSpot, j, k, netting->csaDetails()->csaCurrency());
            else
                csaScenFxRates[j][k] = 1.0;
            if (csaIndexName != "") {
                csaScenRates[j][k] = cubeInterpretation_->getDefaultAggregationScenarioData(
                    AggregationScenarioDataType::IndexFixing, j, k, csaIndexName);
            }
        }
    }

    collateral = CollateralExposureHelper::collateralBalancePaths(
        netting,              // this netting set's definition
        nettingSetValueToday, // today's netting set NPV
        market_->asofDate(),  // original evaluation date
        nettingSetValue,      // matrix of netting set values by date and sample
        nettingSetMaturity,   // netting set's maximum maturity date
        cube_->dates(),               // vector of future evaluation dates
        csaFxRateToday,       // today's FX rate for CSA to base currency, possibly 1
        csaScenFxRates,       // matrix of fx rates by date and sample, possibly 1
        csaRateToday,         // today's collateral compounding rate in CSA currency
        csaScenRates,         // matrix of CSA ccy short rates by date and sample
        calcType_,
        balance);             // initial collateral balances (VM, IM, IA) for the netting set
    LOG("Collateral account balance paths for netting set " << nettingSetId << " done");

    return collateral;
}

vector<Real> NettedExposureCalculator::getMeanExposure(const string& tid, ExposureIndex index) {
    vector<Real> exp(cube_->dates().size() + 1, 0.0);
    exp[0] = exposureCube_->getT0(tid, index);
    for (Size i = 0; i < cube_->dates().size(); i++) {
        if (multiPath_) {
	        for (Size k = 0; k < exposureCube_->samples(); k++) {
	            exp[i + 1] += exposureCube_->get(tid, cube_->dates()[i], k, index);
	        }
	        exp[i + 1] /= exposureCube_->samples();
	    }
	    else {
	        exp[i + 1] = exposureCube_->get(tid, cube_->dates()[i], 0, index);
	    }
    }
    return exp;
}

const string& NettedExposureCalculator::counterparty(const string nettingSetId) {
    auto it = counterpartyMap_.find(nettingSetId);
    QL_REQUIRE(it != counterpartyMap_.end(),
	       "counterparty not found for netting set id " << nettingSetId);
    return it->second;
}

} // namespace analytics
} // namespace ore
