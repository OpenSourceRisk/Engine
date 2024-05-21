/*
 Copyright (C) 2016-2020 Quaternion Risk Management Ltd
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

#include <orea/aggregation/postprocess.hpp>
#include <orea/aggregation/exposureallocator.hpp>
#include <orea/aggregation/dimcalculator.hpp>
#include <orea/aggregation/dimregressioncalculator.hpp>
#include <orea/aggregation/dynamiccreditxvacalculator.hpp>
#include <orea/aggregation/xvacalculator.hpp>
#include <orea/aggregation/staticcreditxvacalculator.hpp>
#include <orea/aggregation/cvaspreadsensitivitycalculator.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/vectorutils.hpp>
#include <ql/errors.hpp>
#include <ql/time/calendars/weekendsonly.hpp>

#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/math/generallinearleastsquares.hpp>
#include <ql/math/kernelfunctions.hpp>
#include <ql/methods/montecarlo/lsmbasissystem.hpp>
#include <ql/time/daycounters/actualactual.hpp>

#include <qle/math/nadarayawatson.hpp>
#include <qle/math/stabilisedglls.hpp>

#include <boost/range/adaptors.hpp>
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/error_of_mean.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>

using namespace std;
using namespace QuantLib;

using namespace boost::accumulators;

namespace ore {
namespace analytics {

PostProcess::PostProcess(
    const QuantLib::ext::shared_ptr<Portfolio>& portfolio, const QuantLib::ext::shared_ptr<NettingSetManager>& nettingSetManager,
    const QuantLib::ext::shared_ptr<CollateralBalances>& collateralBalances,
    const QuantLib::ext::shared_ptr<Market>& market, const std::string& configuration, const QuantLib::ext::shared_ptr<NPVCube>& cube,
    const QuantLib::ext::shared_ptr<AggregationScenarioData>& scenarioData, const map<string, bool>& analytics,
    const string& baseCurrency, const string& allocMethod, Real marginalAllocationLimit, Real quantile,
    const string& calculationType, const string& dvaName, const string& fvaBorrowingCurve,
    const string& fvaLendingCurve, const QuantLib::ext::shared_ptr<DynamicInitialMarginCalculator>& dimCalculator,
    const QuantLib::ext::shared_ptr<CubeInterpretation>& cubeInterpretation, bool fullInitialCollateralisation,
    vector<Period> cvaSensiGrid, Real cvaSensiShiftSize, Real kvaCapitalDiscountRate, Real kvaAlpha,
    Real kvaRegAdjustment, Real kvaCapitalHurdle, Real kvaOurPdFloor, Real kvaTheirPdFloor, Real kvaOurCvaRiskWeight,
    Real kvaTheirCvaRiskWeight, const QuantLib::ext::shared_ptr<NPVCube>& cptyCube, const string& flipViewBorrowingCurvePostfix,
    const string& flipViewLendingCurvePostfix,
    const QuantLib::ext::shared_ptr<CreditSimulationParameters>& creditSimulationParameters,
    const std::vector<Real>& creditMigrationDistributionGrid, const std::vector<Size>& creditMigrationTimeSteps,
    const Matrix& creditStateCorrelationMatrix, bool withMporStickyDate, MporCashFlowMode mporCashFlowMode)
: portfolio_(portfolio), nettingSetManager_(nettingSetManager), collateralBalances_(collateralBalances),
      market_(market), configuration_(configuration),
      cube_(cube), cptyCube_(cptyCube), scenarioData_(scenarioData), analytics_(analytics), baseCurrency_(baseCurrency),
      quantile_(quantile), calcType_(parseCollateralCalculationType(calculationType)), dvaName_(dvaName),
      fvaBorrowingCurve_(fvaBorrowingCurve), fvaLendingCurve_(fvaLendingCurve), dimCalculator_(dimCalculator),
      cubeInterpretation_(cubeInterpretation), fullInitialCollateralisation_(fullInitialCollateralisation),
      cvaSpreadSensiGrid_(cvaSensiGrid), cvaSpreadSensiShiftSize_(cvaSensiShiftSize),
      kvaCapitalDiscountRate_(kvaCapitalDiscountRate), kvaAlpha_(kvaAlpha), kvaRegAdjustment_(kvaRegAdjustment),
      kvaCapitalHurdle_(kvaCapitalHurdle), kvaOurPdFloor_(kvaOurPdFloor), kvaTheirPdFloor_(kvaTheirPdFloor),
      kvaOurCvaRiskWeight_(kvaOurCvaRiskWeight), kvaTheirCvaRiskWeight_(kvaTheirCvaRiskWeight),
      creditSimulationParameters_(creditSimulationParameters),
      creditMigrationDistributionGrid_(creditMigrationDistributionGrid),
      creditMigrationTimeSteps_(creditMigrationTimeSteps), creditStateCorrelationMatrix_(creditStateCorrelationMatrix),
      withMporStickyDate_(withMporStickyDate), mporCashFlowMode_(mporCashFlowMode) {

    QL_REQUIRE(cubeInterpretation_ != nullptr, "PostProcess: cubeInterpretation is not given.");

    if (mporCashFlowMode_ == MporCashFlowMode::Unspecified) {
        mporCashFlowMode_ = withMporStickyDate_ ? MporCashFlowMode::NonePay : MporCashFlowMode::BothPay;
    }

    QL_REQUIRE(!withMporStickyDate_ || mporCashFlowMode_ == MporCashFlowMode::NonePay,
               "PostProcess: MporMode StickyDate supports only MporCashFlowMode NonePay");
    QL_REQUIRE(cubeInterpretation_->storeFlows() || withMporStickyDate || mporCashFlowMode_ == MporCashFlowMode::BothPay,
               "PostProcess: If cube does not hold any mpor flows and MporMode is set to ActualDate, then "
               "MporCashFlowMode must "
               "be set to BothPay");

    bool isRegularCubeStorage = !cubeInterpretation_->withCloseOutLag();

    LOG("cube storage is regular: " << isRegularCubeStorage);
    LOG("cube dates: " << cube->dates().size());

    QL_REQUIRE(marginalAllocationLimit > 0.0, "positive allocationLimit expected");

    // check portfolio and cube have the same trade ids, in the same order
    QL_REQUIRE(portfolio->size() == cube_->idsAndIndexes().size(),
               "PostProcess::PostProcess(): portfolio size ("
                   << portfolio->size() << ") does not match cube trade size (" << cube_->idsAndIndexes().size() << ")");
    
    auto portfolioIt = portfolio->trades().begin();
    auto cubeIdIt = cube_->idsAndIndexes().begin();
    for (size_t i = 0; i < portfolio->size(); i++, portfolioIt++, cubeIdIt++) {
        const std::string& portfolioTradeId = portfolioIt->first;
        const std::string& cubeTradeId = cubeIdIt->first;
        QL_REQUIRE(portfolioTradeId == cubeTradeId, "PostProcess::PostProcess(): portfolio trade #"
                                                        << i << " (id=" << portfolioTradeId
                                                        << ") does not match cube trade id (" << cubeTradeId);
    }

    if (analytics_["dynamicCredit"]) {
        QL_REQUIRE(cptyCube_, "cptyCube cannot be null when dynamicCredit is ON");
        // check portfolio and cptyCube have the same counterparties, in the same order
        auto cptyIds = portfolio->counterparties();
        cptyIds.insert(dvaName_);
        QL_REQUIRE(cptyIds.size() == cptyCube_->idsAndIndexes().size(),
                   "PostProcess::PostProcess(): portfolio counterparty size ("
                       << cptyIds.size() << ") does not match cpty cube trade size ("
                       << cptyCube_->idsAndIndexes().size() << ")");

        auto cptyIt = cptyIds.begin();
        cubeIdIt = cptyCube_->idsAndIndexes().begin();
        for (size_t i = 0; i < cptyIds.size(); ++i, ++cptyIt, ++cubeIdIt) {
            const std::string& counterpartyId = *cptyIt;
            const std::string& cubeTradeId = cubeIdIt->first;
            QL_REQUIRE(counterpartyId == cubeTradeId, "PostProcess::PostProcess(): portfolio counterparty #"
                                                          << i << " (id=" << counterpartyId
                                                          << ") does not match cube name id (" << cubeTradeId);
        }
    }

    ExposureAllocator::AllocationMethod allocationMethod = parseAllocationMethod(allocMethod);

    /***********************************************
     * Step 0: Netting as of today
     * a) Compute the netting set NPV as of today
     * b) Find the final maturity of the netting set
     */
    LOG("Compute netting set NPVs as of today and netting set maturity");
    // Don't use Settings::instance().evaluationDate() here, this has moved to simulation end date.
    Date today = market->asofDate();
    LOG("AsOfDate = " << QuantLib::io::iso_date(today));

    /***************************************************************
     * Step 1: Dynamic Initial Margin calculation
     * Fills DIM cube per netting set that can be
     * - returned to be further analysed
     * - used in collateral calculation
     * - used in MVA calculation
     */
    if (analytics_["dim"] || analytics_["mva"]) {
        QL_REQUIRE(dimCalculator_, "DIM calculator not set");
        dimCalculator_->build();
    }

    /************************************************************
     * Step 2: Trade Exposure and Netting
     * a) Aggregation across scenarios per trade and date
     *    This yields single trade exposure profiles, EPE and ENE
     * b) Aggregation of NPVs within netting sets per date
     *    and scenario. This prepares the netting set exposure
     *    calculation below
     */
    exposureCalculator_ =
        QuantLib::ext::make_shared<ExposureCalculator>(
            portfolio, cube_, cubeInterpretation_,
            market_, analytics_["exerciseNextBreak"], baseCurrency_, configuration_,
            quantile_, calcType_, analytics_["dynamicCredit"], analytics_["flipViewXVA"]
        );
    exposureCalculator_->build();

    /******************************************************************
     * Step 3: Netting set exposure and allocation to trades
     *
     * a) Compute all netting set exposure profiles EPE and ENE using
     *    collateral if CSAs are given and active.
     * b) Compute the expected collateral balance for each netting set.
     * c) Allocate each netting set's exposure profile to the trade
     *    level such that the trade exposures add up to the netting
     *    set exposure.
     *    Reference:
     *    Michael Pykhtin & Dan Rosen, Pricing Counterparty Risk
     *    at the Trade Level and CVA Allocations, October 2010
     */
    nettedExposureCalculator_ = QuantLib::ext::make_shared<NettedExposureCalculator>(
        portfolio_, market_, cube_, baseCurrency, configuration_, quantile_, calcType_, analytics_["dynamicCredit"],
        nettingSetManager_, collateralBalances_, exposureCalculator_->nettingSetDefaultValue(),
        exposureCalculator_->nettingSetCloseOutValue(), exposureCalculator_->nettingSetMporPositiveFlow(),
        exposureCalculator_->nettingSetMporNegativeFlow(), scenarioData_, cubeInterpretation_, analytics_["dim"],
        dimCalculator_, fullInitialCollateralisation_,
        allocationMethod == ExposureAllocator::AllocationMethod::Marginal, marginalAllocationLimit,
        exposureCalculator_->exposureCube(), ExposureCalculator::allocatedEPE, ExposureCalculator::allocatedENE,
        analytics_["flipViewXVA"], withMporStickyDate_, mporCashFlowMode_);
    nettedExposureCalculator_->build();

    /********************************************************
     * Update Stand Alone XVAs
     * needed for some of the simple allocation methods below
     */
    if (analytics_["dynamicCredit"]) {
        cvaCalculator_ = QuantLib::ext::make_shared<DynamicCreditXvaCalculator>(
            portfolio_, market_, configuration_,baseCurrency_, dvaName_,
            fvaBorrowingCurve_, fvaLendingCurve_, analytics_["dim"],
            dimCalculator, exposureCalculator_->exposureCube(),
            nettedExposureCalculator_->exposureCube(), cptyCube_,
            ExposureCalculator::ExposureIndex::EPE,
            ExposureCalculator::ExposureIndex::ENE,
            NettedExposureCalculator::ExposureIndex::EPE,
            NettedExposureCalculator::ExposureIndex::ENE, 0, analytics_["flipViewXVA"], 
            flipViewBorrowingCurvePostfix, flipViewLendingCurvePostfix);
    } else {
        cvaCalculator_ = QuantLib::ext::make_shared<StaticCreditXvaCalculator>(
            portfolio_, market_, configuration_,baseCurrency_, dvaName_,
            fvaBorrowingCurve_, fvaLendingCurve_, analytics_["dim"],
            dimCalculator, exposureCalculator_->exposureCube(),
            nettedExposureCalculator_->exposureCube(),
            ExposureCalculator::ExposureIndex::EPE,
            ExposureCalculator::ExposureIndex::ENE,
            NettedExposureCalculator::ExposureIndex::EPE,
            NettedExposureCalculator::ExposureIndex::ENE, analytics_["flipViewXVA"], 
            flipViewBorrowingCurvePostfix, flipViewLendingCurvePostfix);
    }
    cvaCalculator_->build();

    /***************************
     * Simple allocation methods
     */
    QuantLib::ext::shared_ptr<ExposureAllocator> exposureAllocator;
    if (allocationMethod == ExposureAllocator::AllocationMethod::Marginal) {
        DLOG("Marginal Calculation handled in NettedExposureCalculator");
    }
    else if (allocationMethod == ExposureAllocator::AllocationMethod::RelativeFairValueNet)
        exposureAllocator = QuantLib::ext::make_shared<RelativeFairValueNetExposureAllocator>(
            portfolio, exposureCalculator_->exposureCube(),
            nettedExposureCalculator_->exposureCube(), cube_,
            ExposureCalculator::allocatedEPE, ExposureCalculator::allocatedENE,
            ExposureCalculator::EPE, ExposureCalculator::ENE,
            NettedExposureCalculator::EPE, NettedExposureCalculator::ENE);
    else if (allocationMethod == ExposureAllocator::AllocationMethod::RelativeFairValueGross)
        exposureAllocator = QuantLib::ext::make_shared<RelativeFairValueGrossExposureAllocator>(
            portfolio, exposureCalculator_->exposureCube(),
            nettedExposureCalculator_->exposureCube(), cube_,
            ExposureCalculator::allocatedEPE, ExposureCalculator::allocatedENE,
            ExposureCalculator::EPE, ExposureCalculator::ENE,
            NettedExposureCalculator::EPE, NettedExposureCalculator::ENE);
    else if (allocationMethod == ExposureAllocator::AllocationMethod::RelativeXVA)
        exposureAllocator = QuantLib::ext::make_shared<RelativeXvaExposureAllocator>(
            portfolio, exposureCalculator_->exposureCube(),
            nettedExposureCalculator_->exposureCube(), cube_,
            cvaCalculator_->tradeCva(), cvaCalculator_->tradeDva(),
            cvaCalculator_->nettingSetSumCva(), cvaCalculator_->nettingSetSumDva(),
            ExposureCalculator::allocatedEPE, ExposureCalculator::allocatedENE,
            ExposureCalculator::EPE, ExposureCalculator::ENE,
            NettedExposureCalculator::EPE, NettedExposureCalculator::ENE);
    else if (allocationMethod == ExposureAllocator::AllocationMethod::None)
        exposureAllocator = QuantLib::ext::make_shared<NoneExposureAllocator>(
            portfolio, exposureCalculator_->exposureCube(),
            nettedExposureCalculator_->exposureCube());
    else
        QL_FAIL("allocationMethod " << allocationMethod << " not available");
    if(exposureAllocator)
        exposureAllocator->build();

    /********************************************************
     * Update Allocated XVAs
     */
    if (analytics_["dynamicCredit"]) {
        allocatedCvaCalculator_ = QuantLib::ext::make_shared<DynamicCreditXvaCalculator>(
            portfolio_, market_, configuration_, baseCurrency_, dvaName_, fvaBorrowingCurve_, fvaLendingCurve_,
            analytics_["dim"], dimCalculator, exposureCalculator_->exposureCube(),
            nettedExposureCalculator_->exposureCube(), cptyCube_, ExposureCalculator::ExposureIndex::allocatedEPE,
            ExposureCalculator::ExposureIndex::allocatedENE, NettedExposureCalculator::ExposureIndex::EPE,
            NettedExposureCalculator::ExposureIndex::ENE, 0, analytics_["flipViewXVA"], flipViewBorrowingCurvePostfix,
            flipViewLendingCurvePostfix);
    } else {
        allocatedCvaCalculator_ = QuantLib::ext::make_shared<StaticCreditXvaCalculator>(
            portfolio_, market_, configuration_, baseCurrency_, dvaName_, fvaBorrowingCurve_, fvaLendingCurve_,
            analytics_["dim"], dimCalculator, exposureCalculator_->exposureCube(),
            nettedExposureCalculator_->exposureCube(), ExposureCalculator::ExposureIndex::allocatedEPE,
            ExposureCalculator::ExposureIndex::allocatedENE, NettedExposureCalculator::ExposureIndex::EPE,
            NettedExposureCalculator::ExposureIndex::ENE, analytics_["flipViewXVA"], flipViewBorrowingCurvePostfix,
            flipViewLendingCurvePostfix);
    }
    allocatedCvaCalculator_->build();

    /********************************************************
     * Cache average EPE and ENE
     */
    for (const auto& [tradeId,_]: tradeIds()) {
        tradeEPE_[tradeId] = exposureCalculator_->epe(tradeId);
        tradeENE_[tradeId] = exposureCalculator_->ene(tradeId);
        allocatedTradeEPE_[tradeId] = exposureCalculator_->allocatedEpe(tradeId);
        allocatedTradeENE_[tradeId] = exposureCalculator_->allocatedEne(tradeId);
    }
    for (const auto& [nettingSetId, pos] : nettingSetIds()) {
        netEPE_[nettingSetId] = nettedExposureCalculator_->epe(nettingSetId);
        netENE_[nettingSetId] = nettedExposureCalculator_->ene(nettingSetId);
    }

    /********************************************************
     * Calculate netting set KVA-CCR and KVA-CVA
     */
    updateNettingSetKVA();

    /***************************************
     * Calculate netting set CVA sensitivity
     */
    updateNettingSetCvaSensitivity();

    /***************************************
     * Credit migration analysis
     */
    if (analytics_["creditMigration"]) {
        creditMigrationCalculator_ = QuantLib::ext::make_shared<CreditMigrationCalculator>(
            portfolio_, creditSimulationParameters_, cube_, cubeInterpretation_,
            nettedExposureCalculator_->nettedCube(), scenarioData_, creditMigrationDistributionGrid_,
            creditMigrationTimeSteps_, creditStateCorrelationMatrix_, baseCurrency_);
        creditMigrationCalculator_->build();
        creditMigrationUpperBucketBounds_ = creditMigrationCalculator_->upperBucketBounds();
        creditMigrationCdf_ = creditMigrationCalculator_->cdf();
        creditMigrationPdf_ = creditMigrationCalculator_->pdf();
    }
}

