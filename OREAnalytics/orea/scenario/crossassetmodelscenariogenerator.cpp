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

#include <qle/models/lgmimpliedyieldtermstructure.hpp>

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
    Date today, boost::shared_ptr<ore::analytics::DateGrid> grid, boost::shared_ptr<ore::data::Market> initMarket,
    const std::string& configuration)
    : ScenarioPathGenerator(today, grid->dates(), grid->timeGrid()), model_(model), pathGenerator_(pathGenerator),
      scenarioFactory_(scenarioFactory), simMarketConfig_(simMarketConfig), initMarket_(initMarket),
      configuration_(configuration) {

    QL_REQUIRE(initMarket != NULL, "CrossAssetScenarioGenerator: initMarket is null");
    QL_REQUIRE(timeGrid_.size() == dates_.size() + 1, "date/time grid size mismatch");

    // Cache yield curve keys
    Size n_ccy = model_->components(IR);
    Size n_ten = simMarketConfig_->yieldCurveTenors().size();
    discountCurveKeys_.reserve(n_ccy * n_ten);
    for (Size j = 0; j < model_->components(IR); j++) {
        std::string ccy = model_->irlgm1f(j)->currency().code();
        for (Size k = 0; k < n_ten; k++)
            discountCurveKeys_.emplace_back(RiskFactorKey::KeyType::DiscountCurve, ccy, k); // j * n_ten + k
    }

    // Cache index curve keys
    Size n_indices = simMarketConfig_->indices().size();
    indexCurveKeys_.reserve(n_indices * n_ten);
    for (Size j = 0; j < n_indices; ++j) {
        for (Size k = 0; k < n_ten; ++k) {
            indexCurveKeys_.emplace_back(RiskFactorKey::KeyType::IndexCurve, simMarketConfig_->indices()[j], k);
        }
    }

    // Cache FX rate keys
    fxKeys_.reserve(n_ccy - 1);
    for (Size k = 0; k < n_ccy - 1; k++) {
        const string& foreign = model_->irlgm1f(k + 1)->currency().code(); // Is this safe?
        const string& domestic = model_->irlgm1f(0)->currency().code();
        fxKeys_.emplace_back(RiskFactorKey::KeyType::FXSpot, foreign + domestic); // k
    }

    // set up CrossAssetModelImpliedFxVolTermStructures
    if (simMarketConfig_->simulateFXVols()) {
        LOG("CrossAssetModel is simulating FX vols");
        for (Size k = 0; k < simMarketConfig_->ccyPairs().size(); k++) {
            // Calculating the index is messy
            const string& pair = simMarketConfig_->ccyPairs()[k];
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
        const string& eqName = model_->eqbs(k)->eqName();
        eqKeys_.emplace_back(RiskFactorKey::KeyType::EQSpot, eqName);
    }

    // equity vols
    if (simMarketConfig_->eqVolNames().size() > 0 && simMarketConfig_->simulateEQVols()) {
        LOG("CrossAssetModel is simulating EQ vols");
        for (Size k = 0; k < simMarketConfig_->eqVolNames().size(); k++) {
            // Calculating the index is messy
            const string& eqName = simMarketConfig_->eqVolNames()[k];
            LOG("Set up CrossAssetModelImpliedEqVolTermStructures for " << eqName);
            Size eqIndex = model->eqIndex(eqName);
            // eqVols_ are indexed by ccyPairs
            LOG("EQ Vol Name = " << eqName << ", index = " << eqIndex);
            // index - 1 to convert "IR" index into an "FX" index
            eqVols_.push_back(boost::make_shared<CrossAssetModelImpliedEqVolTermStructure>(model_, eqIndex));
            LOG("Set up CrossAssetModelImpliedEqVolTermStructures for " << eqName << " done");
        }
    }
}

std::vector<boost::shared_ptr<Scenario>> CrossAssetModelScenarioGenerator::nextPath() {
    std::vector<boost::shared_ptr<Scenario>> scenarios(dates_.size());
    Sample<MultiPath> sample = pathGenerator_->next();
    Size n_ccy = model_->components(IR);
    Size n_eq = model_->components(EQ);
    Size n_indices = simMarketConfig_->indices().size();
    Size n_ten = simMarketConfig_->yieldCurveTenors().size();
    vector<string> ccyPairs(n_ccy - 1);
    vector<boost::shared_ptr<QuantExt::LgmImpliedYieldTermStructure>> curves, fwdCurves;
    vector<boost::shared_ptr<IborIndex>> indices;

    DayCounter dc = model_->irlgm1f(0)->termStructure()->dayCounter();

    for (Size j = 0; j < model_->components(IR); ++j) {
        curves.push_back(boost::make_shared<QuantExt::LgmImpliedYieldTermStructure>(model_->lgm(j), dc, true));
    }

    for (Size j = 0; j < n_indices; ++j) {
        std::string indexName = simMarketConfig_->indices()[j];
        boost::shared_ptr<IborIndex> index = *initMarket_->iborIndex(indexName, configuration_);
        Handle<YieldTermStructure> fts = index->forwardingTermStructure();
        auto impliedFwdCurve = boost::make_shared<LgmImpliedYtsFwdFwdCorrected>(
            model_->lgm(model_->ccyIndex(index->currency())), fts, dc, false);
        fwdCurves.push_back(impliedFwdCurve);
        indices.push_back(index->clone(Handle<YieldTermStructure>(impliedFwdCurve)));
    }

    for (Size i = 0; i < dates_.size(); i++) {
        Real t = timeGrid_[i + 1]; // recall: time grid has inserted t=0

        scenarios[i] = scenarioFactory_->buildScenario(dates_[i]);

        // Set numeraire, numeraire currency and the (deterministic) domestic discount
        // Asset index 0 in sample.value[0][i+1] refers to the domestic currency process.
        Real z0 = sample.value[0][i + 1]; // domestic LGM factor, second index = 0 holds initial values
        scenarios[i]->setNumeraire(model_->numeraire(0, t, z0));

        // Yield curves
        for (Size j = 0; j < n_ccy; j++) {
            // LGM factor value, second index = 0 holds initial values
            Real z = sample.value[model_->pIdx(IR, j)][i + 1];
            curves[j]->move(t, z);
            for (Size k = 0; k < n_ten; k++) {
                Date d = dates_[i] + simMarketConfig_->yieldCurveTenors()[k];
                Time T = dc.yearFraction(dates_[i], d);
                Real discount = curves[j]->discount(T);
                scenarios[i]->add(discountCurveKeys_[j * n_ten + k], discount);
            }
        }

        // Index curves and Index fixings
        for (Size j = 0; j < n_indices; ++j) {
            Real z = sample.value[model_->pIdx(IR, model_->ccyIndex(indices[j]->currency()))][i + 1];
            fwdCurves[j]->move(dates_[i], z);
            for (Size k = 0; k < n_ten; ++k) {
                Date d = dates_[i] + simMarketConfig_->yieldCurveTenors()[k];
                Time T = dc.yearFraction(dates_[i], d);
                Real discount = fwdCurves[j]->discount(T);
                scenarios[i]->add(indexCurveKeys_[j * n_ten + k], discount);
            }
        }

        // FX rates
        for (Size k = 0; k < n_ccy - 1; k++) {
            // if (i == 0) { // construct the labels at the first scenario date only
            //     const string& foreign = model_->irlgm1f(k+1)->currency().code(); // Is this safe?
            //     const string& domestic = model_->irlgm1f(0)->currency().code();
            //     ccyPairs[k] = foreign + domestic; // x USDEUR means 1 USD is worth x EUR
            // }
            Real fx = std::exp(sample.value[model_->pIdx(FX, k)][i + 1]); // multiplies USD amount to get EUR
            scenarios[i]->add(fxKeys_[k], fx);
        }

        // FX vols
        if (simMarketConfig_->simulateFXVols()) {
            const vector<Period>& expires = simMarketConfig_->fxVolExpiries();
            for (Size k = 0; k < simMarketConfig_->ccyPairs().size(); k++) {
                const string& ccyPair = simMarketConfig_->ccyPairs()[k];

                Size fxIndex = fxVols_[k]->fxIndex();
                Real zFor = sample.value[fxIndex + 1][i + 1];
                Real logFx = sample.value[n_ccy + fxIndex][i + 1]; // multiplies USD amount to get EUR
                fxVols_[k]->move(dates_[i], z0, zFor, logFx);

                for (Size j = 0; j < expires.size(); j++) {
                    Real vol = fxVols_[k]->blackVol(dates_[i] + expires[j], Null<Real>(), true);
                    scenarios[i]->add(RiskFactorKey(RiskFactorKey::KeyType::FXVolatility, ccyPair, j), vol);
                }
            }
        }

        // Equity spots
        for (Size k = 0; k < n_eq; k++) {
            Real eqSpot = std::exp(sample.value[model_->pIdx(EQ, k)][i + 1]);
            scenarios[i]->add(eqKeys_[k], eqSpot);
        }

        // Equity vols
        if (simMarketConfig_->simulateEQVols()) {
            const vector<Period>& expiries = simMarketConfig_->eqVolExpiries();
            for (Size k = 0; k < simMarketConfig_->eqVolNames().size(); k++) {
                const string& eqName = simMarketConfig_->eqVolNames()[k];

                Size eqIndex = eqVols_[k]->equityIndex();
                Size eqCcyIdx = eqVols_[k]->eqCcyIndex();
                Real z_eqIr = sample.value[eqCcyIdx][i + 1];
                Real logEq = sample.value[eqIndex][i + 1];
                eqVols_[k]->move(dates_[i], z_eqIr, logEq);

                for (Size j = 0; j < expiries.size(); j++) {
                    Real vol = eqVols_[k]->blackVol(dates_[i] + expiries[j], Null<Real>(), true);
                    scenarios[i]->add(RiskFactorKey(RiskFactorKey::KeyType::EQVolatility, eqName, j), vol);
                }
            }
        }

        // TODO: Further risk factor classes are added here
    }
    return scenarios;
}
}
}
