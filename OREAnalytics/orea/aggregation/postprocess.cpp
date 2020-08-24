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
#include <orea/aggregation/dimcalculator.hpp>
#include <orea/aggregation/dimregressioncalculator.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/vectorutils.hpp>
#include <ql/errors.hpp>
#include <ql/time/calendars/weekendsonly.hpp>
#include <ql/version.hpp>

#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/math/generallinearleastsquares.hpp>
#include <ql/math/kernelfunctions.hpp>
#include <ql/methods/montecarlo/lsmbasissystem.hpp>
#include <ql/time/daycounters/actualactual.hpp>

#include <qle/math/nadarayawatson.hpp>
#include <qle/math/stabilisedglls.hpp>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/error_of_mean.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>

using namespace std;
using namespace QuantLib;

using namespace boost::accumulators;

namespace ore {
namespace analytics {

AllocationMethod parseAllocationMethod(const string& s) {
    static map<string, AllocationMethod> m = {
        {"None", AllocationMethod::None},
        {"Marginal", AllocationMethod::Marginal},
        {"RelativeFairValueGross", AllocationMethod::RelativeFairValueGross},
        {"RelativeFairValueNet", AllocationMethod::RelativeFairValueNet},
        {"RelativeXVA", AllocationMethod::RelativeXVA},
    };

    auto it = m.find(s);
    if (it != m.end()) {
        return it->second;
    } else {
        QL_FAIL("AllocationMethod \"" << s << "\" not recognized");
    }
}

std::ostream& operator<<(std::ostream& out, AllocationMethod m) {
    if (m == AllocationMethod::None)
        out << "None";
    else if (m == AllocationMethod::Marginal)
        out << "Marginal";
    else if (m == AllocationMethod::RelativeFairValueGross)
        out << "RelativeFairValueGross";
    else if (m == AllocationMethod::RelativeFairValueNet)
        out << "RelativeFairValueNet";
    else if (m == AllocationMethod::RelativeXVA)
        out << "RelativeXVA";
    else
        QL_FAIL("Allocation method not covered");
    return out;
}

PostProcess::PostProcess(
    const boost::shared_ptr<Portfolio>& portfolio, const boost::shared_ptr<NettingSetManager>& nettingSetManager,
    const boost::shared_ptr<Market>& market, const std::string& configuration, const boost::shared_ptr<NPVCube>& cube,
    const boost::shared_ptr<AggregationScenarioData>& scenarioData, const map<string, bool>& analytics,
    const string& baseCurrency, const string& allocMethod, Real marginalAllocationLimit, Real quantile,
    const string& calculationType, const string& dvaName, const string& fvaBorrowingCurve,
    const string& fvaLendingCurve,const boost::shared_ptr<DynamicInitialMarginCalculator>& dimCalculator,
    const boost::shared_ptr<CubeInterpretation>& cubeInterpretation, bool fullInitialCollateralisation,
    Real kvaCapitalDiscountRate, Real kvaAlpha, Real kvaRegAdjustment, Real kvaCapitalHurdle, Real kvaOurPdFloor,
    Real kvaTheirPdFloor, Real kvaOurCvaRiskWeight, Real kvaTheirCvaRiskWeight)
    : portfolio_(portfolio), nettingSetManager_(nettingSetManager), market_(market), configuration_(configuration),
      cube_(cube), scenarioData_(scenarioData), analytics_(analytics), baseCurrency_(baseCurrency), quantile_(quantile),
      calcType_(parseCollateralCalculationType(calculationType)), dvaName_(dvaName),
      fvaBorrowingCurve_(fvaBorrowingCurve), fvaLendingCurve_(fvaLendingCurve), dimCalculator_(dimCalculator),
      cubeInterpretation_(cubeInterpretation), fullInitialCollateralisation_(fullInitialCollateralisation),
      kvaCapitalDiscountRate_(kvaCapitalDiscountRate), kvaAlpha_(kvaAlpha), kvaRegAdjustment_(kvaRegAdjustment),
      kvaCapitalHurdle_(kvaCapitalHurdle), kvaOurPdFloor_(kvaOurPdFloor), kvaTheirPdFloor_(kvaTheirPdFloor),
      kvaOurCvaRiskWeight_(kvaOurCvaRiskWeight), kvaTheirCvaRiskWeight_(kvaTheirCvaRiskWeight) {

    // set a default value for the cube interpretation object if it is NULL
    if (!cubeInterpretation_) {
        WLOG("cube interpretation is not set, use regular");
        cubeInterpretation_ = boost::make_shared<RegularCubeInterpretation>();
    }
    boost::shared_ptr<RegularCubeInterpretation> regularCubeInterpretation =
        boost::dynamic_pointer_cast<RegularCubeInterpretation>(cubeInterpretation_);
    bool isRegularCubeStorage = (regularCubeInterpretation != NULL);

    LOG("cube storage is regular: " << isRegularCubeStorage);
    LOG("cube dates: " << cube->dates().size());

    QL_REQUIRE(marginalAllocationLimit > 0.0, "positive allocationLimit expected");

    // check portfolio and cube have the same trade ids, in the same order
    QL_REQUIRE(portfolio->size() == cube_->ids().size(),
               "PostProcess::PostProcess(): portfolio size ("
                   << portfolio->size() << ") does not match cube trade size (" << cube_->ids().size() << ")");
    for (Size i = 0; i < portfolio->size(); ++i) {
        QL_REQUIRE(portfolio->trades()[i]->id() == cube_->ids()[i], "PostProcess::PostProcess(): portfolio trade #"
                                                                        << i << " (id=" << portfolio->trades()[i]->id()
                                                                        << ") does not match cube trade id ("
                                                                        << cube_->ids()[i]);
    }

    Size trades = portfolio->size();
    Size dates = cube_->dates().size(); // (isRegularCubeStorage) ? cube_->dates().size() - 1 : cube_->dates().size();
    Size samples = cube->samples();

    AllocationMethod allocationMethod = parseAllocationMethod(allocMethod);

    /***********************************************
     * Step 0: Netting as of today
     * a) Compute the netting set NPV as of today
     * b) Find the final maturity of the netting set
     */
    LOG("Compute netting set NPVs as of today and netting set maturity");
    map<string, Real> tradeValueToday;
    map<string, Real> nettingSetValueToday, nettingSetPositiveValueToday, nettingSetNegativeValueToday;
    map<string, Date> nettingSetMaturity;
    // Don't use Settings::instance().evaluationDate() here, this has moved to simulation end date.
    Date today = market->asofDate();
    LOG("AsOfDate = " << QuantLib::io::iso_date(today));

    vector<Real> times(dates);
    DayCounter dc = ActualActual();

    for (Size i = 0; i < dates; i++)
        times[i] = dc.yearFraction(today, cube_->dates()[i]);

    map<string, string> cidMap, nidMap;
    map<string, Date> matMap;
    for (auto trade : portfolio->trades()) {
        string tradeId = trade->id();
        nidMap[tradeId] = trade->envelope().nettingSetId();
        cidMap[tradeId] = trade->envelope().counterparty();
        if (cidMap[tradeId] != nettingSetManager_->get(nidMap[tradeId])->counterparty()) {
            // changed from QL_REQUIRE in Roland Kapl's original pull request to an ALERT
            ALOG("counterparty from trade (" << cidMap[tradeId]
                                             << "is not the same as counterparty from trade's netting set: "
                                             << nettingSetManager_->get(nidMap[tradeId])->counterparty());
        }
        matMap[tradeId] = trade->maturity();
    }

    for (Size i = 0; i < cube_->ids().size(); ++i) {
        string tradeId = cube_->ids()[i];
        string nettingSetId = nidMap[tradeId];
        string cpId = cidMap[tradeId];
        Real npv = cube_->getT0(i);

        tradeValueToday[tradeId] = npv;
        counterpartyId_[nettingSetId] = cpId;

        if (nettingSetValueToday.find(nettingSetId) == nettingSetValueToday.end()) {
            nettingSetValueToday[nettingSetId] = 0.0;
            nettingSetPositiveValueToday[nettingSetId] = 0.0;
            nettingSetNegativeValueToday[nettingSetId] = 0.0;
            nettingSetMaturity[nettingSetId] = today;
        }

        nettingSetValueToday[nettingSetId] += npv;
        if (npv > 0)
            nettingSetPositiveValueToday[nettingSetId] += npv;
        else
            nettingSetNegativeValueToday[nettingSetId] += npv;

        if (matMap[tradeId] > nettingSetMaturity[nettingSetId])
            nettingSetMaturity[nettingSetId] = matMap[tradeId];
    }

    /***************************************************************
     * Step 1: Dynamic Initial Margin calculation
     * Fills DIM cube per netting set that can be
     * - returned to be further analysed
     * - used in collateral calculation
     * - used in MVA calculation
     */
    if (analytics_["dim"] || analytics_["mva"]) {
        QL_REQUIRE(dimCalculator_, "DIM calculator not set");
	// dynamicInitialMargin();
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
    LOG("Compute trade exposure profiles");
    map<string, vector<vector<Real>>> nettingSetValue, nettingSetDefaultValue, nettingSetCloseOutValue;
    map<string, Size> nettingSetSize;
    set<string> nettingSets;
    bool exerciseNextBreak = analytics_["exerciseNextBreak"];
    for (Size i = 0; i < portfolio->size(); ++i) {
        string tradeId = portfolio->trades()[i]->id();
        string nettingSetId = portfolio->trades()[i]->envelope().nettingSetId();
        LOG("Aggregate exposure for trade " << tradeId);
        if (nettingSets.find(nettingSetId) == nettingSets.end()) {
            nettingSetDefaultValue[nettingSetId] = vector<vector<Real>>(dates, vector<Real>(samples, 0.0));
            nettingSetCloseOutValue[nettingSetId] = vector<vector<Real>>(dates, vector<Real>(samples, 0.0));
            nettingSets.insert(nettingSetId);
            nettingSetSize[nettingSetId] = 0;
        }
        nettingSetSize[nettingSetId]++;

        // Identify the next break date if provided, default is trade maturity.
        Date nextBreakDate = portfolio->trades()[i]->maturity();
        TradeActions ta = portfolio->trades()[i]->tradeActions();
        if (exerciseNextBreak && !ta.empty()) {
            // loop over actions and pick next mutual break, if available
            vector<TradeAction> actions = ta.actions();
            for (Size j = 0; j < actions.size(); ++j) {
                DLOG("TradeAction for " << tradeId << ", actionType " << actions[j].type() << ", actionOwner "
                                        << actions[j].owner());
                // FIXME: Introduce enumeration and parse text when building trade
                if (actions[j].type() == "Break" && actions[j].owner() == "Mutual") {
                    QuantLib::Schedule schedule = ore::data::makeSchedule(actions[j].schedule());
                    vector<Date> dates = schedule.dates();
                    std::sort(dates.begin(), dates.end());
                    Date today = Settings::instance().evaluationDate();
                    for (Size k = 0; k < dates.size(); ++k) {
                        if (dates[k] > today && dates[k] < nextBreakDate) {
                            nextBreakDate = dates[k];
                            DLOG("Next break date for trade " << tradeId << ": "
                                                              << QuantLib::io::iso_date(nextBreakDate));
                            break;
                        }
                    }
                }
            }
        }

        Handle<YieldTermStructure> curve = market_->discountCurve(baseCurrency_, configuration_);
        Real npv0 = tradeValueToday[tradeId];
        vector<Real> epe(dates + 1, 0.0);
        vector<Real> ene(dates + 1, 0.0);
        vector<Real> ee_b(dates + 1, 0.0);
        vector<Real> eee_b(dates + 1, 0.0);
        vector<Real> pfe(dates + 1, 0.0);
        epe[0] = std::max(npv0, 0.0);
        ene[0] = std::max(-npv0, 0.0);
        ee_b[0] = epe[0];
        eee_b[0] = ee_b[0];
        pfe[0] = std::max(npv0, 0.0);
        for (Size j = 0; j < dates; ++j) {
            Date d = cube_->dates()[j];
            vector<Real> distribution(samples, 0.0);
            for (Size k = 0; k < samples; ++k) {
                // RL 2020-07-17
                // 1) If the calculation type is set to NoLag:
                //    Collateral balances are NOT delayed by the MPoR, but we use the close-out NPV.
                // 2) Otherwise:
                //    Collateral balances are delayed by the MPoR (if possible, i.e. the valuation
                //    grid has MPoR spacing), and we use the default date NPV.
                //    This is the treatment in the ORE releases up to June 2020).
                Real defaultValue =
                    d > nextBreakDate && exerciseNextBreak ? 0.0 : cubeInterpretation_->getDefaultNpv(cube_, i, j, k);
                Real closeOutValue;
                if (isRegularCubeStorage && j == dates - 1)
                    closeOutValue = defaultValue;
                else
                    closeOutValue = d > nextBreakDate && exerciseNextBreak
                                        ? 0.0
                                        : cubeInterpretation_->getCloseOutNpv(cube_, i, j, k);
                Real npv = calcType_ == CollateralExposureHelper::CalculationType::NoLag ? closeOutValue : defaultValue;
                epe[j + 1] += max(npv, 0.0) / samples;
                ene[j + 1] += max(-npv, 0.0) / samples;
                nettingSetDefaultValue[nettingSetId][j][k] += defaultValue;
                nettingSetCloseOutValue[nettingSetId][j][k] += closeOutValue;
                distribution[k] = npv;
            }
            ee_b[j + 1] = epe[j + 1] / curve->discount(cube_->dates()[j]);
            eee_b[j + 1] = std::max(eee_b[j], ee_b[j + 1]);
            std::sort(distribution.begin(), distribution.end());
            Size index = Size(floor(quantile_ * (samples - 1) + 0.5));
            pfe[j + 1] = std::max(distribution[index], 0.0);
        }
        tradeIds_.push_back(tradeId);
        tradeEPE_[tradeId] = epe;
        tradeENE_[tradeId] = ene;
        tradeEE_B_[tradeId] = ee_b;
        tradeEEE_B_[tradeId] = eee_b;
        tradePFE_[tradeId] = pfe;

        Real epe_b = 0.0;
        Real eepe_b = 0.0;

        Size t = 0;
        Calendar cal = WeekendsOnly();
        /*The time average in the EEPE calculation is taken over the first year of the exposure evolution
        (or until maturity if all positions of the netting set mature before one year).
        This one year point is actually taken to be today+1Y+4D, so that the 1Y point on the dateGrid is always
        included.
        This may effect DateGrids with daily data points*/
        Date maturity = std::min(cal.adjust(today + 1 * Years + 4 * Days), portfolio->trades()[i]->maturity());
        QuantLib::Real maturityTime = dc.yearFraction(today, maturity);

        while (t < dates && times[t] <= maturityTime)
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
        tradeEPE_B_[tradeId] = epe_b;
        tradeEEPE_B_[tradeId] = eepe_b;
        // Allocated exposures will be populated in step 3 below
        allocatedTradeEPE_[tradeId] = vector<Real>(dates + 1, 0.0);
        allocatedTradeENE_[tradeId] = vector<Real>(dates + 1, 0.0);
    }

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
    LOG("Compute netting set exposure profiles");

    for (auto n : nettingSetDefaultValue)
        nettingSetIds_.push_back(n.first);

    // FIXME: Why is this not passed in? why are we hardcoding a cube instance here?
    nettedCube_ = boost::make_shared<SinglePrecisionInMemoryCube>(today, nettingSetIds_, cube_->dates(), samples);

    bool applyInitialMargin = analytics_["dim"];

    Size nettingSetCount = 0;
    nettingSetValue = (calcType_ == CollateralExposureHelper::CalculationType::NoLag ? nettingSetCloseOutValue
                                                                                     : nettingSetDefaultValue);
    for (auto n : nettingSetValue) {
        string nettingSetId = n.first;
        Size nettingSetTrades = nettingSetSize[nettingSetId];

        LOG("Aggregate exposure for netting set " << nettingSetId);
        vector<vector<Real>> data = n.second;

        // Get the collateral account balance paths for the netting set.
        // The pointer may remain empty if there is no CSA or if it is inactive.
        boost::shared_ptr<vector<boost::shared_ptr<CollateralAccount>>> collateral = collateralPaths(
            nettingSetId, nettingSetManager, market, configuration, scenarioData, dates, samples,
            nettingSetDefaultValue[nettingSetId], nettingSetValueToday[nettingSetId], nettingSetMaturity[nettingSetId]);

        // Get the CSA index for Eonia Floor calculation below
        nettingSetCOLVA_[nettingSetId] = 0.0;
        nettingSetCollateralFloor_[nettingSetId] = 0.0;
        boost::shared_ptr<NettingSetDefinition> netting = nettingSetManager->get(nettingSetId);
        string csaIndexName;
        Handle<IborIndex> csaIndex;
        if (netting->activeCsaFlag()) {
            csaIndexName = netting->index();
            if (csaIndexName != "") {
                csaIndex = market->iborIndex(csaIndexName);
                QL_REQUIRE(scenarioData->has(AggregationScenarioDataType::IndexFixing, csaIndexName),
                           "scenario data does not provide index values for " << csaIndexName);
            }
        }

        Handle<YieldTermStructure> curve = market_->discountCurve(baseCurrency_, configuration_);
        vector<Real> epe(dates + 1, 0.0);
        vector<Real> ene(dates + 1, 0.0);
        vector<Real> ee_b(dates + 1, 0.0);
        vector<Real> eee_b(dates + 1, 0.0);
        vector<Real> eee_b_kva_1(dates + 1, 0.0);
        vector<Real> eee_b_kva_2(dates + 1, 0.0);
        vector<Real> eepe_b_kva_1(dates + 1, 0.0);
        vector<Real> eepe_b_kva_2(dates + 1, 0.0);
        vector<Real> eab(dates + 1, 0.0);
        vector<Real> pfe(dates + 1, 0.0);
        vector<Real> colvaInc(dates + 1, 0.0);
        vector<Real> eoniaFloorInc(dates + 1, 0.0);
        Real npv = nettingSetValueToday[nettingSetId];
        if ((fullInitialCollateralisation_) & (netting->activeCsaFlag())) {
            // This assumes that the collateral at t=0 is the same as the npv at t=0.
            epe[0] = 0;
            ene[0] = 0;
            pfe[0] = 0;
        } else {
            epe[0] = std::max(npv, 0.0);
            ene[0] = std::max(-npv, 0.0);
            pfe[0] = std::max(npv, 0.0);
        }
        // The fullInitialCollateralisation flag doesn't affect the eab, which feeds into the "ExpectedCollateral"
        // column of the 'exposure_nettingset_*' reports.  We always assume the full collateral here.
        eab[0] = -npv;
        ee_b[0] = epe[0];
        eee_b[0] = ee_b[0];
        nettedCube_->setT0(nettingSetValueToday[nettingSetId], nettingSetCount);

        for (Size j = 0; j < dates; ++j) {

            Date date = cube_->dates()[j];
            Date prevDate = j > 0 ? cube_->dates()[j - 1] : today;

            vector<Real> distribution(samples, 0.0);
            for (Size k = 0; k < samples; ++k) {
                Real balance = 0.0;
                if (collateral)
                    balance = collateral->at(k)->accountBalance(date);

                eab[j + 1] += balance / samples;
                Real exposure = data[j][k] - balance;
                Real dim = 0.0;
                if (applyInitialMargin) {
                    // Initial Margin
                    // Use IM to reduce exposure
                    // Size dimIndex = j == 0 ? 0 : j - 1;
                    Size dimIndex = j;
                    // dim = nettingSetDIM_[nettingSetId][dimIndex][k];
                    dim = dimCalculator_->dynamicIM(nettingSetId)[dimIndex][k];
                    QL_REQUIRE(dim >= 0, "negative DIM for set " << nettingSetId << ", date " << j << ", sample " << k
                                                                 << ": " << dim);
                }
                epe[j + 1] += std::max(exposure - dim, 0.0) /
                              samples; // dim here represents the held IM, and is expressed as a positive number
                ene[j + 1] += std::max(-exposure - dim, 0.0) /
                              samples; // dim here represents the posted IM, and is expressed as a positive number
                distribution[k] = exposure;
                nettedCube_->set(exposure, nettingSetCount, j, k);

                if (netting->activeCsaFlag()) {
                    Real indexValue = 0.0;
                    DayCounter dc = ActualActual();
                    if (csaIndexName != "") {
                        indexValue = scenarioData->get(j, k, AggregationScenarioDataType::IndexFixing, csaIndexName);
                        dc = csaIndex->dayCounter();
                    }
                    Real dcf = dc.yearFraction(prevDate, date);
                    Real collateralSpread = (balance >= 0.0 ? netting->collatSpreadRcv() : netting->collatSpreadPay());
                    Real colvaDelta = -balance * collateralSpread * dcf / samples;
                    // inutuitive floorDelta including collateralSpread would be:
                    // -balance * (max(indexValue - collateralSpread,0) - (indexValue - collateralSpread)) * dcf /
                    // samples
                    Real floorDelta = -balance * std::max(-(indexValue - collateralSpread), 0.0) * dcf / samples;
                    colvaInc[j + 1] += colvaDelta;
                    nettingSetCOLVA_[nettingSetId] += colvaDelta;
                    eoniaFloorInc[j + 1] += floorDelta;
                    nettingSetCollateralFloor_[nettingSetId] += floorDelta;
                }

                if (allocationMethod == AllocationMethod::Marginal) {
                    for (Size i = 0; i < trades; ++i) {
                        string nid = portfolio->trades()[i]->envelope().nettingSetId();
                        if (nid != nettingSetId)
                            continue;
                        string tid = portfolio->trades()[i]->id();
                        Real allocation = 0.0;
                        if (balance == 0.0)
                            allocation = cubeInterpretation_->getDefaultNpv(cube_, i, j, k);
                        // else if (data[j][k] == 0.0)
                        else if (fabs(data[j][k]) <= marginalAllocationLimit)
                            allocation = exposure / nettingSetTrades;
                        else
                            allocation = exposure * cubeInterpretation_->getDefaultNpv(cube_, i, j, k) / data[j][k];

                        if (exposure > 0.0)
                            allocatedTradeEPE_[tid][j + 1] += allocation / samples;
                        else
                            allocatedTradeENE_[tid][j + 1] -= allocation / samples;
                    }
                }
            }
            ee_b[j + 1] = epe[j + 1] / curve->discount(cube_->dates()[j]);
            eee_b[j + 1] = std::max(eee_b[j], ee_b[j + 1]);
            std::sort(distribution.begin(), distribution.end());
            Size index = Size(floor(quantile_ * (samples - 1) + 0.5));
            pfe[j + 1] = std::max(distribution[index], 0.0);
        }
        expectedCollateral_[nettingSetId] = eab;
        netEPE_[nettingSetId] = epe;
        netENE_[nettingSetId] = ene;
        netEE_B_[nettingSetId] = ee_b;
        netEEE_B_[nettingSetId] = eee_b;
        netPFE_[nettingSetId] = pfe;
        colvaInc_[nettingSetId] = colvaInc;
        eoniaFloorInc_[nettingSetId] = eoniaFloorInc;
        nettingSetCount++;

        Real epe_b = 0;
        Real eepe_b = 0;

        Size t = 0;
        Calendar cal = WeekendsOnly();
        Date maturity = std::min(cal.adjust(today + 1 * Years + 4 * Days), nettingSetMaturity[nettingSetId]);
        QuantLib::Real maturityTime = dc.yearFraction(today, maturity);

        while (t < dates && times[t] <= maturityTime)
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
        netEPE_B_[nettingSetId] = epe_b;
        netEEPE_B_[nettingSetId] = eepe_b;
    }

    /********************************************************
     * Update Stand Alone XVAs
     * needed for some of the simple allocation methods below
     */
    updateStandAloneXVA();

    /***************************
     * Simple allocation methods
     */
    if (allocationMethod != AllocationMethod::Marginal) {
        for (auto n : nettingSetValue) {
            string nettingSetId = n.first;

            for (Size i = 0; i < trades; ++i) {
                string nid = portfolio->trades()[i]->envelope().nettingSetId();
                if (nid != nettingSetId)
                    continue;
                string tid = portfolio->trades()[i]->id();

                for (Size j = 0; j < dates; ++j) {
                    if (allocationMethod == AllocationMethod::RelativeFairValueNet) {
                        // FIXME: What to do when either the pos. or neg. netting set value is zero?
                        QL_REQUIRE(nettingSetPositiveValueToday[nid] > 0.0, "non-zero positive NPV expected");
                        QL_REQUIRE(nettingSetNegativeValueToday[nid] > 0.0, "non-zero negative NPV expected");
                        allocatedTradeEPE_[tid][j + 1] =
                            netEPE_[nid][j] * std::max(tradeValueToday[tid], 0.0) / nettingSetPositiveValueToday[nid];
                        allocatedTradeENE_[tid][j + 1] =
                            netENE_[nid][j] * -std::max(-tradeValueToday[tid], 0.0) / nettingSetNegativeValueToday[nid];
                    } else if (allocationMethod == AllocationMethod::RelativeFairValueGross) {
                        // FIXME: What to do when the netting set value is zero?
                        QL_REQUIRE(nettingSetValueToday[nid] != 0.0, "non-zero netting set value expected");
                        allocatedTradeEPE_[tid][j + 1] =
                            netEPE_[nid][j] * tradeValueToday[tid] / nettingSetValueToday[nid];
                        allocatedTradeENE_[tid][j + 1] =
                            netENE_[nid][j] * tradeValueToday[tid] / nettingSetValueToday[nid];
                    } else if (allocationMethod == AllocationMethod::RelativeXVA) {
                        allocatedTradeEPE_[tid][j + 1] = netEPE_[nid][j] * tradeCVA_[tid] / sumTradeCVA_[nid];
                        allocatedTradeENE_[tid][j + 1] = netENE_[nid][j] * tradeDVA_[tid] / sumTradeDVA_[nid];
                    } else if (allocationMethod == AllocationMethod::None) {
                        DLOG("No allocation from " << nid << " to " << tid << " date " << j);
                        allocatedTradeEPE_[tid][j + 1] = 0.0;
                        allocatedTradeENE_[tid][j + 1] = 0.0;
                    } else
                        QL_FAIL("allocationMethod " << allocationMethod << " not available");
                }
            }
        }
    }

    /********************************************************
     * Update Allocated XVAs
     */
    updateAllocatedXVA();

    /********************************************************
     * Calculate netting set KVA-CCR and KVA-CVA
     */
    updateNettingSetKVA();
}

void PostProcess::updateNettingSetKVA() {
    // Loop over all netting sets
    for (auto n : netEPE_) {
        string nettingSetId = n.first;
        // Init results
        ourNettingSetKVACCR_[nettingSetId] = 0.0;
        theirNettingSetKVACCR_[nettingSetId] = 0.0;
        ourNettingSetKVACVA_[nettingSetId] = 0.0;
        theirNettingSetKVACVA_[nettingSetId] = 0.0;
    }

    if (!analytics_["kva"])
        return;

    vector<Date> dateVector = cube_->dates();
    Size dates = dateVector.size();
    Date today = market_->asofDate();
    Handle<YieldTermStructure> discountCurve = market_->discountCurve(baseCurrency_, configuration_);
    DayCounter dc = ActualActual();

    // Loop over all netting sets
    for (auto n : netEPE_) {
        string nettingSetId = n.first;
        string cid = counterpartyId_[nettingSetId];
        LOG("KVA for netting set " << nettingSetId);

        // Main input are the EPE and ENE profiles, previously computed
        vector<Real> epe = netEPE_[nettingSetId];
        vector<Real> ene = netENE_[nettingSetId];

        // PD from counterparty Dts, floored to avoid 0 ...
        // Today changed to today+1Y to get the one-year PD
        Handle<DefaultProbabilityTermStructure> cvaDts = market_->defaultCurve(cid, configuration_);
        QL_REQUIRE(!cvaDts.empty(), "Default curve missing for counterparty " << cid);
        Real cvaRR = market_->recoveryRate(cid, configuration_)->value();
        Real PD1 = std::max(cvaDts->defaultProbability(today + 1 * Years), 0.000000000001);
        Real LGD1 = (1 - cvaRR);

        Handle<DefaultProbabilityTermStructure> dvaDts;
        Real dvaRR = 0.0;
        Real PD2 = 0;
        if (dvaName_ != "") {
            dvaDts = market_->defaultCurve(dvaName_, configuration_);
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
            // 2) Basel EEPE as of time j, i.e. as time averge over EEE, starting at time j
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
}

boost::shared_ptr<vector<boost::shared_ptr<CollateralAccount>>>
PostProcess::collateralPaths(const string& nettingSetId, const boost::shared_ptr<NettingSetManager>& nettingSetManager,
                             const boost::shared_ptr<Market>& market, const std::string& configuration,
                             const boost::shared_ptr<AggregationScenarioData>& scenarioData, Size dates, Size samples,
                             const vector<vector<Real>>& nettingSetValue, Real nettingSetValueToday,
                             const Date& nettingSetMaturity) {

    boost::shared_ptr<vector<boost::shared_ptr<CollateralAccount>>> collateral;

    if (!nettingSetManager->has(nettingSetId) || !nettingSetManager->get(nettingSetId)->activeCsaFlag()) {
        LOG("CSA missing or inactive for netting set " << nettingSetId);
        return collateral;
    }

    LOG("Build collateral account balance paths for netting set " << nettingSetId);
    boost::shared_ptr<NettingSetDefinition> netting = nettingSetManager->get(nettingSetId);
    string csaFxPair = netting->csaCurrency() + baseCurrency_;
    Real csaFxRateToday = 1.0;
    if (netting->csaCurrency() != baseCurrency_)
        csaFxRateToday = market->fxSpot(csaFxPair, configuration)->value();
    LOG("CSA FX rate for pair " << csaFxPair << " = " << csaFxRateToday);

    // Don't use Settings::instance().evaluationDate() here, this has moved to simulation end date.
    Date today = market->asofDate();
    string csaIndexName = netting->index();
    Real csaRateToday = market->iborIndex(csaIndexName, configuration)->fixing(today);
    LOG("CSA compounding rate for index " << csaIndexName << " = " << csaRateToday);

    // Copy scenario data to keep the collateral exposure helper unchanged
    vector<vector<Real>> csaScenFxRates(dates, vector<Real>(samples, 0.0));
    vector<vector<Real>> csaScenRates(dates, vector<Real>(samples, 0.0));
    if (netting->csaCurrency() != baseCurrency_) {
        QL_REQUIRE(scenarioData_->has(AggregationScenarioDataType::FXSpot, netting->csaCurrency()),
                   "scenario data does not provide FX rates for " << csaFxPair);
    }
    if (csaIndexName != "") {
        QL_REQUIRE(scenarioData_->has(AggregationScenarioDataType::IndexFixing, csaIndexName),
                   "scenario data does not provide index values for " << csaIndexName);
    }
    for (Size j = 0; j < dates; ++j) {
        for (Size k = 0; k < samples; ++k) {
            if (netting->csaCurrency() != baseCurrency_)
                csaScenFxRates[j][k] = cubeInterpretation_->getDefaultAggrionScenarioData(
                    scenarioData_, AggregationScenarioDataType::FXSpot, j, k, csaFxPair);
            else
                csaScenFxRates[j][k] = 1.0;
            if (csaIndexName != "") {
                csaScenRates[j][k] = cubeInterpretation_->getDefaultAggrionScenarioData(
                    scenarioData_, AggregationScenarioDataType::IndexFixing, j, k, csaIndexName);
            }
        }
    }

    collateral = CollateralExposureHelper::collateralBalancePaths(
        netting,              // this netting set's definition
        nettingSetValueToday, // today's netting set NPV
        today,                // original evaluation date
        nettingSetValue,      // matrix of netting set values by date and sample
        nettingSetMaturity,   // netting set's maximum maturity date
        cube_->dates(),       // vector of future evaluation dates
        csaFxRateToday,       // today's FX rate for CSA to base currency, possibly 1
        csaScenFxRates,       // matrix of fx rates by date and sample, possibly 1
        csaRateToday,         // today's collateral compounding rate in CSA currency
        csaScenRates,         // matrix of CSA ccy short rates by date and sample
        calcType_);
    LOG("Collateral account balance paths for netting set " << nettingSetId << " done");

    return collateral;
}

void PostProcess::updateStandAloneXVA() {
    Size trades = portfolio_->size();
    Size dates = cube_->dates().size();
    Date today = market_->asofDate();

    // Trade XVA
    for (Size i = 0; i < trades; ++i) {
        string tradeId = portfolio_->trades()[i]->id();
        LOG("Update XVA for trade " << tradeId);
        string cid = portfolio_->trades()[i]->envelope().counterparty();
        string nid = portfolio_->trades()[i]->envelope().nettingSetId();
        Handle<DefaultProbabilityTermStructure> cvaDts = market_->defaultCurve(cid, configuration_);
        QL_REQUIRE(!cvaDts.empty(), "Default curve missing for counterparty " << cid);
        Real cvaRR = market_->recoveryRate(cid, configuration_)->value();
        Handle<DefaultProbabilityTermStructure> dvaDts;
        Real dvaRR = 0.0;
        if (dvaName_ != "") {
            dvaDts = market_->defaultCurve(dvaName_, configuration_);
            dvaRR = market_->recoveryRate(dvaName_, configuration_)->value();
        }
        Handle<YieldTermStructure> borrowingCurve, lendingCurve;
        if (fvaBorrowingCurve_ != "")
            borrowingCurve = market_->yieldCurve(fvaBorrowingCurve_, configuration_);
        if (fvaLendingCurve_ != "")
            lendingCurve = market_->yieldCurve(fvaLendingCurve_, configuration_);

        Handle<YieldTermStructure> oisCurve = market_->discountCurve(baseCurrency_, configuration_);
        tradeCVA_[tradeId] = 0.0;
        tradeDVA_[tradeId] = 0.0;
        tradeFBA_[tradeId] = 0.0;
        tradeFCA_[tradeId] = 0.0;
        tradeMVA_[tradeId] = 0.0; // FIXME: MVA is not computed at trade level yet, remains initialised at 0
        for (Size j = 0; j < dates; ++j) {
            Date d0 = j == 0 ? today : cube_->dates()[j - 1];
            Date d1 = cube_->dates()[j];
            Real cvaS0 = cvaDts->survivalProbability(d0);
            Real cvaS1 = cvaDts->survivalProbability(d1);
            Real dvaS0 = dvaDts.empty() ? 1.0 : dvaDts->survivalProbability(d0);
            Real dvaS1 = dvaDts.empty() ? 1.0 : dvaDts->survivalProbability(d1);
            Real cvaIncrement = (1.0 - cvaRR) * (cvaS0 - cvaS1) * tradeEPE_[tradeId][j + 1];
            Real dvaIncrement = (1.0 - dvaRR) * (dvaS0 - dvaS1) * tradeENE_[tradeId][j + 1];
            tradeCVA_[tradeId] += cvaIncrement;
            tradeDVA_[tradeId] += dvaIncrement;

            Real borrowingSpreadDcf = 0.0;
            if (!borrowingCurve.empty())
                borrowingSpreadDcf = borrowingCurve->discount(d0) / borrowingCurve->discount(d1) -
                                     oisCurve->discount(d0) / oisCurve->discount(d1);
            Real fcaIncrement = cvaS0 * dvaS0 * borrowingSpreadDcf * tradeEPE_[tradeId][j + 1];
            Real fcaIncrement_exOwnSP = cvaS0 * borrowingSpreadDcf * tradeEPE_[tradeId][j + 1];
            Real fcaIncrement_exAllSP = borrowingSpreadDcf * tradeEPE_[tradeId][j + 1];
            tradeFCA_[tradeId] += fcaIncrement;
            tradeFCA_exOwnSP_[tradeId] += fcaIncrement_exOwnSP;
            tradeFCA_exAllSP_[tradeId] += fcaIncrement_exAllSP;

            Real lendingSpreadDcf = 0.0;
            if (!lendingCurve.empty())
                lendingSpreadDcf = lendingCurve->discount(d0) / lendingCurve->discount(d1) -
                                   oisCurve->discount(d0) / oisCurve->discount(d1);
            Real fbaIncrement = cvaS0 * dvaS0 * lendingSpreadDcf * tradeENE_[tradeId][j + 1];
            Real fbaIncrement_exOwnSP = cvaS0 * lendingSpreadDcf * tradeENE_[tradeId][j + 1];
            Real fbaIncrement_exAllSP = lendingSpreadDcf * tradeENE_[tradeId][j + 1];
            tradeFBA_[tradeId] += fbaIncrement;
            tradeFBA_exOwnSP_[tradeId] += fbaIncrement_exOwnSP;
            tradeFBA_exAllSP_[tradeId] += fbaIncrement_exAllSP;
        }
        if (sumTradeCVA_.find(nid) == sumTradeCVA_.end()) {
            sumTradeCVA_[nid] = 0.0;
            sumTradeDVA_[nid] = 0.0;
        }
        sumTradeCVA_[nid] += tradeCVA_[tradeId];
        sumTradeDVA_[nid] += tradeDVA_[tradeId];
    }

    bool applyMVA = analytics_["mva"];

    // Netting Set XVA
    for (auto n : netEPE_) {
        string nettingSetId = n.first;
        LOG("Update XVA for netting set " << nettingSetId);
        vector<Real> epe = netEPE_[nettingSetId];
        vector<Real> ene = netENE_[nettingSetId];

        vector<Real> edim;
        if (applyMVA)
            // edim = nettingSetExpectedDIM_[nettingSetId];
            edim = dimCalculator_->expectedIM(nettingSetId);
        string cid = counterpartyId_[nettingSetId];
        Handle<DefaultProbabilityTermStructure> cvaDts = market_->defaultCurve(cid);
        QL_REQUIRE(!cvaDts.empty(), "Default curve missing for counterparty " << cid);
        Real cvaRR = market_->recoveryRate(cid, configuration_)->value();
        Handle<DefaultProbabilityTermStructure> dvaDts;
        Real dvaRR = 0.0;
        if (dvaName_ != "") {
            dvaDts = market_->defaultCurve(dvaName_, configuration_);
            dvaRR = market_->recoveryRate(dvaName_, configuration_)->value();
        }

        Handle<YieldTermStructure> borrowingCurve, lendingCurve;
        if (fvaBorrowingCurve_ != "")
            borrowingCurve = market_->yieldCurve(fvaBorrowingCurve_, configuration_);
        if (fvaLendingCurve_ != "")
            lendingCurve = market_->yieldCurve(fvaLendingCurve_, configuration_);

        Handle<YieldTermStructure> oisCurve = market_->discountCurve(baseCurrency_, configuration_);
        nettingSetCVA_[nettingSetId] = 0.0;
        nettingSetDVA_[nettingSetId] = 0.0;
        nettingSetFBA_[nettingSetId] = 0.0;
        nettingSetFCA_[nettingSetId] = 0.0;
        nettingSetMVA_[nettingSetId] = 0.0;
        for (Size j = 0; j < dates; ++j) {
            Date d0 = j == 0 ? today : cube_->dates()[j - 1];
            Date d1 = cube_->dates()[j];
            Real cvaS0 = cvaDts->survivalProbability(d0);
            Real cvaS1 = cvaDts->survivalProbability(d1);
            Real dvaS0 = dvaDts.empty() ? 1.0 : dvaDts->survivalProbability(d0);
            Real dvaS1 = dvaDts.empty() ? 1.0 : dvaDts->survivalProbability(d1);
            Real cvaIncrement = (1.0 - cvaRR) * (cvaS0 - cvaS1) * epe[j + 1];
            Real dvaIncrement = (1.0 - dvaRR) * (dvaS0 - dvaS1) * ene[j + 1];
            nettingSetCVA_[nettingSetId] += cvaIncrement;
            nettingSetDVA_[nettingSetId] += dvaIncrement;

            Real borrowingSpreadDcf = 0.0;
            if (!borrowingCurve.empty())
                borrowingSpreadDcf = borrowingCurve->discount(d0) / borrowingCurve->discount(d1) -
                                     oisCurve->discount(d0) / oisCurve->discount(d1);
            Real fcaIncrement = cvaS0 * dvaS0 * borrowingSpreadDcf * epe[j + 1];
            Real fcaIncrement_exOwnSP = cvaS0 * borrowingSpreadDcf * epe[j + 1];
            Real fcaIncrement_exAllSP = borrowingSpreadDcf * epe[j + 1];
            nettingSetFCA_[nettingSetId] += fcaIncrement;
            nettingSetFCA_exOwnSP_[nettingSetId] += fcaIncrement_exOwnSP;
            nettingSetFCA_exAllSP_[nettingSetId] += fcaIncrement_exAllSP;

            Real lendingSpreadDcf = 0.0;
            if (!lendingCurve.empty())
                lendingSpreadDcf = lendingCurve->discount(d0) / lendingCurve->discount(d1) -
                                   oisCurve->discount(d0) / oisCurve->discount(d1);
            Real fbaIncrement = cvaS0 * dvaS0 * lendingSpreadDcf * ene[j + 1];
            Real fbaIncrement_exOwnSP = cvaS0 * lendingSpreadDcf * ene[j + 1];
            Real fbaIncrement_exAllSP = lendingSpreadDcf * ene[j + 1];
            nettingSetFBA_[nettingSetId] += fbaIncrement;
            nettingSetFBA_exOwnSP_[nettingSetId] += fbaIncrement_exOwnSP;
            nettingSetFBA_exAllSP_[nettingSetId] += fbaIncrement_exAllSP;

            // FIXME: Subtract the spread received on posted IM in MVA calculation
            if (applyMVA) {
                Real mvaIncrement = cvaS0 * dvaS0 * borrowingSpreadDcf * edim[j];
                nettingSetMVA_[nettingSetId] += mvaIncrement;
            }
        }
    }
}

void PostProcess::updateAllocatedXVA() {
    Size trades = portfolio_->size();
    Size dates = cube_->dates().size();
    Date today = market_->asofDate();

    // Allocated Trade XVA
    for (Size i = 0; i < trades; ++i) {
        string tradeId = portfolio_->trades()[i]->id();
        LOG("Update XVA for trade " << tradeId);
        string cid = portfolio_->trades()[i]->envelope().counterparty();
        Handle<DefaultProbabilityTermStructure> cvaDts = market_->defaultCurve(cid, configuration_);
        QL_REQUIRE(!cvaDts.empty(), "Default curve missing for counterparty " << cid);
        Real cvaRR = market_->recoveryRate(cid, configuration_)->value();
        Handle<DefaultProbabilityTermStructure> dvaDts;
        Real dvaRR = 0.0;
        if (dvaName_ != "") {
            dvaDts = market_->defaultCurve(dvaName_, configuration_);
            dvaRR = market_->recoveryRate(dvaName_, configuration_)->value();
        }
        allocatedTradeCVA_[tradeId] = 0.0;
        allocatedTradeDVA_[tradeId] = 0.0;
        for (Size j = 0; j < dates; ++j) {
            Date d0 = j == 0 ? today : cube_->dates()[j - 1];
            Date d1 = cube_->dates()[j];
            Real cvaS0 = cvaDts->survivalProbability(d0);
            Real cvaS1 = cvaDts->survivalProbability(d1);
            Real dvaS0 = dvaDts.empty() ? 1.0 : dvaDts->survivalProbability(d0);
            Real dvaS1 = dvaDts.empty() ? 1.0 : dvaDts->survivalProbability(d1);
            Real allocatedCvaIncrement = (1.0 - cvaRR) * (cvaS0 - cvaS1) * allocatedTradeEPE_[tradeId][j + 1];
            Real allocatedDvaIncrement = (1.0 - dvaRR) * (dvaS0 - dvaS1) * allocatedTradeENE_[tradeId][j + 1];
            allocatedTradeCVA_[tradeId] += allocatedCvaIncrement;
            allocatedTradeDVA_[tradeId] += allocatedDvaIncrement;
        }
    }
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
    QL_REQUIRE(tradeEE_B_.find(tradeId) != tradeEE_B_.end(), "Trade " << tradeId << " not found in exposure map");
    return tradeEE_B_[tradeId];
}

const Real& PostProcess::tradeEPE_B(const string& tradeId) {
    QL_REQUIRE(tradeEPE_B_.find(tradeId) != tradeEPE_B_.end(), "Trade " << tradeId << " not found in exposure map");
    return tradeEPE_B_[tradeId];
}

const vector<Real>& PostProcess::tradeEEE_B(const string& tradeId) {
    QL_REQUIRE(tradeEEE_B_.find(tradeId) != tradeEEE_B_.end(), "Trade " << tradeId << " not found in exposure map");
    return tradeEEE_B_[tradeId];
}

const Real& PostProcess::tradeEEPE_B(const string& tradeId) {
    QL_REQUIRE(tradeEEPE_B_.find(tradeId) != tradeEEPE_B_.end(), "Trade " << tradeId << " not found in exposure map");
    return tradeEEPE_B_[tradeId];
}

const vector<Real>& PostProcess::tradePFE(const string& tradeId) {
    QL_REQUIRE(tradePFE_.find(tradeId) != tradePFE_.end(), "Trade " << tradeId << " not found in the trade PFE map");
    return tradePFE_[tradeId];
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

const vector<Real>& PostProcess::netEE_B(const string& nettingSetId) {
    QL_REQUIRE(netEE_B_.find(nettingSetId) != netEE_B_.end(),
               "Netting set " << nettingSetId << " not found in exposure map");
    return netEE_B_[nettingSetId];
}

const Real& PostProcess::netEPE_B(const string& nettingSetId) {
    QL_REQUIRE(netEPE_B_.find(nettingSetId) != netEPE_B_.end(),
               "Netting set " << nettingSetId << " not found in exposure map");
    return netEPE_B_[nettingSetId];
}

const vector<Real>& PostProcess::netEEE_B(const string& nettingSetId) {
    QL_REQUIRE(netEEE_B_.find(nettingSetId) != netEEE_B_.end(),
               "Netting set " << nettingSetId << " not found in exposure map");
    return netEEE_B_[nettingSetId];
}

const Real& PostProcess::netEEPE_B(const string& nettingSetId) {
    QL_REQUIRE(netEEPE_B_.find(nettingSetId) != netEEPE_B_.end(),
               "Netting set " << nettingSetId << " not found in exposure map");
    return netEEPE_B_[nettingSetId];
}

const vector<Real>& PostProcess::netPFE(const string& nettingSetId) {
    QL_REQUIRE(netPFE_.find(nettingSetId) != netPFE_.end(),
               "Netting set " << nettingSetId << " not found in net PFE map");
    return netPFE_[nettingSetId];
}

const vector<Real>& PostProcess::expectedCollateral(const string& nettingSetId) {
    QL_REQUIRE(expectedCollateral_.find(nettingSetId) != expectedCollateral_.end(),
               "Netting set " << nettingSetId << " not found in exposure map");
    return expectedCollateral_[nettingSetId];
}

const vector<Real>& PostProcess::colvaIncrements(const string& nettingSetId) {
    QL_REQUIRE(colvaInc_.find(nettingSetId) != colvaInc_.end(),
               "Netting set " << nettingSetId << " not found in colvaInc map");
    return colvaInc_[nettingSetId];
}

const vector<Real>& PostProcess::collateralFloorIncrements(const string& nettingSetId) {
    QL_REQUIRE(eoniaFloorInc_.find(nettingSetId) != eoniaFloorInc_.end(),
               "Netting set " << nettingSetId << " not found in eoniaFloorInc map");
    return eoniaFloorInc_[nettingSetId];
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
    QL_REQUIRE(tradeCVA_.find(tradeId) != tradeCVA_.end(), "TradeId " << tradeId << " not found in trade CVA map");
    return tradeCVA_[tradeId];
}

Real PostProcess::tradeDVA(const string& tradeId) {
    QL_REQUIRE(tradeDVA_.find(tradeId) != tradeDVA_.end(), "TradeId " << tradeId << " not found in trade DVA map");
    return tradeDVA_[tradeId];
}

Real PostProcess::tradeMVA(const string& tradeId) {
    QL_REQUIRE(tradeMVA_.find(tradeId) != tradeMVA_.end(), "TradeId " << tradeId << " not found in trade MVA map");
    return tradeMVA_[tradeId];
}

Real PostProcess::tradeFBA(const string& tradeId) {
    QL_REQUIRE(tradeFBA_.find(tradeId) != tradeFBA_.end(), "TradeId " << tradeId << " not found in trade FBA map");
    return tradeFBA_[tradeId];
}

Real PostProcess::tradeFCA(const string& tradeId) {
    QL_REQUIRE(tradeFCA_.find(tradeId) != tradeFCA_.end(), "TradeId " << tradeId << " not found in trade FCA map");
    return tradeFCA_[tradeId];
}

Real PostProcess::tradeFBA_exOwnSP(const string& tradeId) {
    QL_REQUIRE(tradeFBA_.find(tradeId) != tradeFBA_.end(), "TradeId " << tradeId << " not found in trade FBA map");
    return tradeFBA_exOwnSP_[tradeId];
}

Real PostProcess::tradeFCA_exOwnSP(const string& tradeId) {
    QL_REQUIRE(tradeFCA_.find(tradeId) != tradeFCA_.end(), "TradeId " << tradeId << " not found in trade FCA map");
    return tradeFCA_exOwnSP_[tradeId];
}

Real PostProcess::tradeFBA_exAllSP(const string& tradeId) {
    QL_REQUIRE(tradeFBA_.find(tradeId) != tradeFBA_.end(), "TradeId " << tradeId << " not found in trade FBA map");
    return tradeFBA_exAllSP_[tradeId];
}

Real PostProcess::tradeFCA_exAllSP(const string& tradeId) {
    QL_REQUIRE(tradeFCA_.find(tradeId) != tradeFCA_.end(), "TradeId " << tradeId << " not found in trade FCA map");
    return tradeFCA_exAllSP_[tradeId];
}

Real PostProcess::nettingSetCVA(const string& nettingSetId) {
    QL_REQUIRE(nettingSetCVA_.find(nettingSetId) != nettingSetCVA_.end(),
               "NettingSetId " << nettingSetId << " not found in nettingSet CVA map");
    return nettingSetCVA_[nettingSetId];
}

Real PostProcess::nettingSetDVA(const string& nettingSetId) {
    QL_REQUIRE(nettingSetDVA_.find(nettingSetId) != nettingSetDVA_.end(),
               "NettingSetId " << nettingSetId << " not found in nettingSet DVA map");
    return nettingSetDVA_[nettingSetId];
}

Real PostProcess::nettingSetMVA(const string& nettingSetId) {
    QL_REQUIRE(nettingSetMVA_.find(nettingSetId) != nettingSetMVA_.end(),
               "NettingSetId " << nettingSetId << " not found in nettingSet MVA map");
    return nettingSetMVA_[nettingSetId];
}

Real PostProcess::nettingSetFBA(const string& nettingSetId) {
    QL_REQUIRE(nettingSetFBA_.find(nettingSetId) != nettingSetFBA_.end(),
               "NettingSetId " << nettingSetId << " not found in nettingSet FBA map");
    return nettingSetFBA_[nettingSetId];
}

Real PostProcess::nettingSetFCA(const string& nettingSetId) {
    QL_REQUIRE(nettingSetFCA_.find(nettingSetId) != nettingSetFCA_.end(),
               "NettingSetId " << nettingSetId << " not found in nettingSet FCA map");
    return nettingSetFCA_[nettingSetId];
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
    QL_REQUIRE(nettingSetFBA_exOwnSP_.find(nettingSetId) != nettingSetFBA_exOwnSP_.end(),
               "NettingSetId " << nettingSetId << " not found in nettingSet FBA_exOwnSP map");
    return nettingSetFBA_exOwnSP_[nettingSetId];
}

Real PostProcess::nettingSetFCA_exOwnSP(const string& nettingSetId) {
    QL_REQUIRE(nettingSetFCA_exOwnSP_.find(nettingSetId) != nettingSetFCA_exOwnSP_.end(),
               "NettingSetId " << nettingSetId << " not found in nettingSet FCA_exOwnSP map");
    return nettingSetFCA_exOwnSP_[nettingSetId];
}

Real PostProcess::nettingSetFBA_exAllSP(const string& nettingSetId) {
    QL_REQUIRE(nettingSetFBA_exAllSP_.find(nettingSetId) != nettingSetFBA_exAllSP_.end(),
               "NettingSetId " << nettingSetId << " not found in nettingSet FBA_exAllSP map");
    return nettingSetFBA_exAllSP_[nettingSetId];
}

Real PostProcess::nettingSetFCA_exAllSP(const string& nettingSetId) {
    QL_REQUIRE(nettingSetFCA_exAllSP_.find(nettingSetId) != nettingSetFCA_exAllSP_.end(),
               "NettingSetId " << nettingSetId << " not found in nettingSet FCA_exAllSP map");
    return nettingSetFCA_exAllSP_[nettingSetId];
}

Real PostProcess::allocatedTradeCVA(const string& allocatedTradeId) {
    QL_REQUIRE(allocatedTradeCVA_.find(allocatedTradeId) != allocatedTradeCVA_.end(),
               "AllocatedTradeId " << allocatedTradeId << " not found in allocatedTrade CVA map");
    return allocatedTradeCVA_[allocatedTradeId];
}

Real PostProcess::allocatedTradeDVA(const string& allocatedTradeId) {
    QL_REQUIRE(allocatedTradeDVA_.find(allocatedTradeId) != allocatedTradeDVA_.end(),
               "AllocatedTradeId " << allocatedTradeId << " not found in allocatedTrade DVA map");
    return allocatedTradeDVA_[allocatedTradeId];
}

Real PostProcess::nettingSetCOLVA(const string& nettingSetId) {
    QL_REQUIRE(nettingSetCOLVA_.find(nettingSetId) != nettingSetCOLVA_.end(),
               "NettingSetId " << nettingSetId << " not found in nettingSetCOLVA map");
    return nettingSetCOLVA_[nettingSetId];
}

Real PostProcess::nettingSetCollateralFloor(const string& nettingSetId) {
    QL_REQUIRE(nettingSetCollateralFloor_.find(nettingSetId) != nettingSetCollateralFloor_.end(),
               "NettingSetId " << nettingSetId << " not found in nettingSetCollateralFloor map");
    return nettingSetCollateralFloor_[nettingSetId];
}

void PostProcess::exportDimEvolution(ore::data::Report& dimEvolutionReport) {
    dimCalculator_->exportDimEvolution(dimEvolutionReport);
}
  
void PostProcess::exportDimRegression(const std::string& nettingSet, const std::vector<Size>& timeSteps,
                                      const std::vector<boost::shared_ptr<ore::data::Report>>& dimRegReports) {

    boost::shared_ptr<RegressionDynamicInitialMarginCalculator> regCalc =
        boost::dynamic_pointer_cast<RegressionDynamicInitialMarginCalculator>(dimCalculator_);

    if (regCalc)
        regCalc->exportDimRegression(nettingSet, timeSteps, dimRegReports);
}

} // namespace analytics
} // namespace ore