void PostProcess::updateNettingSetKVA() {

    // Loop over all netting sets
    for (const auto& [nettingSetId,pos] : nettingSetIds()) {
        // Init results
        ourNettingSetKVACCR_[nettingSetId] = 0.0;
        theirNettingSetKVACCR_[nettingSetId] = 0.0;
        ourNettingSetKVACVA_[nettingSetId] = 0.0;
        theirNettingSetKVACVA_[nettingSetId] = 0.0;
    }

    if (!analytics_["kva"])
        return;

    LOG("Update netting set KVA");
    
    vector<Date> dateVector = cube_->dates();
    Size dates = dateVector.size();
    Date today = market_->asofDate();
    Handle<YieldTermStructure> discountCurve = market_->discountCurve(baseCurrency_, configuration_);
    DayCounter dc = ActualActual(ActualActual::ISDA);

    // Loop over all netting sets
    for (const auto& [nettingSetId, pos] : nettingSetIds()) {
        string cid;
        if (analytics_["flipViewXVA"]) {
            cid = dvaName_;
        } else {
            cid = nettedExposureCalculator_->counterparty(nettingSetId);
        }
        LOG("KVA for netting set " << nettingSetId);

        // Main input are the EPE and ENE profiles, previously computed
        vector<Real> epe = netEPE_[nettingSetId];
        vector<Real> ene = netENE_[nettingSetId];

        // PD from counterparty Dts, floored to avoid 0 ...
        // Today changed to today+1Y to get the one-year PD
        Handle<DefaultProbabilityTermStructure> cvaDts = market_->defaultCurve(cid, configuration_)->curve();
        QL_REQUIRE(!cvaDts.empty(), "Default curve missing for counterparty " << cid);
        Real cvaRR = market_->recoveryRate(cid, configuration_)->value();
        Real PD1 = std::max(cvaDts->defaultProbability(today + 1 * Years), 0.000000000001);
        Real LGD1 = (1 - cvaRR);

        // FIXME: if flipViewXVA is sufficient, then all code for their KVA-CCR could be discarded here...
        Handle<DefaultProbabilityTermStructure> dvaDts;
        Real dvaRR = 0.0;
        Real PD2 = 0;
        if (analytics_["flipViewXVA"]) {
            dvaName_ = nettedExposureCalculator_->counterparty(nettingSetId);
        }
        if (dvaName_ != "") {
            dvaDts = market_->defaultCurve(dvaName_, configuration_)->curve();
            dvaRR = market_->recoveryRate(dvaName_, configuration_)->value();
            PD2 = std::max(dvaDts->defaultProbability(today + 1 * Years), 0.000000000001);
        } else {
            ALOG("dvaName not specified, own PD set to zero for their KVA calculation");
        }
        Real LGD2 = (1 - dvaRR);

        // Granularity adjustment, Gordy (2004):
        Real rho1 = 0.12 * (1 - std::exp(-50 * PD1)) / (1 - std::exp(-50)) +
                    0.24 * (1 - (1 - std::exp(-50 * PD1)) / (1 - std::exp(-50)));
        Real rho2 = 0.12 * (1 - std::exp(-50 * PD2)) / (1 - std::exp(-50)) +
                    0.24 * (1 - (1 - std::exp(-50 * PD2)) / (1 - std::exp(-50)));

        // Basel II internal rating based (IRB) estimate of worst case PD:
        // Large homogeneous pool (LHP) approximation of Vasicek (1997)
        InverseCumulativeNormal icn;
        CumulativeNormalDistribution cnd;
        Real PD99_1 = cnd((icn(PD1) + std::sqrt(rho1) * icn(0.999)) / (std::sqrt(1 - rho1))) - PD1;
        Real PD99_2 = cnd((icn(PD2) + std::sqrt(rho2) * icn(0.999)) / (std::sqrt(1 - rho2))) - PD2;

        // KVA regulatory PD, worst case PD, floored at 0.03 for corporates and banks, not floored for sovereigns
        Real kva99PD1 = std::max(PD99_1, kvaTheirPdFloor_);
        Real kva99PD2 = std::max(PD99_2, kvaOurPdFloor_);

        // Factor B(PD) for the maturity adjustment factor, B(PD) = (0.11852 - 0.05478 * ln(PD)) ^ 2
        Real kvaMatAdjB1 = std::pow((0.11852 - 0.05478 * std::log(PD1)), 2.0);
        Real kvaMatAdjB2 = std::pow((0.11852 - 0.05478 * std::log(PD2)), 2.0);

        DLOG("Our KVA-CCR " << nettingSetId << ": PD=" << PD1);
        DLOG("Our KVA-CCR " << nettingSetId << ": LGD=" << LGD1);
        DLOG("Our KVA-CCR " << nettingSetId << ": rho=" << rho1);
        DLOG("Our KVA-CCR " << nettingSetId << ": PD99=" << PD99_1);
        DLOG("Our KVA-CCR " << nettingSetId << ": PD Floor=" << kvaTheirPdFloor_);
        DLOG("Our KVA-CCR " << nettingSetId << ": Floored PD99=" << kva99PD1);
        DLOG("Our KVA-CCR " << nettingSetId << ": B(PD)=" << kvaMatAdjB1);

        DLOG("Their KVA-CCR " << nettingSetId << ": PD=" << PD2);
        DLOG("Their KVA-CCR " << nettingSetId << ": LGD=" << LGD2);
        DLOG("Their KVA-CCR " << nettingSetId << ": rho=" << rho2);
        DLOG("Their KVA-CCR " << nettingSetId << ": PD99=" << PD99_2);
        DLOG("Their KVA-CCR " << nettingSetId << ": PD Floor=" << kvaOurPdFloor_);
        DLOG("Their KVA-CCR " << nettingSetId << ": Floored PD99=" << kva99PD2);
        DLOG("Their KVA-CCR " << nettingSetId << ": B(PD)=" << kvaMatAdjB2);

        for (Size j = 0; j < dates; ++j) {
            Date d0 = j == 0 ? today : cube_->dates()[j - 1];
            Date d1 = cube_->dates()[j];

            // Preprocess:
            // 1) Effective maturity from effective expected exposure as of time j
            //    Index _1 corresponds to our perspective, index _2 to their perspective.
            // 2) Basel EEPE as of time j, i.e. as time average over EEE, starting at time j
            // More accuracy may be achieved here by using a Longstaff-Schwartz method / regression
            Real eee_kva_1 = 0.0, eee_kva_2 = 0.0;
            Real effMatNumer1 = 0.0, effMatNumer2 = 0.0;
            Real effMatDenom1 = 0.0, effMatDenom2 = 0.0;
            Real eepe_kva_1 = 0, eepe_kva_2 = 0.0;
            Size kmax = j, count = 0;
            // Cut off index for EEPE/EENE calculation: One year ahead
            while (dateVector[kmax] < dateVector[j] + 1 * Years + 4 * Days && kmax < dates - 1)
                kmax++;
            Real sumdt = 0.0, eee1_b = 0.0, eee2_b = 0.0;
            for (Size k = j; k < dates; ++k) {
                Date d2 = cube_->dates()[k];
                Date prevDate = k == 0 ? today : dateVector[k - 1];

                eee_kva_1 = std::max(eee_kva_1, epe[k + 1]);
                eee_kva_2 = std::max(eee_kva_2, ene[k + 1]);

                // Components of the KVA maturity adjustment MA as of time j
                if (dc.yearFraction(d1, d2) > 1.0) {
                    effMatNumer1 += epe[k + 1] * dc.yearFraction(prevDate, d2);
                    effMatNumer2 += ene[k + 1] * dc.yearFraction(prevDate, d2);
                }
                if (dc.yearFraction(d1, d2) <= 1.0) {
                    effMatDenom1 += eee_kva_1 * dc.yearFraction(prevDate, d2);
                    effMatDenom2 += eee_kva_2 * dc.yearFraction(prevDate, d2);
                }

                if (k < kmax) {
                    Real dt = dc.yearFraction(cube_->dates()[k], cube_->dates()[k + 1]);
                    sumdt += dt;
                    Real epe_b = epe[k + 1] / discountCurve->discount(dateVector[k]);
                    Real ene_b = ene[k + 1] / discountCurve->discount(dateVector[k]);
                    eee1_b = std::max(epe_b, eee1_b);
                    eee2_b = std::max(ene_b, eee2_b);
                    eepe_kva_1 += eee1_b * dt;
                    eepe_kva_2 += eee2_b * dt;
                    count++;
                }
            }

            // Normalize EEPE/EENE calculation
            eepe_kva_1 = count > 0 ? eepe_kva_1 / sumdt : 0.0;
            eepe_kva_2 = count > 0 ? eepe_kva_2 / sumdt : 0.0;

            // KVA CCR using the IRB risk weighted asset method and IMM:
            // KVA effective maturity of the nettingSet, capped at 5
            Real kvaNWMaturity1 = std::min(1.0 + (effMatDenom1 == 0.0 ? 0.0 : effMatNumer1 / effMatDenom1), 5.0);
            Real kvaNWMaturity2 = std::min(1.0 + (effMatDenom2 == 0.0 ? 0.0 : effMatNumer2 / effMatDenom2), 5.0);

            // Maturity adjustment factor for the RWA method:
            // MA(PD, M) = (1 + (M - 2.5) * B(PD)) / (1 - 1.5 * B(PD)), capped at 5, floored at 1, M = effective
            // maturity
            Real kvaMatAdj1 =
                std::max(std::min((1.0 + (kvaNWMaturity1 - 2.5) * kvaMatAdjB1) / (1.0 - 1.5 * kvaMatAdjB1), 5.0), 1.0);
            Real kvaMatAdj2 =
                std::max(std::min((1.0 + (kvaNWMaturity2 - 2.5) * kvaMatAdjB2) / (1.0 - 1.5 * kvaMatAdjB2), 5.0), 1.0);

            // CCR Capital: RC = EAD x LGD x PD99.9 x MA(PD, M); EAD = alpha x EEPE(t) (approximated by EPE here);
            Real kvaRC1 = kvaAlpha_ * eepe_kva_1 * LGD1 * kva99PD1 * kvaMatAdj1;
            Real kvaRC2 = kvaAlpha_ * eepe_kva_2 * LGD2 * kva99PD2 * kvaMatAdj2;

            // Expected risk capital discounted at capital discount rate
            Real kvaCapitalDiscount = 1 / std::pow(1 + kvaCapitalDiscountRate_, dc.yearFraction(today, d0));
            Real kvaCCRIncrement1 =
                kvaRC1 * kvaCapitalDiscount * dc.yearFraction(d0, d1) * kvaCapitalHurdle_ * kvaRegAdjustment_;
            Real kvaCCRIncrement2 =
                kvaRC2 * kvaCapitalDiscount * dc.yearFraction(d0, d1) * kvaCapitalHurdle_ * kvaRegAdjustment_;

            ourNettingSetKVACCR_[nettingSetId] += kvaCCRIncrement1;
            theirNettingSetKVACCR_[nettingSetId] += kvaCCRIncrement2;

            DLOG("Our KVA-CCR for " << nettingSetId << ": " << j << " EEPE=" << setprecision(2) << eepe_kva_1
                                    << " EPE=" << epe[j] << " RC=" << kvaRC1 << " M=" << setprecision(6)
                                    << kvaNWMaturity1 << " MA=" << kvaMatAdj1 << " Cost=" << setprecision(2)
                                    << kvaCCRIncrement1 << " KVA=" << ourNettingSetKVACCR_[nettingSetId]);
            DLOG("Their KVA-CCR for " << nettingSetId << ": " << j << " EENE=" << eepe_kva_2 << " ENE=" << ene[j]
                                      << " RC=" << kvaRC2 << " M=" << setprecision(6) << kvaNWMaturity2
                                      << " MA=" << kvaMatAdj2 << " Cost=" << setprecision(2) << kvaCCRIncrement2
                                      << " KVA=" << theirNettingSetKVACCR_[nettingSetId]);

            // CVA Capital
            // effective maturity without cap at 5, DF set to 1 for IMM banks
            // TODO: Set MA in CCR capital calculation to 1
            Real kvaCvaMaturity1 = 1.0 + (effMatDenom1 == 0.0 ? 0.0 : effMatNumer1 / effMatDenom1);
            Real kvaCvaMaturity2 = 1.0 + (effMatDenom2 == 0.0 ? 0.0 : effMatNumer2 / effMatDenom2);
            Real scva1 = kvaTheirCvaRiskWeight_ * kvaCvaMaturity1 * eepe_kva_1;
            Real scva2 = kvaOurCvaRiskWeight_ * kvaCvaMaturity2 * eepe_kva_2;
            Real kvaCVAIncrement1 =
                scva1 * kvaCapitalDiscount * dc.yearFraction(d0, d1) * kvaCapitalHurdle_ * kvaRegAdjustment_;
            Real kvaCVAIncrement2 =
                scva2 * kvaCapitalDiscount * dc.yearFraction(d0, d1) * kvaCapitalHurdle_ * kvaRegAdjustment_;

            DLOG("Our KVA-CVA for " << nettingSetId << ": " << j << " EEPE=" << eepe_kva_1 << " SCVA=" << scva1
                                    << " Cost=" << kvaCVAIncrement1);
            DLOG("Their KVA-CVA for " << nettingSetId << ": " << j << " EENE=" << eepe_kva_2 << " SCVA=" << scva2
                                      << " Cost=" << kvaCVAIncrement2);

            ourNettingSetKVACVA_[nettingSetId] += kvaCVAIncrement1;
            theirNettingSetKVACVA_[nettingSetId] += kvaCVAIncrement2;
        }
    }

    LOG("Update netting set KVA done");
}

