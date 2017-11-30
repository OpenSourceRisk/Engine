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

#include <qle/models/lgmimpliedyieldtermstructure.hpp>
#include <qle/models/dkimpliedzeroinflationtermstructure.hpp>
#include <qle/models/dkimpliedyoyinflationtermstructure.hpp>
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
    Date today, boost::shared_ptr<ore::analytics::DateGrid> grid, boost::shared_ptr<ore::data::Market> initMarket,
    const std::string& configuration)
    : ScenarioPathGenerator(today, grid->dates(), grid->timeGrid()), model_(model), pathGenerator_(pathGenerator),
      scenarioFactory_(scenarioFactory), simMarketConfig_(simMarketConfig), initMarket_(initMarket),
      configuration_(configuration) {

    QL_REQUIRE(initMarket != NULL, "CrossAssetScenarioGenerator: initMarket is null");
    QL_REQUIRE(timeGrid_.size() == dates_.size() + 1, "date/time grid size mismatch");

    // TODO, curve tenors might be overwritten by dates in simMarketConfig_, here we just take the tenors

    // Cache yield curve keys
    Size n_ccy = model_->components(IR);
    discountCurveKeys_.reserve(n_ccy * simMarketConfig_->yieldCurveTenors("").size());
    for (Size j = 0; j < model_->components(IR); j++) {
        std::string ccy = model_->irlgm1f(j)->currency().code();
        ten_dsc_.push_back(simMarketConfig_->yieldCurveTenors(ccy));
        Size n_ten = ten_dsc_.back().size();
        for (Size k = 0; k < n_ten; k++)
            discountCurveKeys_.emplace_back(RiskFactorKey::KeyType::DiscountCurve, ccy, k); // j * n_ten + k
    }

    // Cache index curve keys
    Size n_indices = simMarketConfig_->indices().size();
    indexCurveKeys_.reserve(n_indices * simMarketConfig_->yieldCurveTenors("").size());
    for (Size j = 0; j < n_indices; ++j) {
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
    fxKeys_.reserve(n_ccy - 1);
    for (Size k = 0; k < n_ccy - 1; k++) {
        const string& foreign = model_->irlgm1f(k + 1)->currency().code(); // Is this safe?
        const string& domestic = model_->irlgm1f(0)->currency().code();
        fxKeys_.emplace_back(RiskFactorKey::KeyType::FXSpot, foreign + domestic); // k
    }

    // set up CrossAssetModelImpliedFxVolTermStructures
    if (simMarketConfig_->simulateFXVols()) {
        LOG("CrossAssetModel is simulating FX vols");
        for (Size k = 0; k < simMarketConfig_->fxVolCcyPairs().size(); k++) {
            // Calculating the index is messy
            const string& pair = simMarketConfig_->fxVolCcyPairs()[k];
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
        eqKeys_.emplace_back(RiskFactorKey::KeyType::EquitySpot, eqName);
    }

    // equity vols
    if (simMarketConfig_->equityVolNames().size() > 0 && simMarketConfig_->simulateEquityVols()) {
        LOG("CrossAssetModel is simulating EQ vols");
        for (Size k = 0; k < simMarketConfig_->equityVolNames().size(); k++) {
            // Calculating the index is messy
            const string& equityName = simMarketConfig_->equityVolNames()[k];
            LOG("Set up CrossAssetModelImpliedEqVolTermStructures for " << equityName);
            Size eqIndex = model->eqIndex(equityName);
            // eqVols_ are indexed by ccyPairs
            LOG("EQ Vol Name = " << equityName << ", index = " << eqIndex);
            // index - 1 to convert "IR" index into an "FX" index
            eqVols_.push_back(boost::make_shared<CrossAssetModelImpliedEqVolTermStructure>(model_, eqIndex));
            LOG("Set up CrossAssetModelImpliedEqVolTermStructures for " << equityName << " done");
        }
    }
        
    // Cache INF rate keys0
    Size n_inf = model_->components(INF);
    if (n_inf > 0) {
        cpiKeys_.reserve(n_inf);
        for (Size j = 0; j < n_inf; ++j) {
            cpiKeys_.emplace_back(RiskFactorKey::KeyType::CPIIndex, model->infdk(j)->name());
        }

        Size n_zeroinf = simMarketConfig_->zeroInflationIndices().size();
        if (n_zeroinf > 0) {
            zeroInflationKeys_.reserve(n_zeroinf * simMarketConfig_->zeroInflationTenors("").size());
            for (Size j = 0; j < n_zeroinf; ++j) {
                ten_zinf_.push_back(simMarketConfig_->zeroInflationTenors(simMarketConfig_->zeroInflationIndices()[j]));
                Size n_ten = ten_zinf_.back().size();
                for (Size k = 0; k < n_ten; ++k) {
                    zeroInflationKeys_.emplace_back(RiskFactorKey::KeyType::ZeroInflationCurve, simMarketConfig_->zeroInflationIndices()[j], k);
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
                    yoyInflationKeys_.emplace_back(RiskFactorKey::KeyType::YoYInflationCurve, simMarketConfig_->yoyInflationIndices()[j], k);
                }
            }
        }
    }

    // Equity Forecast curve keys
    equityForecastCurveKeys_.reserve(n_ccy * simMarketConfig_->equityForecastTenors("").size());
    for (Size j = 0; j < model_->components(EQ); j++) {
        ten_efc_.push_back(simMarketConfig_->equityForecastTenors(simMarketConfig_->equityNames()[j]));
        Size n_ten = ten_efc_.back().size();
        for (Size k = 0; k < n_ten; k++)
            equityForecastCurveKeys_.emplace_back(RiskFactorKey::KeyType::EquityForecastCurve, simMarketConfig_->equityNames()[j], k); // j * n_ten + k
    }
}

std::vector<boost::shared_ptr<Scenario>> CrossAssetModelScenarioGenerator::nextPath() {
    std::vector<boost::shared_ptr<Scenario>> scenarios(dates_.size());
    Sample<MultiPath> sample = pathGenerator_->next();
    Size n_ccy = model_->components(IR);
    Size n_eq = model_->components(EQ);
    Size n_inf = model_->components(INF);
    Size n_indices = simMarketConfig_->indices().size();
    Size n_curves = simMarketConfig_->yieldCurveNames().size();
    Size n_zeroinf = simMarketConfig_->zeroInflationIndices().size();
    Size n_yoyinf = simMarketConfig_->yoyInflationIndices().size();
    vector<boost::shared_ptr<QuantExt::LgmImpliedYieldTermStructure>> curves, fwdCurves, yieldCurves;
    vector<boost::shared_ptr<QuantExt::DkImpliedZeroInflationTermStructure>> zeroInfCurves;
    vector<boost::shared_ptr<QuantExt::DkImpliedYoYInflationTermStructure>> yoyInfCurves;
    vector<boost::shared_ptr<IborIndex>> indices;
    vector<Currency> yieldCurveCurrency;
    vector<boost::shared_ptr<QuantExt::LgmImpliedYieldTermStructure>> equityForecastCurves;
    vector<Currency> equityForecastCurrency;
    vector<string> zeroInflationIndex, yoyInflationIndex;

    DayCounter dc = model_->irlgm1f(0)->termStructure()->dayCounter();

    for (Size j = 0; j < n_ccy; ++j) {
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

    for (Size j = 0; j < n_curves; ++j) {
        std::string curveName = simMarketConfig_->yieldCurveNames()[j];
        Currency ccy = ore::data::parseCurrency(simMarketConfig_->yieldCurveCurrencies()[j]);
        Handle<YieldTermStructure> yts = initMarket_->yieldCurve(curveName, configuration_);
        auto impliedYieldCurve =
            boost::make_shared<LgmImpliedYtsFwdFwdCorrected>(model_->lgm(model_->ccyIndex(ccy)), yts, dc, false);
        yieldCurves.push_back(impliedYieldCurve);
        yieldCurveCurrency.push_back(ccy);
    }

    for (Size j = 0; j < n_zeroinf; ++j) {
        zeroInfCurves.push_back(boost::make_shared<QuantExt::DkImpliedZeroInflationTermStructure>(model_, j));
    }

    for (Size j = 0; j < n_yoyinf; ++j) {
        yoyInfCurves.push_back(boost::make_shared<QuantExt::DkImpliedYoYInflationTermStructure>(model_, j));
    }
  
    for (Size j = 0; j < n_eq; ++j) {
        std::string curveName = simMarketConfig_->equityNames()[j];
        Currency ccy = model_->eqbs(j)->currency();
        Handle<YieldTermStructure> yts = initMarket_->equityForecastCurve(curveName, configuration_);
        auto ForecastCurve =
            boost::make_shared<LgmImpliedYtsFwdFwdCorrected>(model_->lgm(model_->ccyIndex(ccy)), yts, dc, false);
        equityForecastCurves.push_back(ForecastCurve);
        equityForecastCurrency.push_back(ccy);
    }

    for (Size i = 0; i < dates_.size(); i++) {
        Real t = timeGrid_[i + 1]; // recall: time grid has inserted t=0

        scenarios[i] = scenarioFactory_->buildScenario(dates_[i]);

        // Set numeraire, numeraire currency and the (deterministic) domestic discount
        // Asset index 0 in sample.value[0][i+1] refers to the domestic currency process.
        Real z0 = sample.value[0][i + 1]; // domestic LGM factor, second index = 0 holds initial values
        scenarios[i]->setNumeraire(model_->numeraire(0, t, z0));

        // Discount curves
        for (Size j = 0; j < n_ccy; j++) {
            // LGM factor value, second index = 0 holds initial values
            Real z = sample.value[model_->pIdx(IR, j)][i + 1];
            curves[j]->move(t, z);
            for (Size k = 0; k < ten_dsc_[j].size(); k++) {
                Date d = dates_[i] + ten_dsc_[j][k];
                Time T = dc.yearFraction(dates_[i], d);
                Real discount = std::max(curves[j]->discount(T), 0.00001);
                scenarios[i]->add(discountCurveKeys_[j * ten_dsc_[j].size() + k], discount);
            }
        }

        // Index curves and Index fixings
        for (Size j = 0; j < n_indices; ++j) {
            Real z = sample.value[model_->pIdx(IR, model_->ccyIndex(indices[j]->currency()))][i + 1];
            fwdCurves[j]->move(dates_[i], z);
            for (Size k = 0; k < ten_idx_[j].size(); ++k) {
                Date d = dates_[i] + ten_idx_[j][k];
                Time T = dc.yearFraction(dates_[i], d);
                Real discount = std::max(fwdCurves[j]->discount(T), 0.00001);
                scenarios[i]->add(indexCurveKeys_[j * ten_idx_[j].size() + k], discount);
            }
        }

        // Yield curves
        for (Size j = 0; j < n_curves; ++j) {
            Real z = sample.value[model_->pIdx(IR, model_->ccyIndex(yieldCurveCurrency[j]))][i + 1];
            yieldCurves[j]->move(dates_[i], z);
            for (Size k = 0; k < ten_yc_[j].size(); ++k) {
                Date d = dates_[i] + ten_yc_[j][k];
                Time T = dc.yearFraction(dates_[i], d);
                Real discount = std::max(yieldCurves[j]->discount(T), 0.00001);
                scenarios[i]->add(yieldCurveKeys_[j * ten_yc_[j].size() + k], discount);
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
            for (Size k = 0; k < simMarketConfig_->fxVolCcyPairs().size(); k++) {
                const string& ccyPair = simMarketConfig_->fxVolCcyPairs()[k];

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
        if (simMarketConfig_->simulateEquityVols()) {
            const vector<Period>& expiries = simMarketConfig_->equityVolExpiries();
            for (Size k = 0; k < simMarketConfig_->equityVolNames().size(); k++) {
                const string& equityName = simMarketConfig_->equityVolNames()[k];

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

        // Inflation curves
        for (Size j = 0; j < n_inf; j++) {
            // LGM factor value, second index = 0 holds initial values
            Real z = sample.value[model_->pIdx(INF, j, 0)][i + 1];
            Real y = sample.value[model_->pIdx(INF, j, 1)][i + 1];

            // update fixing manage with fixing for base date
            boost::shared_ptr<ZeroInflationIndex> index = *initMarket_->zeroInflationIndex(model_->infdk(j)->name());
            Time relativeTime = dc.yearFraction(model_->infdk(j)->termStructure()->referenceDate(), dates_[i]);
            std::pair<Real, Real> ii = model_->infdkI(j, relativeTime, relativeTime + t, z, y);

            Real baseCPI = index->fixing(index->zeroInflationTermStructure()->baseDate());
            scenarios[i]->add(cpiKeys_[j], baseCPI * ii.first);
        }


        for (Size j = 0; j < n_zeroinf; ++j) {
            std::string indexName = simMarketConfig_->zeroInflationIndices()[j];
            Real z = sample.value[model_->pIdx(INF, model_->infIndex(indexName), 0)][i + 1];
            Real y = sample.value[model_->pIdx(INF, model_->infIndex(indexName), 1)][i + 1];
            zeroInfCurves[j]->move(dates_[i], z, y);
            for (Size k = 0; k < ten_zinf_[j].size(); k++) {
                Date d = dates_[i] + ten_zinf_[j][k];
                Time T = dc.yearFraction(dates_[i], d);
                Real zero = zeroInfCurves[j]->zeroRate(T);
                scenarios[i]->add(zeroInflationKeys_[j * ten_zinf_[j].size() + k], zero);
            }
        }

        for (Size j = 0; j < n_yoyinf; ++j) {
            std::string indexName = simMarketConfig_->yoyInflationIndices()[j];
            Size ccy = model_->ccyIndex(model_->infdk(j)->currency());
            Real z = sample.value[model_->pIdx(INF, model_->infIndex(indexName), 0)][i + 1];
            Real y = sample.value[model_->pIdx(INF, model_->infIndex(indexName), 1)][i + 1];
            Real ir_z = sample.value[model_->pIdx(IR, ccy)][i + 1];
            yoyInfCurves[j]->move(dates_[i], z, y, ir_z);
            for (Size k = 0; k < ten_yinf_[j].size(); k++) {
                Date d = dates_[i] + ten_yinf_[j][k];
                Time T = dc.yearFraction(dates_[i], d);
                Real yoy = yoyInfCurves[j]->yoyRate(T);
                scenarios[i]->add(yoyInflationKeys_[j * ten_yinf_[j].size() + k], yoy);
            }
        }

        // EquityForecastCurve
        for (Size j = 0; j < n_eq; ++j) {
            Real z = sample.value[model_->pIdx(IR, model_->ccyIndex(equityForecastCurrency[j]))][i + 1];
            equityForecastCurves[j]->move(dates_[i], z);
            for (Size k = 0; k < ten_efc_[j].size(); ++k) {
                Date d = dates_[i] + ten_efc_[j][k];
                Time T = dc.yearFraction(dates_[i], d);
                Real discount = std::max(equityForecastCurves[j]->discount(T), 0.00001);
                scenarios[i]->add(equityForecastCurveKeys_[j * ten_efc_[j].size() + k], discount);
            }
        }

        // TODO: Further risk factor classes are added here
    }
    return scenarios;
}
} // namespace analytics
} // namespace ore
