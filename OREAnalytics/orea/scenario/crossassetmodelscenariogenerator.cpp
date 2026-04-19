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
#include <orea/scenario/simplescenariofactory.hpp>

#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/osutils.hpp>

#include <qle/indexes/inflationindexobserver.hpp>
#include <qle/utilities/inflation.hpp>

#include <ql/indexes/ibor/euribor.hpp>
#include <ql/termstructures/yield/discountcurve.hpp>
#include <ql/time/daycounters/thirty360.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;

namespace ore {
namespace analytics {

CrossAssetModelScenarioGenerator::CrossAssetModelScenarioGenerator(
    QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel> model,
    QuantLib::ext::shared_ptr<QuantExt::MultiPathGeneratorBase> pathGenerator,
    QuantLib::ext::shared_ptr<ScenarioSimMarketParameters> simMarketConfig, Date today,
    QuantLib::ext::shared_ptr<DateGrid> grid, QuantLib::ext::shared_ptr<ore::data::Market> initMarket,
    const std::string& configuration, const std::string& amcPathDataOutput, Size samples)
    : ScenarioPathGenerator(today, grid->dates(), grid->timeGrid()), model_(model), pathGenerator_(pathGenerator),
      simMarketConfig_(simMarketConfig), initMarket_(initMarket), configuration_(configuration),
      amcPathDataOutput_(amcPathDataOutput), totalSamples_(samples), dateGrid_(grid) {}

void CrossAssetModelScenarioGenerator::init() {
    LOG("CrossAssetModelScenarioGenerator ctor called");

    QL_REQUIRE(initMarket_ != NULL, "CrossAssetScenarioGenerator: initMarket is null");
    QL_REQUIRE(timeGrid_.size() == dates_.size() + 1, "date/time grid size mismatch");

    // build mapping from grid index to path index

    gridIndexInPath_.clear();
    for (Size i = 0; i < timeGrid_.size(); ++i) {
        gridIndexInPath_.push_back(pathGenerator_->timeGrid().closestIndex(timeGrid_[i]));
        QL_REQUIRE(QuantLib::close_enough(pathGenerator_->timeGrid()[gridIndexInPath_.back()], timeGrid_[i]),
                   "CrossAssetModelScenarioGenerator: time in timeGrid ("
                       << timeGrid_[i] << ") is not found in path generator time grid");
    }

    // TODO, curve tenors might be overwritten by dates in simMarketConfig_, here we just take the tenors

    DayCounter dc = model_->irModel(0)->termStructure()->dayCounter();
    n_ccy_ = model_->components(CrossAssetModel::AssetType::IR);
    n_fx_ = model_->components(CrossAssetModel::AssetType::FX);
    n_eq_ = model_->components(CrossAssetModel::AssetType::EQ);
    n_inf_ = model_->components(CrossAssetModel::AssetType::INF);
    n_cr_ = model_->components(CrossAssetModel::AssetType::CR);
    n_com_ = model_->components(CrossAssetModel::AssetType::COM);
    n_crstates_ = model_->components(CrossAssetModel::AssetType::CrState);
    n_survivalweights_ = simMarketConfig_->additionalScenarioDataSurvivalWeights().size();
    n_indices_ = simMarketConfig_->indices().size();
    n_curves_ = simMarketConfig_->yieldCurveNames().size();
    n_states_ = model_->stateProcess()->size();

    auto sharedData = ext::make_shared<SimpleScenario::SharedData>();

    // Cache discount curve keys
    for (Size j = 0; j < n_ccy_; j++) {
        std::string ccy = model_->parametrizations()[j]->currency().code();
        auto const& ten = simMarketConfig_->yieldCurveTenors(ccy);
        time_dsc_.push_back(std::vector<std::vector<double>>(dates_.size(), std::vector<double>(ten.size())));
        for (Size k = 0; k < ten.size(); k++) {
            for (Size i = 0; i < dates_.size(); ++i) {
                time_dsc_.back()[i][k] = dc.yearFraction(dates_[i], dates_[i] + ten[k]);
            }
            sharedData->keys.emplace_back(RiskFactorKey::KeyType::DiscountCurve, ccy, k);
            sharedData->keyIndex[sharedData->keys.back()] = sharedData->keys.size() - 1;
        }
    }

    // Cache index curve keys
    for (Size j = 0; j < n_indices_; ++j) {
        auto const& ten = simMarketConfig_->yieldCurveTenors(simMarketConfig_->indices()[j]);
        time_idx_.push_back(std::vector<std::vector<double>>(dates_.size(), std::vector<double>(ten.size())));
        for (Size k = 0; k < ten.size(); ++k) {
            for (Size i = 0; i < dates_.size(); ++i) {
                time_idx_.back()[i][k] = dc.yearFraction(dates_[i], dates_[i] + ten[k]);
            }
            sharedData->keys.emplace_back(RiskFactorKey::KeyType::IndexCurve, simMarketConfig_->indices()[j], k);
            sharedData->keyIndex[sharedData->keys.back()] = sharedData->keys.size() - 1;
        }
    }

    // Cache yield curve keys
    Size n_curves = simMarketConfig_->yieldCurveNames().size();
    for (Size j = 0; j < n_curves; ++j) {
        auto const& ten = simMarketConfig_->yieldCurveTenors(simMarketConfig_->yieldCurveNames()[j]);
        time_yc_.push_back(std::vector<std::vector<double>>(dates_.size(), std::vector<double>(ten.size())));
        for (Size k = 0; k < ten.size(); ++k) {
            for (Size i = 0; i < dates_.size(); ++i) {
                time_yc_.back()[i][k] = dc.yearFraction(dates_[i], dates_[i] + ten[k]);
            }
            sharedData->keys.emplace_back(RiskFactorKey::KeyType::YieldCurve, simMarketConfig_->yieldCurveNames()[j],
                                          k);
            sharedData->keyIndex[sharedData->keys.back()] = sharedData->keys.size() - 1;
        }
    }

    // Cache FX rate keys
    for (Size k = 0; k < n_ccy_ - 1; k++) {
        const string& foreign = model_->parametrizations()[k + 1]->currency().code();
        const string& domestic = model_->parametrizations()[0]->currency().code();
        sharedData->keys.emplace_back(RiskFactorKey::KeyType::FXSpot, foreign + domestic);
        sharedData->keyIndex[sharedData->keys.back()] = sharedData->keys.size() - 1;
    }

    // set up CrossAssetModelImpliedFxVolTermStructures
    if (simMarketConfig_->simulateFXVols()) {
        DLOG("CrossAssetModel is simulating FX vols");
        QL_REQUIRE(model_->modelType(CrossAssetModel::AssetType::IR, 0) == CrossAssetModel::ModelType::LGM1F,
                   "Simulation of FX vols is only supported for LGM1F ir model type.");
        string baseCcy = model_->parametrizations()[0]->currency().code();
        for (Size k = 0; k < simMarketConfig_->fxVolCcyPairs().size(); k++) {
            // Calculating the index is messy
            const string pair = simMarketConfig_->fxVolCcyPairs()[k];
            DLOG("Set up CrossAssetModelImpliedFxVolTermStructures for " << pair);
            QL_REQUIRE(pair.size() == 6, "Invalid ccypair " << pair);
            // const string& domestic = pair.substr(0, 3);
            // const string& foreign = pair.substr(3);
            // QL_REQUIRE(domestic == model_->parametrizations()[0]->currency().code(), "Only DOM-FOR fx vols
            // supported");
            const string& ccy1 = pair.substr(0, 3);
            const string& ccy2 = pair.substr(3);
            QL_REQUIRE(ccy1 != ccy2, "invalid currency pair " << pair);
            QL_REQUIRE(ccy1 == baseCcy || ccy2 == baseCcy, "currency pair " << pair << " does not contain base");
            string foreign = ccy1 != baseCcy ? ccy1 : ccy2;

            Size index = model_->ccyIndex(parseCurrency(foreign)); // will throw if foreign not there
            QL_REQUIRE(index > 0, "Invalid index for ccy " << foreign << " should be > 0");
            // fxVols_ are indexed by ccyPairs
            DLOG("Pair " << pair << " index " << index);
            // index - 1 to convert "IR" index into an "FX" index
            fxVols_.push_back(QuantLib::ext::make_shared<CrossAssetModelImpliedFxVolTermStructure>(model_, index - 1));
            DLOG("Set up CrossAssetModelImpliedFxVolTermStructures for " << pair << " done");

            const vector<Period>& expires = simMarketConfig_->fxVolExpiries(pair);
            for (Size j = 0; j < expires.size(); j++) {
                sharedData->keys.emplace_back(RiskFactorKey::KeyType::FXVolatility, pair, j);
                sharedData->keyIndex[sharedData->keys.back()] = sharedData->keys.size() - 1;
            }

        }
    }

    // Cache EQ rate keys
    Size n_eq = model_->components(CrossAssetModel::AssetType::EQ);
    for (Size k = 0; k < n_eq; k++) {
        const string& eqName = model_->eqbs(k)->name();
        sharedData->keys.emplace_back(RiskFactorKey::KeyType::EquitySpot, eqName);
        sharedData->keyIndex[sharedData->keys.back()] = sharedData->keys.size() - 1;
    }

    // equity vols
    if (simMarketConfig_->equityVolNames().size() > 0 && simMarketConfig_->simulateEquityVols()) {
        DLOG("CrossAssetModel is simulating EQ vols");
        QL_REQUIRE(model_->modelType(CrossAssetModel::AssetType::IR, 0) == CrossAssetModel::ModelType::LGM1F,
                   "Simulation of EQ vols is only supported for LGM1F ir model type.");
        for (Size k = 0; k < simMarketConfig_->equityVolNames().size(); k++) {
            // Calculating the index is messy
            const string equityName = simMarketConfig_->equityVolNames()[k];
            DLOG("Set up CrossAssetModelImpliedEqVolTermStructures for " << equityName);
            Size eqIndex = model_->eqIndex(equityName);
            // eqVols_ are indexed by ccyPairs
            DLOG("EQ Vol Name = " << equityName << ", index = " << eqIndex);
            // index - 1 to convert "IR" index into an "FX" index
            eqVols_.push_back(QuantLib::ext::make_shared<CrossAssetModelImpliedEqVolTermStructure>(model_, eqIndex));
            DLOG("Set up CrossAssetModelImpliedEqVolTermStructures for " << equityName << " done");

            const vector<Period>& expiries = simMarketConfig_->equityVolExpiries(equityName);
            for (Size j = 0; j < expiries.size(); j++) {
                sharedData->keys.emplace_back(RiskFactorKey::KeyType::EquityVolatility, equityName, j);
                sharedData->keyIndex[sharedData->keys.back()] = sharedData->keys.size() - 1;
            }
        }
    }

    // set up CrossAssetModelImpliedSwaptionVolTermStructures
    if (simMarketConfig_->simulateSwapVols()) {

        DLOG("CrossAssetModel is simulating Swaption vols");

        QL_REQUIRE(model_->modelType(CrossAssetModel::AssetType::IR, 0) == CrossAssetModel::ModelType::LGM1F,
                   "Simulation of Swaption vols is only supported for LGM1F ir model type.");

        for (Size k = 0; k < simMarketConfig_->swapVolKeys().size(); k++) {
            const string key = simMarketConfig_->swapVolKeys()[k];
            DLOG("Set up CrossAssetModelImpliedSwaptionVolTermStructures for key " << key);

            string swapIndexBaseName = initMarket_->swapIndexBase(key);
            string shortSwapIndexBaseName = initMarket_->shortSwapIndexBase(key);
            DLOG("SwapIndexBases for key " << key << " : " << shortSwapIndexBaseName << " " << swapIndexBaseName);
            auto swapIndex = parseSwapIndex(swapIndexBaseName);
            auto shortSwapIndex = parseSwapIndex(shortSwapIndexBaseName);

            Size ccyIndex = model_->ccyIndex(swapIndex->currency());
            swaptionVols_.push_back(QuantLib::ext::make_shared<CrossAssetModelImpliedSwaptionVolTermStructure>(
                model_, curves_[ccyIndex], indices_, swapIndex, shortSwapIndex));

            DLOG("Set up CrossAssetModelImpliedSwaptionVolTermStructures for key " << key << " done");

            const vector<Period>& expires = simMarketConfig_->swapVolExpiries(key);
            const vector<Period>& terms = simMarketConfig_->swapVolTerms(key);

            for (Size j = 0; j < expires.size(); j++) {
                for (Size jj = 0; jj < terms.size(); jj++) {
                    Size idx = j * terms.size() + jj;
                    sharedData->keys.emplace_back(RiskFactorKey::KeyType::SwaptionVolatility, key, idx);
                    sharedData->keyIndex[sharedData->keys.back()] = sharedData->keys.size() - 1;
                }
            }
        }
    }

    // Cache INF rate keys
    Size n_inf = model_->components(CrossAssetModel::AssetType::INF);
    if (n_inf > 0) {
        for (Size j = 0; j < n_inf; ++j) {
            sharedData->keys.emplace_back(RiskFactorKey::KeyType::CPIIndex, model_->inf(j)->name());
            sharedData->keyIndex[sharedData->keys.back()] = sharedData->keys.size() - 1;
        }

        Size n_zeroinf = simMarketConfig_->zeroInflationIndices().size();
        if (n_zeroinf > 0) {
            for (Size j = 0; j < n_zeroinf; ++j) {
                ten_zinf_.push_back(simMarketConfig_->zeroInflationTenors(simMarketConfig_->zeroInflationIndices()[j]));
                Size n_ten = ten_zinf_.back().size();
                for (Size k = 0; k < n_ten; ++k) {
                    sharedData->keys.emplace_back(RiskFactorKey::KeyType::ZeroInflationCurve,
                                                  simMarketConfig_->zeroInflationIndices()[j], k);
                    sharedData->keyIndex[sharedData->keys.back()] = sharedData->keys.size() - 1;
                }
            }
        }

        Size n_yoyinf = simMarketConfig_->yoyInflationIndices().size();
        if (n_yoyinf > 0) {
            for (Size j = 0; j < n_yoyinf; ++j) {
                ten_yinf_.push_back(simMarketConfig_->yoyInflationTenors(simMarketConfig_->yoyInflationIndices()[j]));
                Size n_ten = ten_yinf_.back().size();
                for (Size k = 0; k < n_ten; ++k) {
                    sharedData->keys.emplace_back(RiskFactorKey::KeyType::YoYInflationCurve,
                                                  simMarketConfig_->yoyInflationIndices()[j], k);
                    sharedData->keyIndex[sharedData->keys.back()] = sharedData->keys.size() - 1;
                }
            }
        }
    }

    // Cache default curve keys
    for (Size j = 0; j < model_->components(CrossAssetModel::AssetType::CR); j++) {
        std::string cr_name = model_->cr(j)->name();
        ten_dfc_.push_back(simMarketConfig_->defaultTenors(cr_name));
        Size n_ten = ten_dfc_.back().size();
        for (Size k = 0; k < n_ten; k++) {
            sharedData->keys.emplace_back(RiskFactorKey::KeyType::SurvivalProbability, cr_name, k);
            sharedData->keyIndex[sharedData->keys.back()] = sharedData->keys.size() - 1;
        }
    }

    // Cache commodity curve keys
    if (n_com_ > 0) {
        for (Size j = 0; j < n_com_; j++) {
            std::string name = model_->com(j)->name();
            ten_com_.push_back(simMarketConfig_->commodityCurveTenors(name));
            Size n_ten = ten_com_.back().size();
            for (Size k = 0; k < n_ten; k++) {
            sharedData->keys.emplace_back(RiskFactorKey::KeyType::CommodityCurve, name, k);
            sharedData->keyIndex[sharedData->keys.back()] = sharedData->keys.size() - 1;
            }
        }
    }

    // Cache CrState keys
    for (Size j = 0; j < n_crstates_; ++j) {
        ostringstream numStr;
        numStr << j;
        sharedData->keys.emplace_back(RiskFactorKey::KeyType::CreditState, numStr.str());
        sharedData->keyIndex[sharedData->keys.back()] = sharedData->keys.size() - 1;
    }

    for (Size j = 0; j < n_survivalweights_; ++j) {
        string name = simMarketConfig_->additionalScenarioDataSurvivalWeights()[j];
        sharedData->keys.emplace_back(RiskFactorKey::KeyType::SurvivalWeight, name);
        sharedData->keyIndex[sharedData->keys.back()] = sharedData->keys.size() - 1;
        sharedData->keys.emplace_back(RiskFactorKey::KeyType::RecoveryRate, name);
        sharedData->keyIndex[sharedData->keys.back()] = sharedData->keys.size() - 1;
        survivalWeightsDefaultCurves_.push_back(*initMarket_->defaultCurve(name, configuration_));
    }

    sharedData->refreshKeysHash();

    // build scenario factory using the shared data built above

    scenarioFactory_ = ext::make_shared<SimpleScenarioFactory>(sharedData);

    // cache curves

    // we need a copy of the ir models to enable the cache for the purpose of this path generator
    std::vector<ext::shared_ptr<IrModel>> irModel(n_ccy_);
    for (Size j = 0; j < n_ccy_; ++j) {
        irModel[j] = model_->irModel(j)->clone();
    }

    std::vector<Size> curvesCacheLoopSize(n_ccy_, 0);

    for (Size j = 0; j < n_ccy_; ++j) {
        curves_.push_back(
            QuantLib::ext::make_shared<QuantExt::ModelImpliedYieldTermStructure>(irModel[j], dc, true));
        curvesCacheLoopSize[j] += dates_.size() * time_dsc_[j][0].size();
    }

    indexCcyIdx_.resize(n_indices_);
    for (Size j = 0; j < n_indices_; ++j) {
        std::string indexName = simMarketConfig_->indices()[j];
        QuantLib::ext::shared_ptr<IborIndex> index = *initMarket_->iborIndex(indexName, configuration_);
        indexCcyIdx_[j] = model_->ccyIndex(index->currency());
        Handle<YieldTermStructure> fts = index->forwardingTermStructure();
        auto impliedFwdCurve =
            QuantLib::ext::make_shared<ModelImpliedYtsFwdFwdCorrected>(irModel[indexCcyIdx_[j]], fts, dc, true);
        fwdCurves_.push_back(impliedFwdCurve);
        indices_.push_back(index->clone(Handle<YieldTermStructure>(impliedFwdCurve)));
        curvesCacheLoopSize[indexCcyIdx_[j]] += dates_.size() * time_idx_[j][0].size();
    }

    yieldCurveCcyIndex_.resize(n_curves_);
    for (Size j = 0; j < n_curves_; ++j) {
        std::string curveName = simMarketConfig_->yieldCurveNames()[j];
        Currency ccy = ore::data::parseCurrency(simMarketConfig_->yieldCurveCurrencies().at(curveName));
        yieldCurveCcyIndex_[j] = model_->ccyIndex(ccy);
        Handle<YieldTermStructure> yts = initMarket_->yieldCurve(curveName, configuration_);
        auto impliedYieldCurve =
            QuantLib::ext::make_shared<ModelImpliedYtsFwdFwdCorrected>(irModel[yieldCurveCcyIndex_[j]], yts, dc, true);
        yieldCurves_.push_back(impliedYieldCurve);
        curvesCacheLoopSize[yieldCurveCcyIndex_[j]] += dates_.size() * time_yc_[j][0].size();
    }

    for (Size j = 0; j < n_com_; ++j) {
        QuantLib::ext::shared_ptr<CommodityModel> cm = model_->comModel(j);
        auto pts = QuantLib::ext::make_shared<QuantExt::ModelImpliedPriceTermStructure>(model_->comModel(j), dc, true);
        comCurves_.push_back(pts);
    }

    for (Size j = 0; j < n_ccy_; ++j) {
        // numeraire cache loop size is dates_.size(), only relevant for j == 0
        irModel[j]->enableCache(true, {curvesCacheLoopSize[j], dates_.size()});
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
        auto mt = model_->modelType(CrossAssetModel::AssetType::INF, idx);
        QL_REQUIRE(mt == CrossAssetModel::ModelType::DK || mt == CrossAssetModel::ModelType::JY,
                   "CrossAssetModelScenarioGenerator: expected inflation model to be JY or DK.");
        QuantLib::ext::shared_ptr<ZeroInflationModelTermStructure> ts;

        if (mt == CrossAssetModel::ModelType::DK) {
            ts = QuantLib::ext::make_shared<DkImpliedZeroInflationTermStructure>(model_, idx, dateGrid_->dayCounter());
        } else {
            ts = QuantLib::ext::make_shared<JyImpliedZeroInflationTermStructure>(model_, idx, dateGrid_->dayCounter());
            QL_REQUIRE(model_->modelType(CrossAssetModel::AssetType::IR, 0) == CrossAssetModel::ModelType::LGM1F,
                       "Simulation of INF JY model is only supported for LGM1F ir model type.");
        }
        zeroInfCurves_.emplace_back(idx, ccyIdx, mt, ts);
        ts->enableCache();
    }

    // Same logic here as for the zeroInfCurves above.
    for (const auto& name : simMarketConfig_->yoyInflationIndices()) {
        auto idx = model_->infIndex(name);
        auto ccyIdx = model_->ccyIndex(model_->inf(idx)->currency());
        auto mt = model_->modelType(CrossAssetModel::AssetType::INF, idx);
        QL_REQUIRE(mt == CrossAssetModel::ModelType::DK || mt == CrossAssetModel::ModelType::JY,
                   "CrossAssetModelScenarioGenerator: expected inflation model to be JY or DK.");
        QuantLib::ext::shared_ptr<YoYInflationModelTermStructure> ts;
        if (mt == CrossAssetModel::ModelType::DK) {
            ts = QuantLib::ext::make_shared<DkImpliedYoYInflationTermStructure>(model_, idx, false);
        } else {
            ts = QuantLib::ext::make_shared<JyImpliedYoYInflationTermStructure>(model_, idx, false);
        }
        QL_REQUIRE(model_->modelType(CrossAssetModel::AssetType::IR, 0) == CrossAssetModel::ModelType::LGM1F,
                   "Simulation of INF DK or JY model for YoY curves is only supported for LGM1F ir model type.");
        yoyInfCurves_.emplace_back(idx, ccyIdx, mt, ts);
        ts->enableCache();
    }

    for (Size j = 0; j < n_cr_; ++j) {
        if (model_->modelType(CrossAssetModel::AssetType::CR, j) == CrossAssetModel::ModelType::LGM1F) {
            lgmDefaultCurves_.push_back(QuantLib::ext::make_shared<QuantExt::LgmImpliedDefaultTermStructure>(
                model_, j, model_->ccyIndex(model_->crlgm1f(j)->currency())));
            cirppDefaultCurves_.push_back(QuantLib::ext::shared_ptr<QuantExt::CirppImpliedDefaultTermStructure>());
        } else if (model_->modelType(CrossAssetModel::AssetType::CR, j) == CrossAssetModel::ModelType::CIRPP) {
            lgmDefaultCurves_.push_back(QuantLib::ext::shared_ptr<QuantExt::LgmImpliedDefaultTermStructure>());
            cirppDefaultCurves_.push_back(
                QuantLib::ext::make_shared<QuantExt::CirppImpliedDefaultTermStructure>(model_->crcirppModel(j), j));
        }
    }

    // if we have an amcPathDataOutput create the PathData
    if (!amcPathDataOutput_.empty()) {
        pathData_.fxBuffer.resize(
            n_fx_, std::vector<std::vector<Real>>(dateGrid_->size() + 1, std::vector<Real>(totalSamples_)));
        pathData_.irStateBuffer.resize(
            n_ccy_, std::vector<std::vector<Real>>(dateGrid_->dates().size() + 1, std::vector<Real>(totalSamples_)));
        pathData_.pathTimes =
            std::vector<Real>(std::next(dateGrid_->timeGrid().begin(), 1), dateGrid_->timeGrid().end());
        pathData_.paths.resize(pathData_.pathTimes.size(),
                               std::vector<RandomVariable>(n_states_, RandomVariable(totalSamples_)));
    }

    LOG("CrossAssetModelScenarioGenerator ctor done");
} // init

namespace {
void copyPathToArray(const MultiPath& p, Size t, Size a, Array& target) {
    for (Size k = 0; k < target.size(); ++k)
        target[k] = p[a + k][t];
}
} // namespace

std::vector<QuantLib::ext::shared_ptr<Scenario>> CrossAssetModelScenarioGenerator::nextPath() {

    if(!initialized_) {
        init();
        initialized_ = true;
    }

    std::vector<QuantLib::ext::shared_ptr<Scenario>> scenarios(dates_.size());
    QL_REQUIRE(pathGenerator_ != nullptr, "CrossAssetModelScenarioGenerator::nextPath(): pathGenerator is null");
    Sample<MultiPath> sample = pathGenerator_->next();
    ++currentSample_;
    DayCounter dc = model_->irModel(0)->termStructure()->dayCounter();

    if (!amcPathDataOutput_.empty()) {
        for (Size k = 0; k < n_fx_; ++k) {
            for (Size j = 0; j < timeGrid_.size(); ++j) {
                pathData_.fxBuffer[k][j][currentSample_ - 1] =
                    std::exp(sample.value[model_->pIdx(CrossAssetModel::AssetType::FX, k)][j]);
            }
        }

        for (Size k = 0; k < n_ccy_; ++k) {
            for (Size j = 0; j < timeGrid_.size(); ++j) {
                pathData_.irStateBuffer[k][j][currentSample_ - 1] =
                    sample.value[model_->pIdx(CrossAssetModel::AssetType::IR, k)][j];
            }
        }

        for (Size k = 0; k < n_states_; ++k) {
            for (Size j = 0; j < pathData_.pathTimes.size(); ++j) {
                pathData_.paths[j][k].set(currentSample_ - 1, sample.value[k][j + 1]);
            }
        }
    }

    auto timingStart = data::os::nanosecondsClock();

    std::vector<Array> ir_state(n_ccy_);
    for (Size j = 0; j < n_ccy_; ++j) {
        ir_state[j] = Array(model_->irModel(j)->n() + model_->irModel(j)->n_aux());
    }

    for (Size i = 0; i < dates_.size(); i++) {
        // FIXME we need to check the consistency of model and scen-gen day counters, as we already do in amc val engine QPR-13995
        Real t = timeGrid_[i + 1];
        Real t_dc = dc.yearFraction(model_->irModel(0)->termStructure()->referenceDate(), dates_[i]);

        scenarios[i] = scenarioFactory_->buildScenario(dates_[i], true);
        Size rfKeyCounter = 0;

        // populate IR states
        for (Size j = 0; j < n_ccy_; ++j)
            copyPathToArray(sample.value, gridIndexInPath_[i + 1], model_->pIdx(CrossAssetModel::AssetType::IR, j),
                            ir_state[j]);

        // Set numeraire from domestic ir process
        scenarios[i]->setNumeraire(model_->numeraire(0, t, ir_state[0], Handle<YieldTermStructure>()));

        // Discount curves
        for (Size j = 0; j < n_ccy_; j++) {
            curves_[j]->move(t, ir_state[j]);
            for (Size k = 0; k < time_dsc_[j][i].size(); k++) {
                scenarios[i]->add(rfKeyCounter++, std::max(curves_[j]->discount(time_dsc_[j][i][k]), 0.00001));
            }
        }

        // Index curves and Index fixings
        for (Size j = 0; j < n_indices_; ++j) {
            fwdCurves_[j]->move(t_dc, ir_state[indexCcyIdx_[j]]);
            for (Size k = 0; k < time_idx_[j][i].size(); ++k) {
                scenarios[i]->add(rfKeyCounter++, std::max(fwdCurves_[j]->discount(time_idx_[j][i][k]), 0.00001));
            }
        }

        // Yield curves
        for (Size j = 0; j < n_curves_; ++j) {
            yieldCurves_[j]->move(t_dc, ir_state[yieldCurveCcyIndex_[j]]);
            for (Size k = 0; k < time_yc_[j][i].size(); ++k) {
                scenarios[i]->add(rfKeyCounter++, std::max(yieldCurves_[j]->discount(time_yc_[j][i][k]), 0.00001));
            }
        }

        // FX rates
        for (Size k = 0; k < n_ccy_ - 1; k++) {
            Real fx = std::exp(sample.value[model_->pIdx(CrossAssetModel::AssetType::FX, k)][gridIndexInPath_[i + 1]]);
            scenarios[i]->add(rfKeyCounter++, fx);
        }

        // FX vols
        if (simMarketConfig_->simulateFXVols()) {
            for (Size k = 0; k < simMarketConfig_->fxVolCcyPairs().size(); k++) {
                const string ccyPair = simMarketConfig_->fxVolCcyPairs()[k];
                const vector<Period>& expires = simMarketConfig_->fxVolExpiries(ccyPair);

                Size fxIndex = fxVols_[k]->fxIndex();
                Real zFor = sample.value[fxIndex + 1][gridIndexInPath_[i + 1]];
                Real logFx =
                    sample.value[n_ccy_ + fxIndex][gridIndexInPath_[i + 1]]; // multiplies USD amount to get EUR
                fxVols_[k]->move(t, ir_state[0][0], zFor, logFx);

                for (Size j = 0; j < expires.size(); j++) {
                    Real vol = fxVols_[k]->blackVol(dates_[i] + expires[j], Null<Real>(), true);
                    scenarios[i]->add(rfKeyCounter++, vol);
                }
            }
        }


        // Equity spots
        for (Size k = 0; k < n_eq_; k++) {
            Real eqSpot =
                std::exp(sample.value[model_->pIdx(CrossAssetModel::AssetType::EQ, k)][gridIndexInPath_[i + 1]]);
            scenarios[i]->add(rfKeyCounter++, eqSpot);
        }

        // Equity vols
        if (simMarketConfig_->simulateEquityVols()) {
            for (Size k = 0; k < simMarketConfig_->equityVolNames().size(); k++) {
                const string equityName = simMarketConfig_->equityVolNames()[k];

                const vector<Period>& expiries = simMarketConfig_->equityVolExpiries(equityName);

                Size eqIndex = eqVols_[k]->equityIndex();
                Size eqCcyIdx = eqVols_[k]->eqCcyIndex();
                Real z_eqIr = sample.value[eqCcyIdx][gridIndexInPath_[i + 1]];
                Real logEq = sample.value[eqIndex][gridIndexInPath_[i + 1]];
                eqVols_[k]->move(t, z_eqIr, logEq);

                for (Size j = 0; j < expiries.size(); j++) {
                    Real vol = eqVols_[k]->blackVol(dates_[i] + expiries[j], Null<Real>(), true);
                    scenarios[i]->add(rfKeyCounter++, vol);
                }
            }
        }

        // Swaption vols
        if (simMarketConfig_->simulateSwapVols()) {
            for (Size k = 0; k < simMarketConfig_->swapVolKeys().size(); k++) {
                const string key = simMarketConfig_->swapVolKeys()[k];
                const vector<Period>& expires = simMarketConfig_->swapVolExpiries(key);
                const vector<Period>& terms = simMarketConfig_->swapVolTerms(key);

                // ext::shared_ptr<YieldTermStructure> dts = curves_[swaptionVols_[k]->ccyIndex()];
                // ext::shared_ptr<YieldTermStructure> fts = fwdCurves_[swaptionVols_[k]->indexIndex()];
                // Update the implied swaption vols
                swaptionVols_[k]->move(t, ir_state[0][0]);

                for (Size j = 0; j < expires.size(); j++) {
                    for (Size jj = 0; jj < terms.size(); jj++) {
                        Real vol = swaptionVols_[k]->volatility(dates_[i] + expires[j], terms[jj], Null<Real>(), true);
                        scenarios[i]->add(rfKeyCounter++, vol);
                    }
                }
            }
        }

        // Inflation index values
        for (Size j = 0; j < n_inf_; j++) {
            // Depending on type of model, i.e. DK or JY, z and y mean different things.
            Real z = sample.value[model_->pIdx(CrossAssetModel::AssetType::INF, j, 0)][gridIndexInPath_[i + 1]];
            Real y = sample.value[model_->pIdx(CrossAssetModel::AssetType::INF, j, 1)][gridIndexInPath_[i + 1]];
            auto index = *initMarket_->zeroInflationIndex(model_->inf(j)->name());
            auto zts = index->zeroInflationTermStructure();
            Real cpi = scenarioBaseCpi(y, z, dates_[i], model_, j, dateGrid_->dayCounter(), index);
            Date fixingDate = inflationPeriod(dates_[i] - simulationLag(zts), zts->frequency()).first;
            cpi = seasonalizeCPI(fixingDate, cpi, zts);
            scenarios[i]->add(rfKeyCounter++, cpi);
        }

        // Zero inflation curves
        for (Size j = 0; j < zeroInfCurves_.size(); ++j) {

            auto [idx, ccyIdx, modelType, ts] = zeroInfCurves_[j];

            // State variables needed depends on model, 3 for JY and 2 for DK.
            
            Array state(3);
            state[0] = sample.value[model_->pIdx(CrossAssetModel::AssetType::INF, idx, 0)][gridIndexInPath_[i + 1]];
            state[1] = sample.value[model_->pIdx(CrossAssetModel::AssetType::INF, idx, 1)][gridIndexInPath_[i + 1]];
            if (modelType == CrossAssetModel::ModelType::DK) {
                state.resize(2);
            } else {
                state[2] = ir_state[ccyIdx][0];
            }

            // Update the term structure's date and state.
            ts->move(dates_[i], state);

            // Populate the zero inflation scenario values based on the current date and state.
            for (Size k = 0; k < ten_zinf_[j].size(); k++) {
                auto index = *initMarket_->zeroInflationIndex(model_->inf(idx)->name());
                auto obsLag = ts->observationLag();
                auto zeroRate =
                    scenarioInflationZeroRateFromModelTs(dates_[i], ten_zinf_[j][k], obsLag, index, ts, modelType, dc);
                scenarios[i]->add(rfKeyCounter++, zeroRate);
            }
        }

        // YoY inflation curves
        for (Size j = 0; j < yoyInfCurves_.size(); ++j) {

            auto tup = yoyInfCurves_[j];

            // For YoY model implied term structure, JY and DK both need 3 state variables.
            auto idx = std::get<0>(tup);
            Array state(3);
            state[0] = sample.value[model_->pIdx(CrossAssetModel::AssetType::INF, idx, 0)][gridIndexInPath_[i + 1]];
            state[1] = sample.value[model_->pIdx(CrossAssetModel::AssetType::INF, idx, 1)][gridIndexInPath_[i + 1]];
            state[2] = ir_state[std::get<1>(tup)][0];

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
                scenarios[i]->add(rfKeyCounter++, yoyRates.at(pillarDates[k]));
            }
        }

        // Credit curves
        for (Size j = 0; j < n_cr_; ++j) {
            if (model_->modelType(CrossAssetModel::AssetType::CR, j) == CrossAssetModel::ModelType::LGM1F) {
                Real z = sample.value[model_->pIdx(CrossAssetModel::AssetType::CR, j, 0)][gridIndexInPath_[i + 1]];
                Real y = sample.value[model_->pIdx(CrossAssetModel::AssetType::CR, j, 1)][gridIndexInPath_[i + 1]];
                lgmDefaultCurves_[j]->move(dates_[i], z, y);
                for (Size k = 0; k < ten_dfc_[j].size(); k++) {
                    Date d = dates_[i] + ten_dfc_[j][k];
                    Time T = dc.yearFraction(dates_[i], d);
                    Real survProb = std::max(lgmDefaultCurves_[j]->survivalProbability(T), 0.00001);
                    scenarios[i]->add(rfKeyCounter++, survProb);
                }
            } else if (model_->modelType(CrossAssetModel::AssetType::CR, j) == CrossAssetModel::ModelType::CIRPP) {
                Real y = sample.value[model_->pIdx(CrossAssetModel::AssetType::CR, j, 0)][gridIndexInPath_[i + 1]];
                cirppDefaultCurves_[j]->move(dates_[i], y);
                for (Size k = 0; k < ten_dfc_[j].size(); k++) {
                    Date d = dates_[i] + ten_dfc_[j][k];
                    Time T = dc.yearFraction(dates_[i], d);
                    Real survProb = std::max(cirppDefaultCurves_[j]->survivalProbability(T), 0.00001);
                    scenarios[i]->add(rfKeyCounter++, survProb);
                }
            }
        }

        // Commodity curves
        Array comState(1, 0.0); // FIXME: single-factor for now
        for (Size j = 0; j < n_com_; j++) {
            comState[0] = sample.value[model_->pIdx(CrossAssetModel::AssetType::COM, j)][gridIndexInPath_[i + 1]];
            comCurves_[j]->move(t, comState);
            for (Size k = 0; k < ten_com_[j].size(); k++) {
                Date d = dates_[i] + ten_com_[j][k];
                Time T = dc.yearFraction(dates_[i], d);
                Real price = std::max(comCurves_[j]->price(T), 0.00001);
                scenarios[i]->add(rfKeyCounter++, price);
            }
        }

        // Credit States
        for (Size k = 0; k < n_crstates_; ++k) {
            Real z = sample.value[model_->pIdx(CrossAssetModel::AssetType::CrState, k)][gridIndexInPath_[i + 1]];
            scenarios[i]->add(rfKeyCounter++, z);
        }

        // Survival Weights, stochastic cumulative survival probability, Recovery Rates
        for (Size k = 0; k < n_survivalweights_; ++k) {
            string name = simMarketConfig_->additionalScenarioDataSurvivalWeights()[k];
            Real rr = survivalWeightsDefaultCurves_[k]->recovery().empty()
                          ? 0.0
                          : survivalWeightsDefaultCurves_[k]->recovery()->value();
            scenarios[i]->add(rfKeyCounter++,
                              survivalWeightsDefaultCurves_[k]->curve()->survivalProbability(dates_[i]));
            scenarios[i]->add(rfKeyCounter++, rr);
        }
    }

    timing_ += data::os::nanosecondsClock() - timingStart;

    if (totalSamples_ == currentSample_ && !amcPathDataOutput_.empty()) {
        LOG("Serialize paths, fx and irState buffers to'" << amcPathDataOutput_ << "'");
        std::ofstream os(amcPathDataOutput_, std::ios::binary);
        boost::archive::binary_oarchive oa(os, boost::archive::no_header);
        oa << pathData_;
        os.close();
    }

    return scenarios;
}

void CrossAssetModelScenarioGenerator::reset() {

    pathGenerator_->reset();

    for (auto const& [x, b, m, t] : zeroInfCurves_)
        t->clearCache();
    for (auto const& [x, b, m, t] : yoyInfCurves_)
        t->clearCache();
}

} // namespace analytics
} // namespace ore