void PostProcess::updateNettingSetCvaSensitivity() {

    if (!analytics_["cvaSensi"])
        return;

    LOG("Update netting set CVA sensitivities");

    Handle<YieldTermStructure> discountCurve = market_->discountCurve(baseCurrency_, configuration_);

    for (auto n : netEPE_) {
        string nettingSetId = n.first;
        vector<Real> epe = netEPE_[nettingSetId];
        string cid;
        Handle<DefaultProbabilityTermStructure> cvaDts;
        Real cvaRR;
        if (analytics_["flipViewXVA"]) {
            cid = dvaName_;
        } else {
            cid = nettedExposureCalculator_->counterparty(nettingSetId);
        }
        cvaDts = market_->defaultCurve(cid)->curve();
        QL_REQUIRE(!cvaDts.empty(), "Default curve missing for counterparty " << cid);
        cvaRR = market_->recoveryRate(cid, configuration_)->value();

	    bool cvaSensi = analytics_["cvaSensi"];
	    LOG("CVA Sensitivity: " << cvaSensi);
	    if (cvaSensi) {
	        QuantLib::ext::shared_ptr<CVASpreadSensitivityCalculator> cvaSensiCalculator = QuantLib::ext::make_shared<CVASpreadSensitivityCalculator>(
	             nettingSetId, market_->asofDate(), epe, cube_->dates(), cvaDts, cvaRR, discountCurve, cvaSpreadSensiGrid_, cvaSpreadSensiShiftSize_);

	        for (Size i = 0; i < cvaSensiCalculator->shiftTimes().size(); ++i) {
	            DLOG("CVA Sensi Calculator: t=" << cvaSensiCalculator->shiftTimes()[i]
		         << " h=" << cvaSensiCalculator->hazardRateSensitivities()[i] 
		         << " s=" << cvaSensiCalculator->cdsSpreadSensitivities()[i]); 
	        }

	        netCvaHazardRateSensi_[nettingSetId] = cvaSensiCalculator->hazardRateSensitivities();
	        netCvaSpreadSensi_[nettingSetId] = cvaSensiCalculator->cdsSpreadSensitivities();
	        cvaSpreadSensiTimes_ = cvaSensiCalculator->shiftTimes();
	    } else {
	        cvaSpreadSensiTimes_ = vector<Real>();
	        netCvaHazardRateSensi_[nettingSetId] = vector<Real>();
	        netCvaSpreadSensi_[nettingSetId] = vector<Real>();
	    }
    }

    LOG("Update netting set CVA sensitivities done");
}
  
