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

#include <orea/scenario/crossassetmodelscenariogenerator.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>

#include <qle/indexes/inflationindexobserver.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;

namespace ore {
namespace analytics {

using namespace QuantExt::CrossAssetModelTypes;

CrossAssetModelScenarioGenerator::CrossAssetModelScenarioGenerator(
    boost::shared_ptr<QuantExt::CrossAssetModel> model,
    boost::shared_ptr<QuantExt::MultiPathGeneratorBase> pathGenerator,
    boost::shared_ptr<ScenarioFactory> scenarioFactory, boost::shared_ptr<ScenarioSimMarketParameters> simMarketConfig,
    Date today, boost::shared_ptr<DateGrid> grid, boost::shared_ptr<ore::data::Market> initMarket,
    const std::string& configuration)
    : ScenarioPathGenerator(today, grid->dates(), grid->timeGrid()), model_(model), pathGenerator_(pathGenerator),
      scenarioFactory_(scenarioFactory), simMarketConfig_(simMarketConfig), initMarket_(initMarket),
      configuration_(configuration) {

    QL_REQUIRE(initMarket != NULL, "CrossAssetScenarioGenerator: initMarket is null");
    QL_REQUIRE(timeGrid_.size() == dates_.size() + 1, "date/time grid size mismatch");

    // TODO, curve tenors might be overwritten by dates in simMarketConfig_, here we just take the tenors

    DayCounter dc = model_->irlgm1f(0)->termStructure()->dayCounter();
    n_ccy_ = model_->components(IR);
    n_eq_ = model_->components(EQ);
    n_inf_ = model_->components(INF);
    n_cr_ = model_->components(CR);
    n_indices_ = simMarketConfig_->indices().size();
    n_curves_ = simMarketConfig_->yieldCurveNames().size();

    // Cache yield curve keys
    discountCurveKeys_.reserve(n_ccy_ * simMarketConfig_->yieldCurveTenors("").size());
    for (Size j = 0; j < model_->components(IR); j++) {
        std::string ccy = model_->irlgm1f(j)->currency().code();
        ten_dsc_.push_back(simMarketConfig_->yieldCurveTenors(ccy));
        Size n_ten = ten_dsc_.back().size();
        for (Size k = 0; k < n_ten; k++)
            discountCurveKeys_.emplace_back(RiskFactorKey::KeyType::DiscountCurve, ccy, k); // j * n_ten + k
    }

    // Cache index curve keys
    indexCurveKeys_.reserve(n_indices_ * simMarketConfig_->yieldCurveTenors("").size());
    for (Size j = 0; j < n_indices_; ++j) {
        ten_idx_.push_back(simMarketConfig_->yieldCurveTenors(simMarketConfig_->indices()[j]));
        Size n_ten = ten_idx_.back().size();
        for (Size k = 0; k < n_ten; ++k) {
            indexCurveKeys_.emplace_back(RiskFactorKey::KeyType::IndexCurve, simMarketConfig_->indices()[j], k);
        }
    }

    // Cache yield curve keys
    Size n_curves = simMarketConfig_->yieldCurveNames().size();
    yieldCurveKeys_.reserve(n_curves * simMarketConfig_->yieldCurveTenors("").size());
    for (Size j = 0; j < n_curves; ++j) {
        ten_yc_.push_back(simMarketConfig_->yieldCurveTenors(simMarketConfig_->yieldCurveNames()[j]));
        Size n_ten = ten_yc_.back().size();
        for (Size k = 0; k < n_ten; ++k) {
            yieldCurveKeys_.emplace_back(RiskFactorKey::KeyType::YieldCurve, simMarketConfig_->yieldCurveNames()[j], k);
        }
    }

    // Cache FX rate keys
    fxKeys_.reserve(n_ccy_ - 1);
    for (Size k = 0; k < n_ccy_ - 1; k++) {
        const string& foreign = model_->irlgm1f(k + 1)->currency().code(); // Is this safe?
        const string& domestic = model_->irlgm1f(0)->currency().code();
        fxKeys_.emplace_back(RiskFactorKey::KeyType::FXSpot, foreign + domestic); // k
    }

    // set up CrossAssetModelImpliedFxVolTermStructures
    if (simMarketConfig_->simulateFXVols()) {
        LOG("CrossAssetModel is simulating FX vols");
        for (Size k = 0; k < simMarketConfig_->fxVolCcyPairs().size(); k++) {
            // Calculating the index is messy
            const string pair = simMarketConfig_->fxVolCcyPairs()[k];
            LOG("Set up CrossAssetModelImpliedFxVolTermStructures for " << pair);
            QL_REQUIRE(pair.size() == 6, "Invalid ccypair " << pair);
            const string& domestic = pair.substr(0, 3);
            const string& foreign = pair.substr(3);
            QL_REQUIRE(domestic == model_->irlgm1f(0)->currency().code(), "Only DOM-FOR fx vols supported");
            Size index = model->ccyIndex(parseCurrency(foreign)); // will throw if foreign not there
            QL_REQUIRE(index > 0, "Invalid index for ccy " << foreign << " should be > 0");
            // fxVols_ are indexed by ccyPairs
            LOG("Pair " << pair << " index " << index);
            // index - 1 to convert "IR" index into an "FX" index
            fxVols_.push_back(boost::make_shared<CrossAssetModelImpliedFxVolTermStructure>(model_, index - 1));
            LOG("Set up CrossAssetModelImpliedFxVolTermStructures for " << pair << " done");
        }
    }

    // Cache EQ rate keys
    Size n_eq = model_->components(EQ);
    eqKeys_.reserve(n_eq);
    for (Size k = 0; k < n_eq; k++) {
        const string& eqName = model_->eqbs(k)->name();
        eqKeys_.emplace_back(RiskFactorKey::KeyType::EquitySpot, eqName);
    }

    // equity vols
    if (simMarketConfig_->equityVolNames().size() > 0 && simMarketConfig_->simulateEquityVols()) {
        LOG("CrossAssetModel is simulating EQ vols");
        for (Size k = 0; k < simMarketConfig_->equityVolNames().size(); k++) {
            // Calculating the index is messy
            const string equityName = simMarketConfig_->equityVolNames()[k];
            LOG("Set up CrossAssetModelImpliedEqVolTermStructures for " << equityName);
            Size eqIndex = model->eqIndex(equityName);
            // eqVols_ are indexed by ccyPairs
            LOG("EQ Vol Name = " << equityName << ", index = " << eqIndex);
            // index - 1 to convert "IR" index into an "FX" index
            eqVols_.push_back(boost::make_shared<CrossAssetModelImpliedEqVolTermStructure>(model_, eqIndex));
            LOG("Set up CrossAssetModelImpliedEqVolTermStructures for " << equityName << " done");
        }
    }

    // Cache INF rate keys
    Size n_inf = model_->components(INF);
    if (n_inf > 0) {
        cpiKeys_.reserve(n_inf);
        for (Size j = 0; j < n_inf; ++j) {
            cpiKeys_.emplace_back(RiskFactorKey::KeyType::CPIIndex, model->inf(j)->name());
        }

        Size n_zeroinf = simMarketConfig_->zeroInflationIndices().size();
        if (n_zeroinf > 0) {
            zeroInflationKeys_.reserve(n_zeroinf * simMarketConfig_->zeroInflationTenors("").size());
            for (Size j = 0; j < n_zeroinf; ++j) {
                ten_zinf_.push_back(simMarketConfig_->zeroInflationTenors(simMarketConfig_->zeroInflationIndices()[j]));
                Size n_ten = ten_zinf_.back().size();
                for (Size k = 0; k < n_ten; ++k) {
                    zeroInflationKeys_.emplace_back(RiskFactorKey::KeyType::ZeroInflationCurve,
                                                    simMarketConfig_->zeroInflationIndices()[j], k);
                }
            }
        }

        Size n_yoyinf = simMarketConfig_->yoyInflationIndices().size();
        if (n_yoyinf > 0) {
            yoyInflationKeys_.reserve(n_yoyinf * simMarketConfig_->yoyInflationTenors("").size());
            for (Size j = 0; j < n_yoyinf; ++j) {
                ten_yinf_.push_back(simMarketConfig_->yoyInflationTenors(simMarketConfig_->yoyInflationIndices()[j]));
                Size n_ten = ten_yinf_.back().size();
                for (Size k = 0; k < n_ten; ++k) {
                    yoyInflationKeys_.emplace_back(RiskFactorKey::KeyType::YoYInflationCurve,
                                                   simMarketConfig_->yoyInflationIndices()[j], k);
                }
            }
        }
    }

    // Cache default curve keys
    Size n_cr = model_->components(CR);
    defaultCurveKeys_.reserve(n_cr * simMarketConfig_->defaultTenors("").size());
    for (Size j = 0; j < model_->components(CR); j++) {
        std::string cr_name = model_->cr(j)->name();
        ten_dfc_.push_back(simMarketConfig_->defaultTenors(cr_name));
        Size n_ten = ten_dfc_.back().size();
        for (Size k = 0; k < n_ten; k++) {
            defaultCurveKeys_.emplace_back(RiskFactorKey::KeyType::SurvivalProbability, cr_name, k); // j * n_ten + k
        }
    }

    // cache curves

    for (Size j = 0; j < n_ccy_; ++j) {
        curves_.push_back(boost::make_shared<QuantExt::LgmImpliedYieldTermStructure>(model_->lgm(j), dc, true));
    }

    for (Size j = 0; j < n_indices_; ++j) {
        std::string indexName = simMarketConfig_->indices()[j];
        boost::shared_ptr<IborIndex> index = *initMarket_->iborIndex(indexName, configuration_);
        Handle<YieldTermStructure> fts = index->forwardingTermStructure();
        auto impliedFwdCurve = boost::make_shared<LgmImpliedYtsFwdFwdCorrected>(
            model_->lgm(model_->ccyIndex(index->currency())), fts, dc, false);
        fwdCurves_.push_back(impliedFwdCurve);
        indices_.push_back(index->clone(Handle<YieldTermStructure>(impliedFwdCurve)));
    }

    for (Size j = 0; j < n_curves_; ++j) {
        std::string curveName = simMarketConfig_->yieldCurveNames()[j];
        Currency ccy = ore::data::parseCurrency(simMarketConfig_->yieldCurveCurrencies().at(curveName));
        Handle<YieldTermStructure> yts = initMarket_->yieldCurve(curveName, configuration_);
        auto impliedYieldCurve =
            boost::make_shared<LgmImpliedYtsFwdFwdCorrected>(model_->lgm(model_->ccyIndex(ccy)), yts, dc, false);
        yieldCurves_.push_back(impliedYieldCurve);
        yieldCurveCurrency_.push_back(ccy);
    }

    // Cache data regarding the zero inflation curves to avoid repeating in date loop below.
    // This vector needs to follow the order of the simMarketConfig_->zeroInflationIndices().
    // 1st element in tuple is the index of the inflation component in the CAM.
    // 2nd element in tuple is the index of the inflation component's currency in the CAM.
    // 3rd element in tuple is the type of inflation model in the CAM.
    // 4th element in tuple is the CAM implied inflation term structure.
    for (const auto& name : simMarketConfig_->zeroInflationIndices()) {
        auto idx = model_->infIndex(name);
        auto ccyIdx = model_->ccyIndex(model_->inf(idx)->currency());
        auto mt = model_->modelType(INF, idx);
        QL_REQUIRE(mt == DK || mt == JY, "CrossAssetModelScenarioGenerator: expected inflation model to be JY or DK.");
        boost::shared_ptr<ZeroInflationModelTermStructure> ts;
        if (mt == DK) {
            ts = boost::make_shared<DkImpliedZeroInflationTermStructure>(model_, idx);
        } else {
            ts = boost::make_shared<JyImpliedZeroInflationTermStructure>(model_, idx);
        }
        zeroInfCurves_.emplace_back(idx, ccyIdx, mt, ts);
    }

    // Same logic here as for the zeroInfCurves above.
    for (const auto& name : simMarketConfig_->yoyInflationIndices()) {
        auto idx = model_->infIndex(name);
        auto ccyIdx = model_->ccyIndex(model_->inf(idx)->currency());
        auto mt = model_->modelType(INF, idx);
        QL_REQUIRE(mt == DK || mt == JY, "CrossAssetModelScenarioGenerator: expected inflation model to be JY or DK.");
        boost::shared_ptr<YoYInflationModelTermStructure> ts;
        if (mt == DK) {
            ts = boost::make_shared<DkImpliedYoYInflationTermStructure>(model_, idx);
        } else {
            ts = boost::make_shared<JyImpliedYoYInflationTermStructure>(model_, idx);
        }
        yoyInfCurves_.emplace_back(idx, ccyIdx, mt, ts);
    }

    for (Size j = 0; j < n_cr_; ++j) {
        if (model_->modelType(CR, j) == ModelType::LGM1F) {
            lgmDefaultCurves_.push_back(boost::make_shared<QuantExt::LgmImpliedDefaultTermStructure>(
                model_, j, model_->ccyIndex(model_->crlgm1f(j)->currency())));
            cirppDefaultCurves_.push_back(boost::shared_ptr<QuantExt::CirppImpliedDefaultTermStructure>());
        } else if (model_->modelType(CR, j) == ModelType::CIRPP) {
            lgmDefaultCurves_.push_back(boost::shared_ptr<QuantExt::LgmImpliedDefaultTermStructure>());
            cirppDefaultCurves_.push_back(
                boost::make_shared<QuantExt::CirppImpliedDefaultTermStructure>(model_->crcirppModel(j), j));
        }
    }

    for (Size j = 0; j < n_eq; ++j) {
        std::string curveName = simMarketConfig_->equityNames()[j];
        Currency ccy = model_->eqbs(j)->currency();
        equityForecastCurrency_.push_back(ccy);
    }
}

std::vector<boost::shared_ptr<Scenario>> CrossAssetModelScenarioGenerator::nextPath() {
    std::vector<boost::shared_ptr<Scenario>> scenarios(dates_.size());
    Sample<MultiPath> sample = pathGenerator_->next();
    DayCounter dc = model_->irlgm1f(0)->termStructure()->dayCounter();

    for (Size i = 0; i < dates_.size(); i++) {
        Real t = timeGrid_[i + 1]; // recall: time grid has inserted t=0

        scenarios[i] = scenarioFactory_->buildScenario(dates_[i]);

        // Set numeraire, numeraire currency and the (deterministic) domestic discount
        // Asset index 0 in sample.value[0][i+1] refers to the domestic currency process.
        Real z0 = sample.value[0][i + 1]; // domestic LGM factor, second index = 0 holds initial values
        if (model_->measure() == Measure::LGM) {
            scenarios[i]->setNumeraire(model_->numeraire(0, t, z0));
        } else if (model_->measure() == Measure::BA) {
            // last state variable is the auxiliary state variable for the bank account measure dynamics
            Real y0 = sample.value[model_->pIdx(AUX, 0)][i + 1];
            scenarios[i]->setNumeraire(model_->bankAccountNumeraire(0, t, z0, y0));
        } else {
            QL_FAIL("Measure not covered");
        }

        // Discount curves
        for (Size j = 0; j < n_ccy_; j++) {
            // LGM factor value, second index = 0 holds initial values
            Real z = sample.value[model_->pIdx(IR, j)][i + 1];
            curves_[j]->move(t, z);
            for (Size k = 0; k < ten_dsc_[j].size(); k++) {
                Date d = dates_[i] + ten_dsc_[j][k];
                Time T = dc.yearFraction(dates_[i], d);
                Real discount = std::max(curves_[j]->discount(T), 0.00001);
                scenarios[i]->add(discountCurveKeys_[j * ten_dsc_[j].size() + k], discount);
            }
        }

        // Index curves and Index fixings
        for (Size j = 0; j < n_indices_; ++j) {
            Real z = sample.value[model_->pIdx(IR, model_->ccyIndex(indices_[j]->currency()))][i + 1];
            fwdCurves_[j]->move(dates_[i], z);
            for (Size k = 0; k < ten_idx_[j].size(); ++k) {
                Date d = dates_[i] + ten_idx_[j][k];
                Time T = dc.yearFraction(dates_[i], d);
                Real discount = std::max(fwdCurves_[j]->discount(T), 0.00001);
                scenarios[i]->add(indexCurveKeys_[j * ten_idx_[j].size() + k], discount);
            }
        }

        // Yield curves
        for (Size j = 0; j < n_curves_; ++j) {
            Real z = sample.value[model_->pIdx(IR, model_->ccyIndex(yieldCurveCurrency_[j]))][i + 1];
            yieldCurves_[j]->move(dates_[i], z);
            for (Size k = 0; k < ten_yc_[j].size(); ++k) {
                Date d = dates_[i] + ten_yc_[j][k];
                Time T = dc.yearFraction(dates_[i], d);
                Real discount = std::max(yieldCurves_[j]->discount(T), 0.00001);
                scenarios[i]->add(yieldCurveKeys_[j * ten_yc_[j].size() + k], discount);
            }
        }

        // FX rates
        for (Size k = 0; k < n_ccy_ - 1; k++) {
            Real fx = std::exp(sample.value[model_->pIdx(FX, k)][i + 1]); // multiplies USD amount to get EUR
            scenarios[i]->add(fxKeys_[k], fx);
        }

        // FX vols
        if (simMarketConfig_->simulateFXVols()) {
            const vector<Period>& expires = simMarketConfig_->fxVolExpiries();
            for (Size k = 0; k < simMarketConfig_->fxVolCcyPairs().size(); k++) {
                const string ccyPair = simMarketConfig_->fxVolCcyPairs()[k];

                Size fxIndex = fxVols_[k]->fxIndex();
                Real zFor = sample.value[fxIndex + 1][i + 1];
                Real logFx = sample.value[n_ccy_ + fxIndex][i + 1]; // multiplies USD amount to get EUR
                fxVols_[k]->move(dates_[i], z0, zFor, logFx);

                for (Size j = 0; j < expires.size(); j++) {
                    Real vol = fxVols_[k]->blackVol(dates_[i] + expires[j], Null<Real>(), true);
                    scenarios[i]->add(RiskFactorKey(RiskFactorKey::KeyType::FXVolatility, ccyPair, j), vol);
                }
            }
        }

        // Equity spots
        for (Size k = 0; k < n_eq_; k++) {
            Real eqSpot = std::exp(sample.value[model_->pIdx(EQ, k)][i + 1]);
            scenarios[i]->add(eqKeys_[k], eqSpot);
        }

        // Equity vols
        if (simMarketConfig_->simulateEquityVols()) {
            for (Size k = 0; k < simMarketConfig_->equityVolNames().size(); k++) {
                const string equityName = simMarketConfig_->equityVolNames()[k];

                const vector<Period>& expiries = simMarketConfig_->equityVolExpiries(equityName);

                Size eqIndex = eqVols_[k]->equityIndex();
                Size eqCcyIdx = eqVols_[k]->eqCcyIndex();
                Real z_eqIr = sample.value[eqCcyIdx][i + 1];
                Real logEq = sample.value[eqIndex][i + 1];
                eqVols_[k]->move(dates_[i], z_eqIr, logEq);

                for (Size j = 0; j < expiries.size(); j++) {
                    Real vol = eqVols_[k]->blackVol(dates_[i] + expiries[j], Null<Real>(), true);
                    scenarios[i]->add(RiskFactorKey(RiskFactorKey::KeyType::EquityVolatility, equityName, j), vol);
                }
            }
        }

        // Inflation index values
        for (Size j = 0; j < n_inf_; j++) {

            // Depending on type of model, i.e. DK or JY, z and y mean different things.
            Real z = sample.value[model_->pIdx(INF, j, 0)][i + 1];
            Real y = sample.value[model_->pIdx(INF, j, 1)][i + 1];

            // Could possibly cache the model type outside the loop to improve performance.
            Real cpi = 0.0;
            if (model_->modelType(INF, j) == JY) {
                cpi = std::exp(sample.value[model_->pIdx(INF, j, 1)][i + 1]);
            } else if (model_->modelType(INF, j) == DK) {
                auto index = *initMarket_->zeroInflationIndex(model_->inf(j)->name());
                Date baseDate = index->zeroInflationTermStructure()->baseDate();
                auto zts = index->zeroInflationTermStructure();
                Time relativeTime =
                    inflationYearFraction(zts->frequency(), zts->indexIsInterpolated(), zts->dayCounter(), baseDate,
                                          dates_[i] - zts->observationLag());
                std::tie(cpi, std::ignore) = model_->infdkI(j, relativeTime, relativeTime, z, y);
                cpi *= index->fixing(baseDate);
            } else {
                QL_FAIL("CrossAssetModelScenarioGenerator: expected inflation model to be JY or DK.");
            }

            scenarios[i]->add(cpiKeys_[j], cpi);
        }

        // Zero inflation curves
        for (Size j = 0; j < zeroInfCurves_.size(); ++j) {

            auto tup = zeroInfCurves_[j];

            // State variables needed depends on model, 3 for JY and 2 for DK.
            auto idx = std::get<0>(tup);
            Array state(3);
            state[0] = sample.value[model_->pIdx(INF, idx, 0)][i + 1];
            state[1] = sample.value[model_->pIdx(INF, idx, 1)][i + 1];
            if (std::get<2>(tup) == DK) {
                state.resize(2);
            } else {
                state[2] = sample.value[model_->pIdx(IR, std::get<1>(tup))][i + 1];
            }

            // Update the term structure's date and state.
            auto ts = std::get<3>(tup);
            ts->move(dates_[i], state);

            // Populate the zero inflation scenario values based on the current date and state.
            for (Size k = 0; k < ten_zinf_[j].size(); k++) {
                Time T = dc.yearFraction(dates_[i], dates_[i] + ten_zinf_[j][k]);
                scenarios[i]->add(zeroInflationKeys_[j * ten_zinf_[j].size() + k], ts->zeroRate(T));
            }
        }

        // YoY inflation curves
        for (Size j = 0; j < yoyInfCurves_.size(); ++j) {

            auto tup = yoyInfCurves_[j];

            // For YoY model implied term structure, JY and DK both need 3 state variables.
            auto idx = std::get<0>(tup);
            Array state(3);
            state[0] = sample.value[model_->pIdx(INF, idx, 0)][i + 1];
            state[1] = sample.value[model_->pIdx(INF, idx, 1)][i + 1];
            state[2] = sample.value[model_->pIdx(IR, std::get<1>(tup))][i + 1];

            // Update the term structure's date and state.
            auto ts = std::get<3>(tup);
            ts->move(dates_[i], state);

            // Create the YoY pillar dates from the tenors.
            vector<Date> pillarDates(ten_yinf_[j].size());
            for (Size k = 0; k < pillarDates.size(); ++k)
                pillarDates[k] = dates_[i] + ten_yinf_[j][k];

            // Use the YoY term structure's YoY rates to populate the scenarios.
            auto yoyRates = ts->yoyRates(pillarDates);
            for (Size k = 0; k < pillarDates.size(); ++k) {
                scenarios[i]->add(yoyInflationKeys_[j * ten_yinf_[j].size() + k], yoyRates.at(pillarDates[k]));
            }
        }

        // Credit curves
        for (Size j = 0; j < n_cr_; ++j) {
            if (model_->modelType(CR, j) == ModelType::LGM1F) {
                Real z = sample.value[model_->pIdx(CR, j, 0)][i + 1];
                Real y = sample.value[model_->pIdx(CR, j, 1)][i + 1];
                lgmDefaultCurves_[j]->move(dates_[i], z, y);
                for (Size k = 0; k < ten_dfc_[j].size(); k++) {
                    Date d = dates_[i] + ten_dfc_[j][k];
                    Time T = dc.yearFraction(dates_[i], d);
                    Real survProb = std::max(lgmDefaultCurves_[j]->survivalProbability(T), 0.00001);
                    scenarios[i]->add(defaultCurveKeys_[j * ten_dfc_[j].size() + k], survProb);
                }
            } else if (model_->modelType(CR, j) == ModelType::CIRPP) {
                Real y = sample.value[model_->pIdx(CR, j, 0)][i + 1];
                cirppDefaultCurves_[j]->move(dates_[i], y);
                for (Size k = 0; k < ten_dfc_[j].size(); k++) {
                    Date d = dates_[i] + ten_dfc_[j][k];
                    Time T = dc.yearFraction(dates_[i], d);
                    Real survProb = std::max(cirppDefaultCurves_[j]->survivalProbability(T), 0.00001);
                    scenarios[i]->add(defaultCurveKeys_[j * ten_dfc_[j].size() + k], survProb);
                }
            }
        }

        // TODO: Further risk factor classes are added here
    }
    return scenarios;
}
} // namespace analytics
} // namespace ore
