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

#include <orea/aggregation/exposurecalculator.hpp>
#include <orea/cube/inmemorycube.hpp>

#include <ored/portfolio/trade.hpp>

#include <ql/time/date.hpp>
#include <ql/time/calendars/weekendsonly.hpp>

using namespace std;
using namespace QuantLib;

namespace ore {
namespace analytics {

ExposureCalculator::ExposureCalculator(
    const QuantLib::ext::shared_ptr<Portfolio>& portfolio, const QuantLib::ext::shared_ptr<NPVCube>& cube,
    const QuantLib::ext::shared_ptr<CubeInterpretation> cubeInterpretation,
    const QuantLib::ext::shared_ptr<Market>& market,
    bool exerciseNextBreak, const string& baseCurrency, const string& configuration,
    const Real quantile, const CollateralExposureHelper::CalculationType calcType, const bool multiPath,
    const bool flipViewXVA)
    : portfolio_(portfolio), cube_(cube), cubeInterpretation_(cubeInterpretation),
       market_(market), exerciseNextBreak_(exerciseNextBreak),
      baseCurrency_(baseCurrency), configuration_(configuration),
      quantile_(quantile), calcType_(calcType),
      multiPath_(multiPath), dates_(cube->dates()),
      today_(market_->asofDate()), dc_(ActualActual(ActualActual::ISDA)), flipViewXVA_(flipViewXVA) {

    QL_REQUIRE(portfolio_, "portfolio is null");

    if (multiPath) {
        exposureCube_ = QuantLib::ext::make_shared<SinglePrecisionInMemoryCubeN>(
            market->asofDate(), portfolio_->ids(), dates_,
            cube_->samples(), EXPOSURE_CUBE_DEPTH);// EPE, ENE, allocatedEPE, allocatedENE
    } else {
        exposureCube_ = QuantLib::ext::make_shared<DoublePrecisionInMemoryCubeN>(
            market->asofDate(), portfolio_->ids(), dates_,
            1, EXPOSURE_CUBE_DEPTH);// EPE, ENE, allocatedEPE, allocatedENE
    }

    set<string> nettingSetIdsSet;
    for (const auto& [tradeId, trade] : portfolio->trades())
        nettingSetIdsSet.insert(trade->envelope().nettingSetId());
    nettingSetIds_= vector<string>(nettingSetIdsSet.begin(), nettingSetIdsSet.end());

    times_ = vector<Real>(dates_.size(), 0.0);
    for (Size i = 0; i < dates_.size(); i++)
        times_[i] = dc_.yearFraction(today_, cube_->dates()[i]);

    isRegularCubeStorage_ = !cubeInterpretation_->withCloseOutLag();
}

void ExposureCalculator::build() {
    LOG("Compute trade exposure profiles, " << (flipViewXVA_ ? "inverted (flipViewXVA = Y)" : "regular (flipViewXVA = N)"));
    size_t i = 0;
    for (auto tradeIt = portfolio_->trades().begin(); tradeIt != portfolio_->trades().end(); ++tradeIt, ++i) {
        auto trade = tradeIt->second;
        string tradeId = tradeIt->first;
        string nettingSetId = trade->envelope().nettingSetId();
        LOG("Aggregate exposure for trade " << tradeId);
        if (nettingSetDefaultValue_.find(nettingSetId) == nettingSetDefaultValue_.end()) {
            nettingSetDefaultValue_[nettingSetId] = vector<vector<Real>>(dates_.size(), vector<Real>(cube_->samples(), 0.0));
            nettingSetCloseOutValue_[nettingSetId] = vector<vector<Real>>(dates_.size(), vector<Real>(cube_->samples(), 0.0));
            nettingSetMporPositiveFlow_[nettingSetId] = vector<vector<Real>>(dates_.size(), vector<Real>(cube_->samples(), 0.0));
            nettingSetMporNegativeFlow_[nettingSetId] = vector<vector<Real>>(dates_.size(), vector<Real>(cube_->samples(), 0.0));
        }

        // Identify the next break date if provided, default is trade maturity.
        Date nextBreakDate = trade->maturity();
        TradeActions ta = trade->tradeActions();
        if (exerciseNextBreak_ && !ta.empty()) {
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
        Real npv0;
        if (flipViewXVA_) {
            npv0 = -cube_->getT0(i);
        } else {
            npv0 = cube_->getT0(i);
        }
        vector<Real> epe(dates_.size() + 1, 0.0);
        vector<Real> ene(dates_.size() + 1, 0.0);
        vector<Real> ee_b(dates_.size() + 1, 0.0);
        vector<Real> eee_b(dates_.size() + 1, 0.0);
        vector<Real> pfe(dates_.size() + 1, 0.0);
        epe[0] = std::max(npv0, 0.0);
        ene[0] = std::max(-npv0, 0.0);
        ee_b[0] = epe[0];
        eee_b[0] = ee_b[0];
        pfe[0] = std::max(npv0, 0.0);
        exposureCube_->setT0(epe[0], tradeId, ExposureIndex::EPE);
        exposureCube_->setT0(ene[0], tradeId, ExposureIndex::ENE);
        for (Size j = 0; j < dates_.size(); ++j) {
            Date d = cube_->dates()[j];
            vector<Real> distribution(cube_->samples(), 0.0);
            for (Size k = 0; k < cube_->samples(); ++k) {
                // RL 2020-07-17
                // 1) If the calculation type is set to NoLag:
                //    Collateral balances are NOT delayed by the MPoR, but we use the close-out NPV.
                // 2) Otherwise:
                //    Collateral balances are delayed by the MPoR (if possible, i.e. the valuation
                //    grid has MPoR spacing), and we use the default date NPV.
                //    This is the treatment in the ORE releases up to June 2020).
                Real defaultValue =
                    d > nextBreakDate && exerciseNextBreak_ ? 0.0 : cubeInterpretation_->getDefaultNpv(cube_, i, j, k);
                Real closeOutValue;
                if (isRegularCubeStorage_ && j == dates_.size() - 1)
                    closeOutValue = defaultValue;
                else
                    closeOutValue = d > nextBreakDate && exerciseNextBreak_
                                        ? 0.0
                                        : cubeInterpretation_->getCloseOutNpv(cube_, i, j, k);

                Real positiveCashFlow = cubeInterpretation_->getMporPositiveFlows(cube_, i, j, k);
                Real negativeCashFlow = cubeInterpretation_->getMporNegativeFlows(cube_, i, j, k);
                //for single trade exposures, always default value is relevant
                Real npv = defaultValue;
                epe[j + 1] += max(npv, 0.0) / cube_->samples();
                ene[j + 1] += max(-npv, 0.0) / cube_->samples();
                nettingSetDefaultValue_[nettingSetId][j][k] += defaultValue;
                nettingSetCloseOutValue_[nettingSetId][j][k] += closeOutValue;
                nettingSetMporPositiveFlow_[nettingSetId][j][k] += positiveCashFlow;
                nettingSetMporNegativeFlow_[nettingSetId][j][k] += negativeCashFlow;
                distribution[k] = npv;
                if (multiPath_) {
                    exposureCube_->set(max(npv, 0.0), tradeId, d, k, ExposureIndex::EPE);
                    exposureCube_->set(max(-npv, 0.0), tradeId, d, k, ExposureIndex::ENE);
                }
            }
            if (!multiPath_) {
                exposureCube_->set(epe[j + 1], tradeId, d, 0, ExposureIndex::EPE);
                exposureCube_->set(ene[j + 1], tradeId, d, 0, ExposureIndex::ENE);
            }
            ee_b[j + 1] = epe[j + 1] / curve->discount(cube_->dates()[j]);
            eee_b[j + 1] = std::max(eee_b[j], ee_b[j + 1]);
            std::sort(distribution.begin(), distribution.end());
            Size index = Size(floor(quantile_ * (cube_->samples() - 1) + 0.5));
            pfe[j + 1] = std::max(distribution[index], 0.0);
        }
        ee_b_[tradeId] = ee_b;
        eee_b_[tradeId] = eee_b;
        pfe_[tradeId] = pfe;

        Real epe_b = 0.0;
        Real eepe_b = 0.0;

        Size t = 0;
        Calendar cal = WeekendsOnly();
        /*The time average in the EEPE calculation is taken over the first year of the exposure evolution
        (or until maturity if all positions of the netting set mature before one year).
        This one year point is actually taken to be today+1Y+4D, so that the 1Y point on the dateGrid is always
        included.
        This may effect DateGrids with daily data points*/
        Date maturity = std::min(cal.adjust(today_ + 1 * Years + 4 * Days), trade->maturity());
        QuantLib::Real maturityTime = dc_.yearFraction(today_, maturity);

        while (t < dates_.size() && times_[t] <= maturityTime)
            ++t;

        if (t > 0) {
            vector<double> weights(t);
            weights[0] = times_[0];
            for (Size k = 1; k < t; k++)
                weights[k] = times_[k] - times_[k - 1];
            double totalWeights = std::accumulate(weights.begin(), weights.end(), 0.0);
            for (Size k = 0; k < t; k++)
                weights[k] /= totalWeights;

            for (Size k = 0; k < t; k++) {
                epe_b += ee_b[k] * weights[k];
                eepe_b += eee_b[k] * weights[k];
            }
        }
        epe_b_[tradeId] = epe_b;
        eepe_b_[tradeId] = eepe_b;
    }
}

vector<Real> ExposureCalculator::getMeanExposure(const string& tid, ExposureIndex index) {
    vector<Real> exp(dates_.size() + 1, 0.0);
    exp[0] = exposureCube_->getT0(tid, index);
    for (Size i = 0; i < dates_.size(); i++) {
        for (Size k = 0; k < exposureCube_->samples(); k++) {
            exp[i + 1] += exposureCube_->get(tid, dates_[i], k, index);
        }
        exp[i + 1] /= exposureCube_->samples();
    }
    return exp;
}


} // namespace analytics
} // namespace ore