const vector<Real>& PostProcess::tradeEPE(const string& tradeId) {
    QL_REQUIRE(tradeEPE_.find(tradeId) != tradeEPE_.end(), "Trade " << tradeId << " not found in exposure map");
    return tradeEPE_[tradeId];
}

const vector<Real>& PostProcess::tradeENE(const string& tradeId) {
    QL_REQUIRE(tradeENE_.find(tradeId) != tradeENE_.end(), "Trade " << tradeId << " not found in exposure map");
    return tradeENE_[tradeId];
}

const vector<Real>& PostProcess::tradeEE_B(const string& tradeId) {
    return exposureCalculator_->ee_b(tradeId);
}

const Real& PostProcess::tradeEPE_B(const string& tradeId) {
    return exposureCalculator_->epe_b(tradeId);
}

const vector<Real>& PostProcess::tradeEEE_B(const string& tradeId) {
    return exposureCalculator_->eee_b(tradeId);
}

const Real& PostProcess::tradeEEPE_B(const string& tradeId) {
    return exposureCalculator_->eepe_b(tradeId);
}

const vector<Real>& PostProcess::tradePFE(const string& tradeId) {
    return exposureCalculator_->pfe(tradeId);
}

const vector<Real>& PostProcess::netEPE(const string& nettingSetId) {
    QL_REQUIRE(netEPE_.find(nettingSetId) != netEPE_.end(),
               "Netting set " << nettingSetId << " not found in exposure map");
    return netEPE_[nettingSetId];
}

