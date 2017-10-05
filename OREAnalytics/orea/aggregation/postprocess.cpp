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

#include <orea/aggregation/postprocess.hpp>
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

PostProcess::PostProcess(const boost::shared_ptr<Portfolio>& portfolio,
                         const boost::shared_ptr<NettingSetManager>& nettingSetManager,
                         const boost::shared_ptr<Market>& market, const std::string& configuration,
                         const boost::shared_ptr<NPVCube>& cube,
                         const boost::shared_ptr<AggregationScenarioData>& scenarioData,
                         const map<string, bool>& analytics, const string& baseCurrency, const string& allocMethod,
                         Real marginalAllocationLimit, Real quantile, const string& calculationType,
                         const string& dvaName, const string& fvaBorrowingCurve, const string& fvaLendingCurve,
                         Real collateralSpread, Real dimQuantile, Size dimHorizonCalendarDays, Size dimRegressionOrder,
                         vector<string> dimRegressors, Size dimLocalRegressionEvaluations,
                         Real dimLocalRegressionBandwidth, Real dimScaling)
    : portfolio_(portfolio), nettingSetManager_(nettingSetManager), market_(market), cube_(cube),
      scenarioData_(scenarioData), analytics_(analytics), baseCurrency_(baseCurrency), quantile_(quantile),
      calcType_(parseCollateralCalculationType(calculationType)), dvaName_(dvaName),
      fvaBorrowingCurve_(fvaBorrowingCurve), fvaLendingCurve_(fvaLendingCurve), collateralSpread_(collateralSpread),
      dimQuantile_(dimQuantile), dimHorizonCalendarDays_(dimHorizonCalendarDays),
      dimRegressionOrder_(dimRegressionOrder), dimRegressors_(dimRegressors),
      dimLocalRegressionEvaluations_(dimLocalRegressionEvaluations),
      dimLocalRegressionBandwidth_(dimLocalRegressionBandwidth), dimScaling_(dimScaling) {

    QL_REQUIRE(marginalAllocationLimit > 0.0, "positive allocationLimit expected");

    Size trades = portfolio->size();
    Size dates = cube_->dates().size();
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
    if (analytics_["dim"] || analytics_["mva"])
        dynamicInitialMargin();

    /************************************************************
     * Step 2: Trade Exposure and Netting
     * a) Aggregation across scenarios per trade and date
     *    This yields single trade exposure profiles, EPE and ENE
     * b) Aggregation of NPVs within netting sets per date
     *    and scenario. This prepares the netting set exposure
     *    calculation below
     */
    LOG("Compute trade exposure profiles");
    map<string, vector<vector<Real>>> nettingSetValue;
    map<string, Size> nettingSetSize;
    set<string> nettingSets;
    bool exerciseNextBreak = analytics_["exerciseNextBreak"];
    for (Size i = 0; i < portfolio->size(); ++i) {
        string tradeId = portfolio->trades()[i]->id();
        string nettingSetId = portfolio->trades()[i]->envelope().nettingSetId();
        LOG("Aggregate exposure for trade " << tradeId);
        if (nettingSets.find(nettingSetId) == nettingSets.end()) {
            nettingSetValue[nettingSetId] = vector<vector<Real>>(dates, vector<Real>(samples, 0.0));
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
        vector<Real> ee_b(dates + 1);
        vector<Real> eee_b(dates + 1);
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
                Real npv = d > nextBreakDate && exerciseNextBreak ? 0.0 : cube->get(i, j, k);
                epe[j + 1] += max(npv, 0.0) / samples;
                ene[j + 1] += max(-npv, 0.0) / samples;
                nettingSetValue[nettingSetId][j][k] += npv;
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

    for (auto n : nettingSetValue)
        nettingSetIds_.push_back(n.first);

    // FIXME: Why is this not passed in? why are we hardcoding a cube instance here?
    nettedCube_ = boost::make_shared<SinglePrecisionInMemoryCube>(today, nettingSetIds_, cube_->dates(), samples);

    bool applyInitialMargin = analytics_["dim"];

    Size nettingSetCount = 0;
    for (auto n : nettingSetValue) {
        string nettingSetId = n.first;
        Size nettingSetTrades = nettingSetSize[nettingSetId];

        LOG("Aggregate exposure for netting set " << nettingSetId);
        vector<vector<Real>> data = n.second;

        // Get the collateral account balance paths for the netting set.
        // The pointer may remain empty if there is no CSA or if it is inactive.
        boost::shared_ptr<vector<boost::shared_ptr<CollateralAccount>>> collateral = collateralPaths(
            nettingSetId, nettingSetManager, market, configuration, scenarioData, dates, samples,
            nettingSetValue[nettingSetId], nettingSetValueToday[nettingSetId], nettingSetMaturity[nettingSetId]);

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
        vector<Real> eab(dates + 1, 0.0);
        vector<Real> pfe(dates + 1, 0.0);
        vector<Real> colvaInc(dates + 1, 0.0);
        vector<Real> eoniaFloorInc(dates + 1, 0.0);
        Real npv = nettingSetValueToday[nettingSetId];
        epe[0] = std::max(npv, 0.0);
        ene[0] = std::max(-npv, 0.0);
        ee_b[0] = epe[0];
        eee_b[0] = ee_b[0];
        eab[0] = -npv;
        pfe[0] = std::max(npv, 0.0);
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
                    // Use IM (at least one MPR in the past) to reduce today's exposure
                    // from both parties' perspectives.
                    // Assume that DIM is symmetric, same amount for both parties
                    // FIXME: Interpolation to determine DIM at time t - MPOR
                    //        The following is only correct for a grid with MPOR time steps.
                    Size dimIndex = j == 0 ? 0 : j - 1;
                    dim = nettingSetDIM_[nettingSetId][dimIndex][k];
                    QL_REQUIRE(dim >= 0, "negative DIM for set " << nettingSetId << ", date " << j << ", sample " << k);
                }
                epe[j + 1] += std::max(exposure - dim, 0.0) / samples;
                ene[j + 1] += std::max(-exposure + dim, 0.0) / samples;
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
                    Real colvaDelta = -balance * collateralSpread_ * dcf / samples;
                    Real floorDelta = -balance * std::max(-indexValue, 0.0) * dcf / samples;
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
                            allocation = cube->get(i, j, k);
                        // else if (data[j][k] == 0.0)
                        else if (fabs(data[j][k]) <= marginalAllocationLimit)
                            allocation = exposure / nettingSetTrades;
                        else
                            allocation = exposure * cube->get(i, j, k) / data[j][k];

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
                csaScenFxRates[j][k] =
                    scenarioData_->get(j, k, AggregationScenarioDataType::FXSpot, netting->csaCurrency());
            else
                csaScenFxRates[j][k] = 1.0;
            if (csaIndexName != "") {
                csaScenRates[j][k] = scenarioData_->get(j, k, AggregationScenarioDataType::IndexFixing, csaIndexName);
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
            Real fbaIncrement = cvaS0 * dvaS0 * borrowingSpreadDcf * tradeEPE_[tradeId][j + 1];
            tradeFBA_[tradeId] += fbaIncrement;

            Real lendingSpreadDcf = 0.0;
            if (!lendingCurve.empty())
                lendingSpreadDcf = lendingCurve->discount(d0) / lendingCurve->discount(d1) -
                                   oisCurve->discount(d0) / oisCurve->discount(d1);
            Real fcaIncrement = cvaS0 * dvaS0 * lendingSpreadDcf * tradeENE_[tradeId][j + 1];
            tradeFCA_[tradeId] += fcaIncrement;
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
            edim = nettingSetExpectedDIM_[nettingSetId];
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
            Real fbaIncrement = cvaS0 * dvaS0 * borrowingSpreadDcf * epe[j + 1];
            nettingSetFBA_[nettingSetId] += fbaIncrement;

            Real lendingSpreadDcf = 0.0;
            if (!lendingCurve.empty())
                lendingSpreadDcf = lendingCurve->discount(d0) / lendingCurve->discount(d1) -
                                   oisCurve->discount(d0) / oisCurve->discount(d1);
            Real fcaIncrement = cvaS0 * dvaS0 * lendingSpreadDcf * ene[j + 1];
            nettingSetFCA_[nettingSetId] += fcaIncrement;

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

Disposable<Array> PostProcess::regressorArray(string nettingSet, Size dateIndex, Size sampleIndex) {
    Array a(dimRegressors_.size());
    for (Size i = 0; i < dimRegressors_.size(); ++i) {
        string variable = dimRegressors_[i];
        if (boost::to_upper_copy(variable) ==
            "NPV") // this allows possibility to include NPV as a regressor alongside more fundamental risk factors
            a[i] = nettingSetNPV_[nettingSet][dateIndex][sampleIndex];
        else if (scenarioData_->has(AggregationScenarioDataType::IndexFixing, variable))
            a[i] = scenarioData_->get(dateIndex, sampleIndex, AggregationScenarioDataType::IndexFixing, variable);
        else if (scenarioData_->has(AggregationScenarioDataType::FXSpot, variable))
            a[i] = scenarioData_->get(dateIndex, sampleIndex, AggregationScenarioDataType::FXSpot, variable);
        else if (scenarioData_->has(AggregationScenarioDataType::Generic, variable))
            a[i] = scenarioData_->get(dateIndex, sampleIndex, AggregationScenarioDataType::Generic, variable);
        else
            QL_FAIL("scenario data does not provide data for " << variable);
    }
    return a;
}

void PostProcess::dynamicInitialMargin() {
    LOG("DIM Analysis by regression");

    Date today = market_->asofDate();
    Size dates = cube_->dates().size();
    Size samples = cube_->samples();
    map<string, Size> nettingSetSize;
    set<string> nettingSets;

    // initialise aggregate NPV and Flow by date and scenario
    for (Size i = 0; i < portfolio_->size(); ++i) {
        string tradeId = portfolio_->trades()[i]->id();
        string nettingSetId = portfolio_->trades()[i]->envelope().nettingSetId();
        LOG("Aggregate exposure for trade " << tradeId);
        if (nettingSets.find(nettingSetId) == nettingSets.end()) {
            nettingSetNPV_[nettingSetId] = vector<vector<Real>>(dates, vector<Real>(samples, 0.0));
            nettingSetFLOW_[nettingSetId] = vector<vector<Real>>(dates, vector<Real>(samples, 0.0));
            nettingSetDIM_[nettingSetId] = vector<vector<Real>>(dates, vector<Real>(samples, 0.0));
            nettingSetDeltaNPV_[nettingSetId] = vector<vector<Real>>(dates, vector<Real>(samples, 0.0));
            regressorArray_[nettingSetId] = vector<vector<Array>>(dates, vector<Array>(samples));
            nettingSetLocalDIM_[nettingSetId] = vector<vector<Real>>(dates, vector<Real>(samples, 0.0));
            nettingSetExpectedDIM_[nettingSetId] = vector<Real>(dates, 0.0);
            nettingSetZeroOrderDIM_[nettingSetId] = vector<Real>(dates, 0.0);
            nettingSetSimpleDIMh_[nettingSetId] = vector<Real>(dates, 0.0);
            nettingSetSimpleDIMp_[nettingSetId] = vector<Real>(dates, 0.0);
            nettingSets.insert(nettingSetId);
            nettingSetSize[nettingSetId] = 0;
        }
        nettingSetSize[nettingSetId]++;

        for (Size j = 0; j < dates; ++j) {
            for (Size k = 0; k < samples; ++k) {
                Real npv = cube_->get(i, j, k);
                QL_REQUIRE(cube_->depth() > 1, "cube depth > 1 expected for DIM, found depth " << cube_->depth());
                Real flow = cube_->get(i, j, k, 1);
                nettingSetNPV_[nettingSetId][j][k] += npv;
                nettingSetFLOW_[nettingSetId][j][k] += flow;
            }
        }
    }

    vector<string> nettingSetIds;
    for (auto n : nettingSets)
        nettingSetIds.push_back(n);

    // Perform the T0 calculation
    performT0DimCalc();

    // This is allocated here and not outside the post processor because we determine the dimension (netting sets) here
    dimCube_ = boost::make_shared<SinglePrecisionInMemoryCube>(today, nettingSetIds, cube_->dates(), samples);

    Size polynomOrder = dimRegressionOrder_;
    LOG("DIM regression polynom order = " << dimRegressionOrder_);
    LsmBasisSystem::PolynomType polynomType = LsmBasisSystem::Monomial;
    Size regressionDimension = dimRegressors_.empty() ? 1 : dimRegressors_.size();
    LOG("DIM regression dimension = " << regressionDimension);
    std::vector<boost::function1<Real, Array>> v(
        LsmBasisSystem::multiPathBasisSystem(regressionDimension, polynomOrder, polynomType));
    Real confidenceLevel = QuantLib::InverseCumulativeNormal()(dimQuantile_);
    LOG("DIM confidence level " << confidenceLevel);

    Size simple_dim_index_h = Size(floor(dimQuantile_ * (samples - 1) + 0.5));
    Size simple_dim_index_p = Size(floor((1.0 - dimQuantile_) * (samples - 1) + 0.5));

    Size nettingSetCount = 0;
    for (auto n : nettingSets) {
        LOG("Process netting set " << n);
        // Set last date's IM to zero for all samples
        for (Size k = 0; k < samples; ++k) {
            nettingSetDIM_[n][dates - 1][k] = 0.0;
            nettingSetLocalDIM_[n][dates - 1][k] = 0.0;
            nettingSetDeltaNPV_[n][dates - 1][k] = 0.0;
        }
        for (Size j = 0; j < dates - 1; ++j) {
            accumulator_set<double, stats<tag::mean, tag::variance>> accDiff;
            accumulator_set<double, stats<tag::mean>> accOneOverNumeraire;
            for (Size k = 0; k < samples; ++k) {
                Real num1 = scenarioData_->get(j, k, AggregationScenarioDataType::Numeraire);
                Real num2 = scenarioData_->get(j + 1, k, AggregationScenarioDataType::Numeraire);
                Real npv1 = nettingSetNPV_[n][j][k];
                Real flow = nettingSetFLOW_[n][j][k];
                Real npv2 = nettingSetNPV_[n][j + 1][k];
                accDiff(npv2 * num2 + flow * num1 - npv1 * num1);
                accOneOverNumeraire(1.0 / num1);
            }

            Date d1 = cube_->dates()[j];
            Date d2 = cube_->dates()[j + 1];
            Real horizonScaling = sqrt(1.0 * dimHorizonCalendarDays_ / (d2 - d1));
            Real stdevDiff = sqrt(variance(accDiff));
            Real E_OneOverNumeraire =
                mean(accOneOverNumeraire); // "re-discount" (the stdev is calculated on non-discounted deltaNPVs)

            nettingSetZeroOrderDIM_[n][j] = stdevDiff * horizonScaling * confidenceLevel;
            nettingSetZeroOrderDIM_[n][j] *= E_OneOverNumeraire;

            vector<Real> rx0(samples, 0.0);
            vector<Array> rx(samples, Array());
            vector<Real> ry1(samples, 0.0);
            vector<Real> ry2(samples, 0.0);
            for (Size k = 0; k < samples; ++k) {
                Real num1 = scenarioData_->get(j, k, AggregationScenarioDataType::Numeraire);
                Real num2 = scenarioData_->get(j + 1, k, AggregationScenarioDataType::Numeraire);
                Real x = nettingSetNPV_[n][j][k] * num1;
                Real f = nettingSetFLOW_[n][j][k] * num1;
                Real y = nettingSetNPV_[n][j + 1][k] * num2;
                Real z = (y + f - x);
                rx[k] = dimRegressors_.empty() ? Array(1, nettingSetNPV_[n][j][k]) : regressorArray(n, j, k);
                rx0[k] = rx[k][0];
                ry1[k] = z;     // for local regression
                ry2[k] = z * z; // for least squares regression
                nettingSetDeltaNPV_[n][j][k] = z;
                regressorArray_[n][j][k] = rx[k];
            }
            vector<Real> delNpvVec_copy = nettingSetDeltaNPV_[n][j];
            sort(delNpvVec_copy.begin(), delNpvVec_copy.end());
            Real simpleDim_h = delNpvVec_copy[simple_dim_index_h];
            Real simpleDim_p = delNpvVec_copy[simple_dim_index_p];
            simpleDim_h *= horizonScaling;                                  // the usual scaling factors
            simpleDim_p *= horizonScaling;                                  // the usual scaling factors
            nettingSetSimpleDIMh_[n][j] = simpleDim_h * E_OneOverNumeraire; // discounted DIM
            nettingSetSimpleDIMp_[n][j] = simpleDim_p * E_OneOverNumeraire; // discounted DIM

            QL_REQUIRE(rx.size() > v.size(), "not enough points for regression with polynom order " << polynomOrder);
            if (close_enough(stdevDiff, 0.0)) {
                LOG("DIM: Zero std dev estimation at step " << j);
                // Skip IM calculation if all samples have zero NPV (e.g. after latest maturity)
                for (Size k = 0; k < samples; ++k) {
                    nettingSetDIM_[n][j][k] = 0.0;
                    nettingSetLocalDIM_[n][j][k] = 0.0;
                }
            } else {
                // Least squares polynomial regression with specified polynom order
                QuantExt::StabilisedGLLS ls(rx, ry2, v, QuantExt::StabilisedGLLS::MeanStdDev);
                LOG("DIM data normalisation at time step "
                    << j << ": " << scientific << setprecision(6) << " x-shift = " << ls.xShift() << " x-multiplier = "
                    << ls.xMultiplier() << " y-shift = " << ls.yShift() << " y-multiplier = " << ls.yMultiplier());
                LOG("DIM regression coefficients at time step " << j << ": " << fixed << setprecision(6)
                                                                << ls.transformedCoefficients());

                // Local regression versus first regression variable (i.e. we do not perform a
                // multidimensional local regression):
                // We evaluate this at a limited number of samples only for validation purposes.
                // Note that computational effort scales quadratically with number of samples.
                // NadarayaWatson needs a large number of samples for good results.
                QuantExt::NadarayaWatson lr(rx0.begin(), rx0.end(), ry1.begin(),
                                            GaussianKernel(0.0, dimLocalRegressionBandwidth_));
                Size localRegressionSamples = samples;
                if (dimLocalRegressionEvaluations_ > 0)
                    localRegressionSamples = Size(floor(1.0 * samples / dimLocalRegressionEvaluations_ + .5));

                // Evaluate regression function to compute DIM for each scenario
                for (Size k = 0; k < samples; ++k) {
                    Real num1 = scenarioData_->get(j, k, AggregationScenarioDataType::Numeraire);
                    Array regressor =
                        dimRegressors_.empty() ? Array(1, nettingSetNPV_[n][j][k]) : regressorArray(n, j, k);
                    Real e = ls.eval(regressor, v);
                    if (e < 0.0)
                        LOG("Negative variance regression for date " << j << ", sample " << k
                                                                     << ", regressor = " << regressor);

                    // Note:
                    // 1) We assume vanishing mean of "z", because the drift over a MPOR is usually small,
                    //    and to avoid a second regression for the conditional mean
                    // 2) In particular the linear regression function can yield negative variance values in
                    //    extreme scenarios where an exact analytical or delta VaR calculation would yield a
                    //    variance aproaching zero. We correct this here by taking the positive part.
                    Real std = sqrt(std::max(e, 0.0));
                    Real scalingFactor = horizonScaling * confidenceLevel * dimScaling_;
                    Real dim = std * scalingFactor / num1;
                    dimCube_->set(dim, nettingSetCount, j, k);
                    nettingSetDIM_[n][j][k] = dim;
                    nettingSetExpectedDIM_[n][j] += dim / samples;

                    // Evaluate the Kernel regression for a subset of the samples only (performance)
                    if (k % localRegressionSamples == 0)
                        nettingSetLocalDIM_[n][j][k] = lr.standardDeviation(regressor[0]) * scalingFactor / num1;
                    else
                        nettingSetLocalDIM_[n][j][k] = 0.0;
                }
            }
        }

        nettingSetCount++;
    }
    LOG("DIM by regression done");
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

void PostProcess::performT0DimCalc() {
    // In this function we proxy the model-implied T0 IM by looking at the
    // cube grid horizon lying closest to t0+mpor. We measure diffs relative
    // to the mean of the distribution at this same time horizon, thus avoiding
    // any cashflow-specific jumps

    Date today = market_->asofDate();
    Size relevantDateIdx = 0;
    Real sqrtTimeScaling = 1.0;
    for (Size i = 0; i < cube_->dates().size(); ++i) {
        Size daysFromT0 = (cube_->dates()[i] - today);
        if (daysFromT0 < dimHorizonCalendarDays_) {
            // iterate until we straddle t0+mpor
            continue;
        } else if (daysFromT0 == dimHorizonCalendarDays_) {
            // this date corresponds to t0+mpor, so use it
            relevantDateIdx = i;
            sqrtTimeScaling = 1.0;
            break;
        } else if (daysFromT0 > dimHorizonCalendarDays_) {
            // the first date greater than t0+MPOR, check if it is closest
            Size lastIdx = (i == 0) ? 0 : (i - 1);
            Size lastDaysFromT0 = (cube_->dates()[lastIdx] - today);
            if (std::fabs(daysFromT0 - dimHorizonCalendarDays_) <=
                std::fabs(lastDaysFromT0 - dimHorizonCalendarDays_)) {
                relevantDateIdx = i;
                sqrtTimeScaling = std::sqrt(Real(dimHorizonCalendarDays_) / Real(daysFromT0));
            } else {
                relevantDateIdx = lastIdx;
                sqrtTimeScaling = std::sqrt(Real(dimHorizonCalendarDays_) / Real(lastDaysFromT0));
            }
            break;
        }
    }
    // set some reasonable bounds on the sqrt time scaling, so that we are not looking at a ridiculous time horizon
    QL_REQUIRE((sqrtTimeScaling * sqrtTimeScaling >= 0.5) && (sqrtTimeScaling * sqrtTimeScaling <= 2.0),
               "T0 IM Estimation - The estimation time horizon from grid "
                   << "is not sufficiently close to t0+MPOR - "
                   << QuantLib::io::iso_date(cube_->dates()[relevantDateIdx]));

    // TODO: Ensure that the simulation containers read-from below are indeed populated

    Real confidenceLevel = QuantLib::InverseCumulativeNormal()(dimQuantile_);
    Size simple_dim_index_h = Size(floor(dimQuantile_ * (cube_->samples() - 1) + 0.5));
    for (auto it_map = nettingSetNPV_.begin(); it_map != nettingSetNPV_.end(); ++it_map) {
        string key = it_map->first;
        boost::shared_ptr<NettingSetDefinition> nettingObj = nettingSetManager_->get(key);
        vector<Real> t0_dist = it_map->second[relevantDateIdx];
        Size dist_size = t0_dist.size();
        QL_REQUIRE(dist_size == cube_->samples(),
                   "T0 IM - cube samples size mismatch - " << dist_size << ", " << cube_->samples());
        Real mean_t0_dist = std::accumulate(t0_dist.begin(), t0_dist.end(), 0.0);
        mean_t0_dist /= dist_size;
        vector<Real> t0_delMtM_dist(dist_size, 0.0);
        accumulator_set<double, stats<tag::mean, tag::variance>> acc_delMtm;
        accumulator_set<double, stats<tag::mean>> acc_OneOverNum;
        for (Size i = 0; i < dist_size; ++i) {
            Real numeraire = scenarioData_->get(relevantDateIdx, i, AggregationScenarioDataType::Numeraire);
            Real deltaMtmFromMean = numeraire * (t0_dist[i] - mean_t0_dist) * sqrtTimeScaling;
            t0_delMtM_dist[i] = deltaMtmFromMean;
            acc_delMtm(deltaMtmFromMean);
            acc_OneOverNum(1.0 / numeraire);
        }
        Real E_OneOverNumeraire = mean(acc_OneOverNum);
        Real variance_t0 = variance(acc_delMtm);
        Real sqrt_t0 = sqrt(variance_t0);
        net_t0_im_reg_h_[key] = (sqrt_t0 * confidenceLevel * E_OneOverNumeraire);
        std::sort(t0_delMtM_dist.begin(), t0_delMtM_dist.end());
        net_t0_im_simple_h_[key] = (t0_delMtM_dist[simple_dim_index_h] * E_OneOverNumeraire);

        LOG("T0 IM (Reg) - {" << key << "} = " << net_t0_im_reg_h_[key]);
        LOG("T0 IM (Simple) - {" << key << "} = " << net_t0_im_simple_h_[key]);
    }
    LOG("T0 IM Calculations Completed");
}

void PostProcess::exportDimEvolution(const std::string& nettingSet, ore::data::Report& dimEvolutionReport) {
    LOG("Export DIM evolution for netting set " << nettingSet);

    Size dates = dimCube_->dates().size();
    Size samples = dimCube_->samples();
    const std::vector<std::string>& ids = dimCube_->ids();

    int index = -1;
    for (Size i = 0; i < ids.size(); ++i) {
        if (ids[i] == nettingSet) {
            index = i;
            break;
        }
    }
    QL_REQUIRE(index >= 0, "netting set " << nettingSet << " not found in DIM cube");

    dimEvolutionReport.addColumn("TimeStep", Size())
        .addColumn("Date", Date())
        .addColumn("DaysInPeriod", Size())
        .addColumn("ZeroOrderDIM", Real(), 6)
        .addColumn("AverageDIM", Real(), 6)
        .addColumn("AverageFLOW", Real(), 6)
        .addColumn("SimpleDIM", Real(), 6);

    for (Size i = 0; i < dates - 1; ++i) {
        Real expectedFlow = 0.0;
        for (Size j = 0; j < samples; ++j) {
            expectedFlow += nettingSetFLOW_[nettingSet][i][j] / samples;
        }

        Date d1 = dimCube_->dates()[i];
        Date d2 = dimCube_->dates()[i + 1];
        Size days = d2 - d1;
        dimEvolutionReport.next()
            .add(i)
            .add(d1)
            .add(days)
            .add(nettingSetZeroOrderDIM_[nettingSet][i])
            .add(nettingSetExpectedDIM_[nettingSet][i])
            .add(expectedFlow)
            .add(nettingSetSimpleDIMh_[nettingSet][i]);
    }
    dimEvolutionReport.end();
    LOG("Exporting expected DIM through time done");
}

bool lessThan(const Array& a, const Array& b) {
    QL_REQUIRE(a.size() > 0, "array a is empty");
    QL_REQUIRE(b.size() > 0, "array a is empty");
    return a[0] < b[0];
}

void PostProcess::exportDimRegression(const std::string& nettingSet, const std::vector<Size>& timeSteps,
                                      const std::vector<boost::shared_ptr<ore::data::Report>>& dimRegReports) {

    QL_REQUIRE(dimRegReports.size() == timeSteps.size(),
               "number of file names (" << dimRegReports.size() << ") does not match number of time steps ("
                                        << timeSteps.size() << ")");
    for (Size ii = 0; ii < timeSteps.size(); ++ii) {
        Size timeStep = timeSteps[ii];
        LOG("Export DIM by sample for netting set " << nettingSet << " and time step " << timeStep);

        Size dates = dimCube_->dates().size();
        const std::vector<std::string>& ids = dimCube_->ids();

        int index = -1;
        for (Size i = 0; i < ids.size(); ++i) {
            if (ids[i] == nettingSet) {
                index = i;
                break;
            }
        }
        QL_REQUIRE(index >= 0, "netting set " << nettingSet << " not found in DIM cube");

        QL_REQUIRE(timeStep < dates - 1, "selected time step " << timeStep << " out of range [0, " << dates - 1 << "]");

        Size samples = cube_->samples();
        vector<Real> numeraires(samples, 0.0);
        for (Size k = 0; k < samples; ++k)
            numeraires[k] = scenarioData_->get(timeStep, k, AggregationScenarioDataType::Numeraire);

        auto p = sort_permutation(regressorArray_[nettingSet][timeStep], lessThan);
        vector<Array> reg = apply_permutation(regressorArray_[nettingSet][timeStep], p);
        vector<Real> dim = apply_permutation(nettingSetDIM_[nettingSet][timeStep], p);
        vector<Real> ldim = apply_permutation(nettingSetLocalDIM_[nettingSet][timeStep], p);
        vector<Real> delta = apply_permutation(nettingSetDeltaNPV_[nettingSet][timeStep], p);
        vector<Real> num = apply_permutation(numeraires, p);

        boost::shared_ptr<ore::data::Report> regReport = dimRegReports[ii];
        regReport->addColumn("Sample", Size());
        for (Size k = 0; k < reg[0].size(); ++k) {
            ostringstream o;
            o << "Regressor_" << k << "_";
            o << (dimRegressors_.empty() ? "NPV" : dimRegressors_[k]);
            regReport->addColumn(o.str(), Real(), 6);
        }
        regReport->addColumn("RegressionDIM", Real(), 6)
            .addColumn("LocalDIM", Real(), 6)
            .addColumn("ExpectedDIM", Real(), 6)
            .addColumn("ZeroOrderDIM", Real(), 6)
            .addColumn("DeltaNPV", Real(), 6)
            .addColumn("SimpleDIM", Real(), 6);

        // Note that RegressionDIM, LocalDIM, DeltaNPV are _not_ reduced by the numeraire in this output,
        // but ExpectedDIM, ZeroOrderDIM and SimpleDIM _are_ reduced by the numeraire.
        // This is so that the regression formula can be manually validated

        for (Size j = 0; j < reg.size(); ++j) {
            regReport->next().add(j);
            for (Size k = 0; k < reg[j].size(); ++k)
                regReport->add(reg[j][k]);
            regReport->add(dim[j] * num[j])
                .add(ldim[j] * num[j])
                .add(nettingSetExpectedDIM_[nettingSet][timeStep])
                .add(nettingSetZeroOrderDIM_[nettingSet][timeStep])
                .add(delta[j])
                .add(nettingSetSimpleDIMh_[nettingSet][timeStep]);
        }
        regReport->end();
        LOG("Exporting DIM by Sample done for");
    }
}
} // namespace analytics
} // namespace ore