const vector<Real>& PostProcess::netENE(const string& nettingSetId) {
    QL_REQUIRE(netENE_.find(nettingSetId) != netENE_.end(),
               "Netting set " << nettingSetId << " not found in exposure map");
    return netENE_[nettingSetId];
}

vector<Real> PostProcess::netCvaHazardRateSensitivity(const string& nettingSetId) {
    if (netCvaHazardRateSensi_.find(nettingSetId) != netCvaHazardRateSensi_.end())
        return netCvaHazardRateSensi_[nettingSetId];
    else
        return vector<Real>();
}

vector<Real> PostProcess::netCvaSpreadSensitivity(const string& nettingSetId) {
    if (netCvaSpreadSensi_.find(nettingSetId) != netCvaSpreadSensi_.end())
        return netCvaSpreadSensi_[nettingSetId];
    else
        return vector<Real>();
}

const vector<Real>& PostProcess::netEE_B(const string& nettingSetId) {
    return nettedExposureCalculator_->ee_b(nettingSetId);
}

const Real& PostProcess::netEPE_B(const string& nettingSetId) {
    return nettedExposureCalculator_->epe_b(nettingSetId);
}

const vector<Real>& PostProcess::netEEE_B(const string& nettingSetId) {
    return nettedExposureCalculator_->eee_b(nettingSetId);
}

const Real& PostProcess::netEEPE_B(const string& nettingSetId) {
    return nettedExposureCalculator_->eepe_b(nettingSetId);
}

const vector<Real>& PostProcess::netPFE(const string& nettingSetId) {
    return nettedExposureCalculator_->pfe(nettingSetId);
}

const vector<Real>& PostProcess::expectedCollateral(const string& nettingSetId) {
    return nettedExposureCalculator_->expectedCollateral(nettingSetId);
}

const vector<Real>& PostProcess::colvaIncrements(const string& nettingSetId) {
    return nettedExposureCalculator_->colvaIncrements(nettingSetId);
}

const vector<Real>& PostProcess::collateralFloorIncrements(const string& nettingSetId) {
    return nettedExposureCalculator_->collateralFloorIncrements(nettingSetId);
}

const vector<Real>& PostProcess::allocatedTradeEPE(const string& tradeId) {
    QL_REQUIRE(allocatedTradeEPE_.find(tradeId) != allocatedTradeEPE_.end(),
               "Trade " << tradeId << " not found in exposure map");
    return allocatedTradeEPE_[tradeId];
}

const vector<Real>& PostProcess::allocatedTradeENE(const string& tradeId) {
    QL_REQUIRE(allocatedTradeENE_.find(tradeId) != allocatedTradeENE_.end(),
               "Trade " << tradeId << " not found in exposure map");
    return allocatedTradeENE_[tradeId];
}

Real PostProcess::tradeCVA(const string& tradeId) {
    return cvaCalculator_->tradeCva(tradeId);
}

Real PostProcess::tradeDVA(const string& tradeId) {
    return cvaCalculator_->tradeDva(tradeId);
}

Real PostProcess::tradeMVA(const string& tradeId) {
    return cvaCalculator_->tradeMva(tradeId);
}

Real PostProcess::tradeFBA(const string& tradeId) {
    return cvaCalculator_->tradeFba(tradeId);
}

Real PostProcess::tradeFCA(const string& tradeId) {
    return cvaCalculator_->tradeFca(tradeId);
}

Real PostProcess::tradeFBA_exOwnSP(const string& tradeId) {
    return cvaCalculator_->tradeFba_exOwnSp(tradeId);
}

Real PostProcess::tradeFCA_exOwnSP(const string& tradeId) {
    return cvaCalculator_->tradeFca_exOwnSp(tradeId);
}

Real PostProcess::tradeFBA_exAllSP(const string& tradeId) {
    return cvaCalculator_->tradeFba_exAllSp(tradeId);
}

Real PostProcess::tradeFCA_exAllSP(const string& tradeId) {
    return cvaCalculator_->tradeFca_exAllSp(tradeId);
}

Real PostProcess::nettingSetCVA(const string& nettingSetId) {
    return cvaCalculator_->nettingSetCva(nettingSetId);
}

Real PostProcess::nettingSetDVA(const string& nettingSetId) {
    return cvaCalculator_->nettingSetDva(nettingSetId);
}

Real PostProcess::nettingSetMVA(const string& nettingSetId) {
    return cvaCalculator_->nettingSetMva(nettingSetId);

}

Real PostProcess::nettingSetFBA(const string& nettingSetId) {
    return cvaCalculator_->nettingSetFba(nettingSetId);

}

Real PostProcess::nettingSetFCA(const string& nettingSetId) {
    return cvaCalculator_->nettingSetFca(nettingSetId);

}

Real PostProcess::nettingSetOurKVACCR(const string& nettingSetId) {
    QL_REQUIRE(ourNettingSetKVACCR_.find(nettingSetId) != ourNettingSetKVACCR_.end(),
               "NettingSetId " << nettingSetId << " not found in nettingSet KVACCR map");
    return ourNettingSetKVACCR_[nettingSetId];
}

Real PostProcess::nettingSetTheirKVACCR(const string& nettingSetId) {
    QL_REQUIRE(theirNettingSetKVACCR_.find(nettingSetId) != theirNettingSetKVACCR_.end(),
               "NettingSetId " << nettingSetId << " not found in nettingSet KVACCR map");
    return theirNettingSetKVACCR_[nettingSetId];
}

Real PostProcess::nettingSetOurKVACVA(const string& nettingSetId) {
    QL_REQUIRE(ourNettingSetKVACVA_.find(nettingSetId) != ourNettingSetKVACVA_.end(),
               "NettingSetId " << nettingSetId << " not found in nettingSet KVACVA map");
    return ourNettingSetKVACVA_[nettingSetId];
}

Real PostProcess::nettingSetTheirKVACVA(const string& nettingSetId) {
    QL_REQUIRE(theirNettingSetKVACVA_.find(nettingSetId) != theirNettingSetKVACVA_.end(),
               "NettingSetId " << nettingSetId << " not found in nettingSet KVACVA map");
    return theirNettingSetKVACVA_[nettingSetId];
}

Real PostProcess::nettingSetFBA_exOwnSP(const string& nettingSetId) {
    return cvaCalculator_->nettingSetFba_exOwnSp(nettingSetId);
}

Real PostProcess::nettingSetFCA_exOwnSP(const string& nettingSetId) {
    return cvaCalculator_->nettingSetFca_exOwnSp(nettingSetId);
}

Real PostProcess::nettingSetFBA_exAllSP(const string& nettingSetId) {
    return cvaCalculator_->nettingSetFba_exAllSp(nettingSetId);
}

Real PostProcess::nettingSetFCA_exAllSP(const string& nettingSetId) {
    return cvaCalculator_->nettingSetFca_exAllSp(nettingSetId);
}

Real PostProcess::allocatedTradeCVA(const string& allocatedTradeId) {
    return allocatedCvaCalculator_->tradeCva(allocatedTradeId);
}

Real PostProcess::allocatedTradeDVA(const string& allocatedTradeId) {
    return allocatedCvaCalculator_->tradeDva(allocatedTradeId);
}

Real PostProcess::nettingSetCOLVA(const string& nettingSetId) {
    return nettedExposureCalculator_->colva(nettingSetId);
}

Real PostProcess::nettingSetCollateralFloor(const string& nettingSetId) {
    return nettedExposureCalculator_->collateralFloor(nettingSetId);
}

void PostProcess::exportDimEvolution(ore::data::Report& dimEvolutionReport) {
    dimCalculator_->exportDimEvolution(dimEvolutionReport);
}
  
void PostProcess::exportDimRegression(const std::string& nettingSet, const std::vector<Size>& timeSteps,
                                      const std::vector<QuantLib::ext::shared_ptr<ore::data::Report>>& dimRegReports) {

    QuantLib::ext::shared_ptr<RegressionDynamicInitialMarginCalculator> regCalc =
        QuantLib::ext::dynamic_pointer_cast<RegressionDynamicInitialMarginCalculator>(dimCalculator_);

    if (regCalc)
        regCalc->exportDimRegression(nettingSet, timeSteps, dimRegReports);
}

} // namespace analytics
} // namespace ore
