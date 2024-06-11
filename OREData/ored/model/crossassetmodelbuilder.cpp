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

#include <ored/model/commodityschwartzmodelbuilder.hpp>
#include <ored/model/crcirbuilder.hpp>
#include <ored/model/crlgmbuilder.hpp>
#include <ored/model/crossassetmodelbuilder.hpp>
#include <ored/model/eqbsbuilder.hpp>
#include <ored/model/fxbsbuilder.hpp>
#include <ored/model/hwbuilder.hpp>
#include <ored/model/inflation/infdkbuilder.hpp>
#include <ored/model/inflation/infjybuilder.hpp>
#include <ored/model/inflation/infjydata.hpp>
#include <ored/model/irhwmodeldata.hpp>
#include <ored/model/lgmbuilder.hpp>
#include <ored/model/structuredmodelwarning.hpp>
#include <ored/model/utilities.hpp>
#include <ored/utilities/correlationmatrix.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>

#include <qle/cashflows/jyyoyinflationcouponpricer.hpp>
#include <qle/models/cpicapfloorhelper.hpp>
#include <qle/models/fxbsconstantparametrization.hpp>
#include <qle/models/fxbspiecewiseconstantparametrization.hpp>
#include <qle/models/fxeqoptionhelper.hpp>
#include <qle/models/irlgm1fconstantparametrization.hpp>
#include <qle/models/irlgm1fpiecewiseconstanthullwhiteadaptor.hpp>
#include <qle/models/irlgm1fpiecewiseconstantparametrization.hpp>
#include <qle/models/irlgm1fpiecewiselinearparametrization.hpp>
#include <qle/models/yoycapfloorhelper.hpp>
#include <qle/models/yoyswaphelper.hpp>
#include <qle/pricingengines/analyticcclgmfxoptionengine.hpp>
#include <qle/pricingengines/analyticdkcpicapfloorengine.hpp>
#include <qle/pricingengines/analyticjycpicapfloorengine.hpp>
#include <qle/pricingengines/analyticjyyoycapfloorengine.hpp>
#include <qle/pricingengines/analyticlgmswaptionengine.hpp>
#include <qle/pricingengines/analyticxassetlgmeqoptionengine.hpp>

#include <ql/math/optimization/levenbergmarquardt.hpp>
#include <ql/models/shortrate/calibrationhelpers/swaptionhelper.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/utilities/dataformatters.hpp>

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/lexical_cast.hpp>

using QuantExt::AnalyticJyCpiCapFloorEngine;
using QuantExt::AnalyticJyYoYCapFloorEngine;
using QuantExt::CpiCapFloorHelper;
using QuantExt::InfDkParametrization;
using QuantExt::IrLgm1fParametrization;
using QuantExt::JyYoYInflationCouponPricer;
using QuantExt::YoYCapFloorHelper;
using QuantExt::YoYSwapHelper;
using QuantLib::DiscountingSwapEngine;
using std::vector;

namespace ore {
namespace data {

CrossAssetModelBuilder::CrossAssetModelBuilder(
    const QuantLib::ext::shared_ptr<ore::data::Market>& market, const QuantLib::ext::shared_ptr<CrossAssetModelData>& config,
    const std::string& configurationLgmCalibration, const std::string& configurationFxCalibration,
    const std::string& configurationEqCalibration, const std::string& configurationInfCalibration,
    const std::string& configurationCrCalibration, const std::string& configurationFinalModel, const bool dontCalibrate,
    const bool continueOnError, const std::string& referenceCalibrationGrid, const SalvagingAlgorithm::Type salvaging,
    const std::string& id)
    : market_(market), config_(config), configurationLgmCalibration_(configurationLgmCalibration),
      configurationFxCalibration_(configurationFxCalibration), configurationEqCalibration_(configurationEqCalibration),
      configurationInfCalibration_(configurationInfCalibration),
      configurationCrCalibration_(configurationCrCalibration),
      configurationComCalibration_(Market::defaultConfiguration), configurationFinalModel_(configurationFinalModel),
      dontCalibrate_(dontCalibrate), continueOnError_(continueOnError),
      referenceCalibrationGrid_(referenceCalibrationGrid), salvaging_(salvaging), id_(id),
      optimizationMethod_(QuantLib::ext::shared_ptr<OptimizationMethod>(new LevenbergMarquardt(1E-8, 1E-8, 1E-8))),
      endCriteria_(EndCriteria(1000, 500, 1E-8, 1E-8, 1E-8)) {
    buildModel();
    // register with sub builders
    for (auto okv : subBuilders_)
        for (auto ikv : okv.second) {
            registerWith(ikv.second);
        }
    // register market observer with correlations
    marketObserver_ = QuantLib::ext::make_shared<MarketObserver>();
    for (auto const& c : config->correlations())
        marketObserver_->addObservable(c.second);
    // reset market observer's updated flag
    marketObserver_->hasUpdated(true);
    // register with market observer
    registerWith(marketObserver_);
}

Handle<QuantExt::CrossAssetModel> CrossAssetModelBuilder::model() const {
    calculate();
    return model_;
}

const std::vector<Real>& CrossAssetModelBuilder::swaptionCalibrationErrors() {
    calculate();
    return swaptionCalibrationErrors_;
}
const std::vector<Real>& CrossAssetModelBuilder::fxOptionCalibrationErrors() {
    calculate();
    return fxOptionCalibrationErrors_;
}
const std::vector<Real>& CrossAssetModelBuilder::eqOptionCalibrationErrors() {
    calculate();
    return eqOptionCalibrationErrors_;
}
const std::vector<Real>& CrossAssetModelBuilder::inflationCalibrationErrors() {
    calculate();
    return inflationCalibrationErrors_;
}
const std::vector<Real>& CrossAssetModelBuilder::comOptionCalibrationErrors() {
    calculate();
    return comOptionCalibrationErrors_;
}

bool CrossAssetModelBuilder::requiresRecalibration() const {
    for (auto okv : subBuilders_)
        for (auto ikv : okv.second)
            if (ikv.second->requiresRecalibration())
                return true;

    return marketObserver_->hasUpdated(false);
}

void CrossAssetModelBuilder::performCalculations() const {
    // if any of the sub models requires a recalibration, we rebuilt the model
    // TODO we could do this more selectively
    if (!dontCalibrate_ && requiresRecalibration()) {
        // reset market observer update flag
        marketObserver_->hasUpdated(true);
        buildModel();
    }
}

void CrossAssetModelBuilder::resetModelParams(const CrossAssetModel::AssetType t, const Size param, const Size index,
                                              const Size i) const {
    auto mp = model_->MoveParameter(t, param, index, i);
    for (Size idx = 0; idx < mp.size(); ++idx) {
        if (!mp[idx]) {
            model_->setParam(idx, params_[idx]);
        }
    }
}

void CrossAssetModelBuilder::copyModelParams(const CrossAssetModel::AssetType t0, const Size param0, const Size index0,
                                             const Size i0, const CrossAssetModel::AssetType t1, const Size param1,
                                             const Size index1, const Size i1, const Real mult) const {
    auto mp0 = model_->MoveParameter(t0, param0, index0, i0);
    auto mp1 = model_->MoveParameter(t1, param1, index1, i1);
    auto s0 = 0, s1 = 0;
    for (Size i = 0; i < mp0.size(); ++i)
        if (!mp0[i])
            s0++;
    for (Size i = 0; i < mp1.size(); ++i)
        if (!mp1[i])
            s1++;
    QL_REQUIRE(s0 == s1, "CrossAssetModelBuilder::copyModelParams(): source range size ("
                             << s0 << ") does not match target range size (" << s1 << ") when copying (" << t0 << ","
                             << param0 << "," << index0 << "," << i0 << ") -> (" << t1 << "," << param1 << "," << index1
                             << "," << i1 << ")");
    std::vector<Real> sourceValues(s0);
    for (Size idx0 = 0, count = 0; idx0 < mp0.size(); ++idx0) {
        if (!mp0[idx0]) {
            sourceValues[count++] = params_[idx0];
        }
    }
    for (Size idx1 = 0, count = 0; idx1 < mp1.size(); ++idx1) {
        if (!mp1[idx1]) {
            model_->setParam(idx1, sourceValues[count++] * mult);
        }
    }
}

void CrossAssetModelBuilder::buildModel() const {

    LOG("Start building CrossAssetModel");

    QL_REQUIRE(market_ != NULL, "CrossAssetModelBuilder: no market given");

    DLOG("configurations: LgmCalibration "
         << configurationLgmCalibration_ << ", FxCalibration " << configurationFxCalibration_ << ", EqCalibration "
         << configurationEqCalibration_ << ", InfCalibration " << configurationInfCalibration_ << ", CrCalibration"
         << configurationCrCalibration_ << ", ComCalibration" << configurationComCalibration_ << ", FinalModel "
         << configurationFinalModel_);

    if (dontCalibrate_) {
        DLOG("Calibration of the model is disabled.");
    }

    bool buildersAreInitialized = !subBuilders_.empty();

    if (!buildersAreInitialized) {
        QL_REQUIRE(config_->irConfigs().size() > 0, "missing IR configurations");
        QL_REQUIRE(config_->irConfigs().size() == config_->fxConfigs().size() + 1,
                   "FX configuration size " << config_->fxConfigs().size()
                                            << " inconsistent with IR configuration size "
                                            << config_->irConfigs().size());

        swaptionBaskets_.resize(config_->irConfigs().size());
        optionExpiries_.resize(config_->irConfigs().size());
        swaptionMaturities_.resize(config_->irConfigs().size());
        swaptionCalibrationErrors_.resize(config_->irConfigs().size());
        fxOptionBaskets_.resize(config_->fxConfigs().size());
        fxOptionExpiries_.resize(config_->fxConfigs().size());
        fxOptionCalibrationErrors_.resize(config_->fxConfigs().size());
        eqOptionBaskets_.resize(config_->eqConfigs().size());
        eqOptionExpiries_.resize(config_->eqConfigs().size());
        eqOptionCalibrationErrors_.resize(config_->eqConfigs().size());
        inflationCalibrationErrors_.resize(config_->infConfigs().size());
        comOptionBaskets_.resize(config_->comConfigs().size());
        comOptionExpiries_.resize(config_->comConfigs().size());
        comOptionCalibrationErrors_.resize(config_->comConfigs().size());
    }

    // Store information on the number of factors for each process. This is used when requesting a correlation matrix
    // from the CorrelationMatrixBuilder below.
    CorrelationMatrixBuilder::ProcessInfo processInfo;

    // Set the measure
    IrModel::Measure measure = IrModel::Measure::LGM;
    if (config_->measure() == "BA") {
        measure = IrModel::Measure::BA;
        DLOG("Setting measure to BA");
    } else if (config_->measure() == "LGM") {
        measure = IrModel::Measure::LGM;
        DLOG("Setting measure to LGM");
    } else if (config_->measure() == "") {
        DLOG("Defaulting to LGM measure");
    } else {
        QL_FAIL("Measure " << config_->measure() << " not recognized");
    }

    /*******************************************************
     * Build the IR parametrizations and calibration baskets
     */
    std::vector<QuantLib::ext::shared_ptr<QuantExt::Parametrization>> irParametrizations;
    std::vector<RelinkableHandle<YieldTermStructure>> irDiscountCurves;
    std::vector<std::string> currencies, regions, crNames, eqNames, infIndices, comNames;
    std::vector<QuantLib::ext::shared_ptr<LgmBuilder>> lgmBuilder;
    std::vector<QuantLib::ext::shared_ptr<HwBuilder>> hwBuilder;
    std::vector<QuantLib::ext::shared_ptr<FxBsBuilder>> fxBuilder;
    std::vector<QuantLib::ext::shared_ptr<EqBsBuilder>> eqBuilder;
    std::vector<QuantLib::ext::shared_ptr<CommoditySchwartzModelBuilder>> csBuilder;

    std::set<std::string> recalibratedCurrencies;
    for (Size i = 0; i < config_->irConfigs().size(); i++) {
        auto irConfig = config_->irConfigs()[i];
        DLOG("IR Parametrization " << i << " qualifier " << irConfig->qualifier());

        if (auto ir = QuantLib::ext::dynamic_pointer_cast<IrLgmData>(irConfig)) {
            if (!buildersAreInitialized) {
                subBuilders_[CrossAssetModel::AssetType::IR][i] = QuantLib::ext::make_shared<LgmBuilder>(
                    market_, ir, configurationLgmCalibration_, config_->bootstrapTolerance(), continueOnError_,
                    referenceCalibrationGrid_, false, id_);
            }
            auto builder = QuantLib::ext::dynamic_pointer_cast<LgmBuilder>(subBuilders_[CrossAssetModel::AssetType::IR][i]);
            lgmBuilder.push_back(builder);
            if (dontCalibrate_) {
                builder->freeze();
            }
            if (builder->requiresRecalibration())
                recalibratedCurrencies.insert(builder->parametrization()->currency().code());
            auto parametrization = builder->parametrization();
            swaptionBaskets_[i] = builder->swaptionBasket();
            QL_REQUIRE(std::find(currencies.begin(), currencies.end(), parametrization->currency().code()) ==
                           currencies.end(),
                       "Duplicate IR parameterization for currency "
                           << parametrization->currency().code()
                           << " - are there maybe two indices with the same currency in CrossAssetModelData?");
            currencies.push_back(parametrization->currency().code());
            irParametrizations.push_back(parametrization);
            irDiscountCurves.push_back(builder->discountCurve());
            processInfo[CrossAssetModel::AssetType::IR].emplace_back(ir->ccy(), 1);
        } else if (auto ir = QuantLib::ext::dynamic_pointer_cast<HwModelData>(irConfig)) {
            bool evaluateBankAccount = true; // updated in cross asset model for non-base ccys
            bool setCalibrationInfo = false;
            HwModel::Discretization discr = HwModel::Discretization::Euler;
            if (!buildersAreInitialized) {
                subBuilders_[CrossAssetModel::AssetType::IR][i] = QuantLib::ext::make_shared<HwBuilder>(
                    market_, ir, measure, discr, evaluateBankAccount, configurationLgmCalibration_,
                    config_->bootstrapTolerance(), continueOnError_, referenceCalibrationGrid_, setCalibrationInfo);
            }
            auto builder = QuantLib::ext::dynamic_pointer_cast<HwBuilder>(subBuilders_[CrossAssetModel::AssetType::IR][i]);
            hwBuilder.push_back(builder);
            if (builder->requiresRecalibration())
                recalibratedCurrencies.insert(builder->parametrization()->currency().code());
            auto parametrization = builder->parametrization();
            if (dontCalibrate_)
                builder->freeze();
            swaptionBaskets_[i] = builder->swaptionBasket();
            QL_REQUIRE(std::find(currencies.begin(), currencies.end(), parametrization->currency().code()) ==
                           currencies.end(),
                       "Duplicate IR parameterization for currency "
                           << parametrization->currency().code()
                           << " - are there maybe two indices with the same currency in CrossAssetModelData?");
            currencies.push_back(parametrization->currency().code());
            irParametrizations.push_back(parametrization);
            irDiscountCurves.push_back(builder->discountCurve());
            processInfo[CrossAssetModel::AssetType::IR].emplace_back(ir->ccy(), parametrization->m());
        }
    }

    QL_REQUIRE(irParametrizations.size() > 0, "missing IR parametrizations");

    QuantLib::Currency domesticCcy = irParametrizations[0]->currency();

    /*******************************************************
     * Build the FX parametrizations and calibration baskets
     */
    std::vector<QuantLib::ext::shared_ptr<QuantExt::FxBsParametrization>> fxParametrizations;
    for (Size i = 0; i < config_->fxConfigs().size(); i++) {
        DLOG("FX Parametrization " << i);
        QuantLib::ext::shared_ptr<FxBsData> fx = config_->fxConfigs()[i];
        QuantLib::Currency ccy = ore::data::parseCurrency(fx->foreignCcy());
        QuantLib::Currency domCcy = ore::data::parseCurrency(fx->domesticCcy());

        QL_REQUIRE(ccy.code() == irParametrizations[i + 1]->currency().code(),
                   "FX parametrization currency[" << i << "]=" << ccy << " does not match IR currency[" << i + 1
                                                  << "]=" << irParametrizations[i + 1]->currency().code());

        QL_REQUIRE(domCcy == domesticCcy, "FX parametrization [" << i << "]=" << ccy << "/" << domCcy
                                                                 << " does not match domestic ccy " << domesticCcy);

        if (!buildersAreInitialized) {
            subBuilders_[CrossAssetModel::AssetType::FX][i] =
                QuantLib::ext::make_shared<FxBsBuilder>(market_, fx, configurationFxCalibration_, referenceCalibrationGrid_);
        }
        auto builder = QuantLib::ext::dynamic_pointer_cast<FxBsBuilder>(subBuilders_[CrossAssetModel::AssetType::FX][i]);
        fxBuilder.push_back(builder);

        QuantLib::ext::shared_ptr<QuantExt::FxBsParametrization> parametrization = builder->parametrization();

        fxOptionBaskets_[i] = builder->optionBasket();
        fxParametrizations.push_back(parametrization);
        processInfo[CrossAssetModel::AssetType::FX].emplace_back(ccy.code() + domCcy.code(), 1);
    }

    /*******************************************************
     * Build the EQ parametrizations and calibration baskets
     */
    std::vector<QuantLib::ext::shared_ptr<QuantExt::EqBsParametrization>> eqParametrizations;
    for (Size i = 0; i < config_->eqConfigs().size(); i++) {
        DLOG("EQ Parametrization " << i);
        QuantLib::ext::shared_ptr<EqBsData> eq = config_->eqConfigs()[i];
        string eqName = eq->eqName();
        QuantLib::Currency eqCcy = ore::data::parseCurrency(eq->currency());
        QL_REQUIRE(std::find(currencies.begin(), currencies.end(), eqCcy.code()) != currencies.end(),
                   "Currency (" << eqCcy << ") for equity " << eqName << " not covered by CrossAssetModelData");
        if (!buildersAreInitialized) {
            subBuilders_[CrossAssetModel::AssetType::EQ][i] = QuantLib::ext::make_shared<EqBsBuilder>(
                market_, eq, domesticCcy, configurationEqCalibration_, referenceCalibrationGrid_);
        }
        QuantLib::ext::shared_ptr<EqBsBuilder> builder =
            QuantLib::ext::dynamic_pointer_cast<EqBsBuilder>(subBuilders_[CrossAssetModel::AssetType::EQ][i]);
        eqBuilder.push_back(builder);
        QuantLib::ext::shared_ptr<QuantExt::EqBsParametrization> parametrization = builder->parametrization();
        eqOptionBaskets_[i] = builder->optionBasket();
        eqParametrizations.push_back(parametrization);
        eqNames.push_back(eqName);
        processInfo[CrossAssetModel::AssetType::EQ].emplace_back(eqName, 1);
    }

    /*******************************************************
     * Build the INF parametrizations and calibration baskets
     */
    vector<QuantLib::ext::shared_ptr<Parametrization>> infParameterizations;
    for (Size i = 0; i < config_->infConfigs().size(); i++) {
        QuantLib::ext::shared_ptr<InflationModelData> imData = config_->infConfigs()[i];
        DLOG("Inflation parameterisation (" << i << ") for index " << imData->index());
        if (auto dkData = QuantLib::ext::dynamic_pointer_cast<InfDkData>(imData)) {
            if (!buildersAreInitialized) {
                subBuilders_[CrossAssetModel::AssetType::INF][i] = QuantLib::ext::make_shared<InfDkBuilder>(
                    market_, dkData, configurationInfCalibration_, referenceCalibrationGrid_, dontCalibrate_);
            }
            QuantLib::ext::shared_ptr<InfDkBuilder> builder =
                QuantLib::ext::dynamic_pointer_cast<InfDkBuilder>(subBuilders_[CrossAssetModel::AssetType::INF][i]);
            infParameterizations.push_back(builder->parametrization());
            processInfo[CrossAssetModel::AssetType::INF].emplace_back(dkData->index(), 1);
        } else if (auto jyData = QuantLib::ext::dynamic_pointer_cast<InfJyData>(imData)) {
            if (!buildersAreInitialized) {
                // for linked real rate params we have to resize the real rate params here again, because their time grid
                // might have been overwritten in the ir calibration step
                if (jyData->linkRealRateParamsToNominalRateParams()) {
                    Size ccyIndex = std::distance(currencies.begin(),
                                                  std::find(currencies.begin(), currencies.end(), jyData->currency()));
                    VolatilityParameter rrVol = jyData->realRateVolatility();
                    ReversionParameter rrRev = jyData->realRateReversion();
                    rrVol.setCalibrate(false);
                    rrRev.setCalibrate(false);
                    auto volTimes = irParametrizations[ccyIndex]->parameterTimes(0);
                    auto volValues = irParametrizations[ccyIndex]->parameterValues(0);
                    auto revTimes = irParametrizations[ccyIndex]->parameterTimes(1);
                    auto revValues = irParametrizations[ccyIndex]->parameterValues(1);
                    rrVol.setTimes(std::vector<Real>(volTimes.begin(), volTimes.end()));
                    rrRev.setTimes(std::vector<Real>(revTimes.begin(), revTimes.end()));
                    rrVol.setValues(std::vector<Real>(volValues.begin(), volValues.end()));
                    rrRev.setValues(std::vector<Real>(revValues.begin(), revValues.end()));
                    rrVol. mult(jyData->linkedRealRateVolatilityScaling());
                    jyData->setRealRateReversion(rrRev);
                    jyData->setRealRateVolatility(rrVol);
                }
                subBuilders_[CrossAssetModel::AssetType::INF][i] = QuantLib::ext::make_shared<InfJyBuilder>(
                    market_, jyData, configurationInfCalibration_, referenceCalibrationGrid_, dontCalibrate_);
            }
            auto builder = QuantLib::ext::dynamic_pointer_cast<InfJyBuilder>(subBuilders_[CrossAssetModel::AssetType::INF][i]);
            infParameterizations.push_back(builder->parameterization());
            processInfo[CrossAssetModel::AssetType::INF].emplace_back(jyData->index(), 2);
        } else {
            QL_FAIL("CrossAssetModelBuilder expects either DK or JY inflation model data.");
        }
        infIndices.push_back(imData->index());
    }

    /*******************************************************
     * Build the CR parametrizations and calibration baskets
     */
    // LGM (if any)
    std::vector<QuantLib::ext::shared_ptr<QuantExt::CrLgm1fParametrization>> crLgmParametrizations;
    for (Size i = 0; i < config_->crLgmConfigs().size(); ++i) {
        LOG("CR LGM Parametrization " << i);
        QuantLib::ext::shared_ptr<CrLgmData> cr = config_->crLgmConfigs()[i];
        string crName = cr->name();
        if (!buildersAreInitialized) {
            subBuilders_[CrossAssetModel::AssetType::CR][i] =
                QuantLib::ext::make_shared<CrLgmBuilder>(market_, cr, configurationCrCalibration_);
        }
        auto builder = QuantLib::ext::dynamic_pointer_cast<CrLgmBuilder>(subBuilders_[CrossAssetModel::AssetType::CR][i]);
        QuantLib::ext::shared_ptr<QuantExt::CrLgm1fParametrization> parametrization = builder->parametrization();
        crLgmParametrizations.push_back(parametrization);
        crNames.push_back(crName);
        processInfo[CrossAssetModel::AssetType::CR].emplace_back(crName, 1);
    }

    // CIR (if any)
    std::vector<QuantLib::ext::shared_ptr<QuantExt::CrCirppParametrization>> crCirParametrizations;
    for (Size i = 0; i < config_->crCirConfigs().size(); ++i) {
        LOG("CR CIR Parametrization " << i);
        QuantLib::ext::shared_ptr<CrCirData> cr = config_->crCirConfigs()[i];
        string crName = cr->name();
        if (!buildersAreInitialized) {
            subBuilders_[CrossAssetModel::AssetType::CR][i] =
                QuantLib::ext::make_shared<CrCirBuilder>(market_, cr, configurationCrCalibration_);
        }
        auto builder = QuantLib::ext::dynamic_pointer_cast<CrCirBuilder>(subBuilders_[CrossAssetModel::AssetType::CR][i]);
        QuantLib::ext::shared_ptr<QuantExt::CrCirppParametrization> parametrization = builder->parametrization();
        crCirParametrizations.push_back(parametrization);
        crNames.push_back(crName);
        processInfo[CrossAssetModel::AssetType::CR].emplace_back(crName, 1);
    }

    /*******************************************************
     * Build the COM parametrizations and calibration baskets
     */
    std::vector<QuantLib::ext::shared_ptr<QuantExt::CommoditySchwartzParametrization>> comParametrizations;
    for (Size i = 0; i < config_->comConfigs().size(); i++) {
        DLOG("COM Parametrization " << i);
        QuantLib::ext::shared_ptr<CommoditySchwartzData> com = config_->comConfigs()[i];
        string comName = com->name();
        QuantLib::Currency comCcy = ore::data::parseCurrency(com->currency());
        QL_REQUIRE(std::find(currencies.begin(), currencies.end(), comCcy.code()) != currencies.end(),
                   "Currency (" << comCcy << ") for commodity " << comName << " not covered by CrossAssetModelData");
        if (!buildersAreInitialized) {
            subBuilders_[CrossAssetModel::AssetType::COM][i] = QuantLib::ext::make_shared<CommoditySchwartzModelBuilder>(
                market_, com, domesticCcy, configurationComCalibration_, referenceCalibrationGrid_);
        }
        auto builder = QuantLib::ext::dynamic_pointer_cast<CommoditySchwartzModelBuilder>(
            subBuilders_[CrossAssetModel::AssetType::COM][i]);
        if (dontCalibrate_)
            builder->freeze();
        csBuilder.push_back(builder);
        QuantLib::ext::shared_ptr<QuantExt::CommoditySchwartzParametrization> parametrization = builder->parametrization();
        comOptionBaskets_[i] = builder->optionBasket();
        comParametrizations.push_back(parametrization);
        comNames.push_back(comName);
        processInfo[CrossAssetModel::AssetType::COM].emplace_back(comName, 1);
    }

    /*******************************************************
     * Build the CrState parametrizations
     */
    std::vector<QuantLib::ext::shared_ptr<QuantExt::CrStateParametrization>> crStateParametrizations;
    for (Size i = 0; i < config_->numberOfCreditStates(); i++) {
        DLOG("CrState Parametrization " << i);
        crStateParametrizations.push_back(QuantLib::ext::make_shared<QuantExt::CrStateParametrization>(i));
        processInfo[CrossAssetModel::AssetType::CrState].emplace_back(std::to_string(i), 1);
    }

    std::vector<QuantLib::ext::shared_ptr<QuantExt::Parametrization>> parametrizations;
    for (Size i = 0; i < irParametrizations.size(); i++)
        parametrizations.push_back(irParametrizations[i]);
    for (Size i = 0; i < fxParametrizations.size(); i++)
        parametrizations.push_back(fxParametrizations[i]);
    for (Size i = 0; i < eqParametrizations.size(); i++)
        parametrizations.push_back(eqParametrizations[i]);
    parametrizations.insert(parametrizations.end(), infParameterizations.begin(), infParameterizations.end());
    for (Size i = 0; i < crLgmParametrizations.size(); i++)
        parametrizations.push_back(crLgmParametrizations[i]);
    for (Size i = 0; i < crCirParametrizations.size(); i++)
        parametrizations.push_back(crCirParametrizations[i]);
    for (Size i = 0; i < comParametrizations.size(); i++)
        parametrizations.push_back(comParametrizations[i]);
    for (Size i = 0; i < crStateParametrizations.size(); i++)
        parametrizations.push_back(crStateParametrizations[i]);

    QL_REQUIRE(fxParametrizations.size() == irParametrizations.size() - 1, "mismatch in IR/FX parametrization sizes");

    /******************************
     * Build the correlation matrix
     */
    DLOG("CrossAssetModelBuilder: adding correlations.");
    CorrelationMatrixBuilder cmb;

    for (auto it = config_->correlations().begin(); it != config_->correlations().end(); it++) {
        cmb.addCorrelation(it->first.first, it->first.second, it->second);
    }

    Matrix corrMatrix = cmb.correlationMatrix(processInfo);

    TLOG("CAM correlation matrix:");
    TLOGGERSTREAM(corrMatrix);

    /*****************************
     * Build the cross asset model
     */

    model_.linkTo(QuantLib::ext::make_shared<QuantExt::CrossAssetModel>(parametrizations, corrMatrix, salvaging_, measure,
                                                                config_->discretization()));

    /* Store initial params to ensure identical start values when recalibrating a component.
       This is only used for fx, eq, inf, cr, com, for ir this is handled in LgmBuilder directly.
       Therefore it does not matter that the IR parameters are calibrated at this point already. */

    if (!buildersAreInitialized) {
        params_ = model_->params();
    }

    /*************************
     * Calibrate IR components
     */

    for (Size i = 0; i < lgmBuilder.size(); i++) {
        DLOG("IR Calibration " << i);
        swaptionCalibrationErrors_[i] = lgmBuilder[i]->error();
    }

    for (Size i = 0; i < hwBuilder.size(); i++) {
        DLOG("IR Calibration " << i);
        swaptionCalibrationErrors_[i] = hwBuilder[i]->error();
    }

    /*************************
     * Relink LGM discount curves to curves used for FX calibration
     */

    for (Size i = 0; i < irParametrizations.size(); i++) {
        auto p = irParametrizations[i];
        irDiscountCurves[i].linkTo(*market_->discountCurve(p->currency().code(), configurationFxCalibration_));
        DLOG("Relinked discounting curve for " << p->currency().code() << " for FX calibration");
    }

    /*************************
     * Calibrate FX components
     */

    for (Size i = 0; i < fxParametrizations.size(); i++) {
        QuantLib::ext::shared_ptr<FxBsData> fx = config_->fxConfigs()[i];

        if (fx->calibrationType() == CalibrationType::None || !fx->calibrateSigma()) {
            DLOG("FX Calibration " << i << " skipped");
            continue;
        }

        if (!fxBuilder[i]->requiresRecalibration() &&
            recalibratedCurrencies.find(fx->foreignCcy()) == recalibratedCurrencies.end() &&
            recalibratedCurrencies.find(fx->domesticCcy()) == recalibratedCurrencies.end()) {
            DLOG("FX Calibration "
                 << i << " skipped, since neither fx builder nor ir models in dom / for ccy were recalibrated.");
            continue;
        }

        DLOG("FX Calibration " << i);

        // attach pricing engines to helpers
        QuantLib::ext::shared_ptr<QuantExt::AnalyticCcLgmFxOptionEngine> engine =
            QuantLib::ext::make_shared<QuantExt::AnalyticCcLgmFxOptionEngine>(*model_, i);
        // enable caching for calibration
        // TODO: review this
        engine->cache(true);
        for (Size j = 0; j < fxOptionBaskets_[i].size(); j++)
            fxOptionBaskets_[i][j]->setPricingEngine(engine);

        if (!dontCalibrate_) {

            // reset to initial params to ensure identical calibration outcomes for identical baskets
            resetModelParams(CrossAssetModel::AssetType::FX, 0, i, Null<Size>());

            if (fx->calibrationType() == CalibrationType::Bootstrap && fx->sigmaParamType() == ParamType::Piecewise)
                model_->calibrateBsVolatilitiesIterative(CrossAssetModel::AssetType::FX, i, fxOptionBaskets_[i],
                                                         *optimizationMethod_, endCriteria_);
            else
                model_->calibrateBsVolatilitiesGlobal(CrossAssetModel::AssetType::FX, i, fxOptionBaskets_[i],
                                                      *optimizationMethod_, endCriteria_);

            DLOG("FX " << fx->foreignCcy() << " calibration errors:");
            fxOptionCalibrationErrors_[i] = getCalibrationError(fxOptionBaskets_[i]);
            if (fx->calibrationType() == CalibrationType::Bootstrap) {
                if (fabs(fxOptionCalibrationErrors_[i]) < config_->bootstrapTolerance()) {
                    TLOGGERSTREAM("Calibration details:");
                    TLOGGERSTREAM(
                        getCalibrationDetails(fxOptionBaskets_[i], fxParametrizations[i], irParametrizations[0]));
                    TLOGGERSTREAM("rmse = " << fxOptionCalibrationErrors_[i]);
                } else {
                    std::string exceptionMessage = "FX BS " + std::to_string(i) + " calibration error " +
                                                   std::to_string(fxOptionCalibrationErrors_[i]) +
                                                   " exceeds tolerance " +
                                                   std::to_string(config_->bootstrapTolerance());
                    StructuredModelWarningMessage("Failed to calibrate FX BS Model", exceptionMessage, id_).log();
                    WLOGGERSTREAM("Calibration details:");
                    WLOGGERSTREAM(
                        getCalibrationDetails(fxOptionBaskets_[i], fxParametrizations[i], irParametrizations[0]));
                    WLOGGERSTREAM("rmse = " << fxOptionCalibrationErrors_[i]);
                    if (!continueOnError_)
                        QL_FAIL(exceptionMessage);
                }
            }
        }
        fxBuilder[i]->setCalibrationDone();
    }

    /*************************
     * Relink LGM discount curves to curves used for EQ calibration
     */

    for (Size i = 0; i < irParametrizations.size(); i++) {
        auto p = irParametrizations[i];
        irDiscountCurves[i].linkTo(*market_->discountCurve(p->currency().code(), configurationEqCalibration_));
        DLOG("Relinked discounting curve for " << p->currency().code() << " for EQ calibration");
    }

    /*************************
     * Calibrate EQ components
     */

    for (Size i = 0; i < eqParametrizations.size(); i++) {
        QuantLib::ext::shared_ptr<EqBsData> eq = config_->eqConfigs()[i];
        if (!eq->calibrateSigma()) {
            DLOG("EQ Calibration " << i << " skipped");
            continue;
        }

        if (!eqBuilder[i]->requiresRecalibration() &&
            recalibratedCurrencies.find(eq->currency()) == recalibratedCurrencies.end()) {
            DLOG("EQ Calibration " << i
                                   << " skipped, since neither eq builder nor ir model in eq ccy were recalibrated.");
            continue;
        }

        DLOG("EQ Calibration " << i);
        // attach pricing engines to helpers
        Currency eqCcy = eqParametrizations[i]->currency();
        Size eqCcyIdx = model_->ccyIndex(eqCcy);
        QuantLib::ext::shared_ptr<QuantExt::AnalyticXAssetLgmEquityOptionEngine> engine =
            QuantLib::ext::make_shared<QuantExt::AnalyticXAssetLgmEquityOptionEngine>(*model_, i, eqCcyIdx);
        for (Size j = 0; j < eqOptionBaskets_[i].size(); j++)
            eqOptionBaskets_[i][j]->setPricingEngine(engine);

        if (!dontCalibrate_) {

            // reset to initial params to ensure identical calibration outcomes for identical baskets
            resetModelParams(CrossAssetModel::AssetType::EQ, 0, i, Null<Size>());

            if (eq->calibrationType() == CalibrationType::Bootstrap && eq->sigmaParamType() == ParamType::Piecewise)
                model_->calibrateBsVolatilitiesIterative(CrossAssetModel::AssetType::EQ, i, eqOptionBaskets_[i],
                                                         *optimizationMethod_, endCriteria_);
            else
                model_->calibrateBsVolatilitiesGlobal(CrossAssetModel::AssetType::EQ, i, eqOptionBaskets_[i],
                                                      *optimizationMethod_, endCriteria_);
            DLOG("EQ " << eq->eqName() << " calibration errors:");
            eqOptionCalibrationErrors_[i] = getCalibrationError(eqOptionBaskets_[i]);
            if (eq->calibrationType() == CalibrationType::Bootstrap) {
                if (fabs(eqOptionCalibrationErrors_[i]) < config_->bootstrapTolerance()) {
                    TLOGGERSTREAM("Calibration details:");
                    TLOGGERSTREAM(
                        getCalibrationDetails(eqOptionBaskets_[i], eqParametrizations[i], irParametrizations[0]));
                    TLOGGERSTREAM("rmse = " << eqOptionCalibrationErrors_[i]);
                } else {
                    std::string exceptionMessage = "EQ BS " + std::to_string(i) + " calibration error " +
                                                   std::to_string(eqOptionCalibrationErrors_[i]) +
                                                   " exceeds tolerance " +
                                                   std::to_string(config_->bootstrapTolerance());
                    StructuredModelWarningMessage("Failed to calibrate EQ BS Model", exceptionMessage, id_).log();
                    WLOGGERSTREAM("Calibration details:");
                    WLOGGERSTREAM(
                        getCalibrationDetails(eqOptionBaskets_[i], eqParametrizations[i], irParametrizations[0]));
                    WLOGGERSTREAM("rmse = " << eqOptionCalibrationErrors_[i]);
                    if (!continueOnError_)
                        QL_FAIL(exceptionMessage);
                }
            }
        }
        eqBuilder[i]->setCalibrationDone();
    }

    /*************************
     * Calibrate COM components
     */

    for (Size i = 0; i < csBuilder.size(); i++) {
        DLOG("COM Calibration " << i);
        comOptionCalibrationErrors_[i] = csBuilder[i]->error();
    }

    /*************************
     * Relink LGM discount curves to curves used for INF calibration
     */

    for (Size i = 0; i < irParametrizations.size(); i++) {
        auto p = irParametrizations[i];
        irDiscountCurves[i].linkTo(*market_->discountCurve(p->currency().code(), configurationInfCalibration_));
        DLOG("Relinked discounting curve for " << p->currency().code() << " for INF calibration");
    }

    // Calibrate INF components
    for (Size i = 0; i < infParameterizations.size(); i++) {
        QuantLib::ext::shared_ptr<InflationModelData> imData = config_->infConfigs()[i];
        if (auto dkData = QuantLib::ext::dynamic_pointer_cast<InfDkData>(imData)) {
            auto dkParam = QuantLib::ext::dynamic_pointer_cast<InfDkParametrization>(infParameterizations[i]);
            QL_REQUIRE(dkParam, "Expected DK model data to have given a DK parameterisation.");
            const auto& builder = subBuilders_.at(CrossAssetModel::AssetType::INF).at(i);
            const auto& dkBuilder = QuantLib::ext::dynamic_pointer_cast<InfDkBuilder>(builder);
            if (!dkBuilder->requiresRecalibration() &&
                recalibratedCurrencies.find(infParameterizations[i]->currency().code()) ==
                    recalibratedCurrencies.end()) {
                DLOG("Skipping inf dk calibration "
                     << i << " since neither inf builder nor ir model in inf ccy were recalibrated.");
                continue;
            }
            calibrateInflation(*dkData, i, dkBuilder->optionBasket(), dkParam);
            dkBuilder->setCalibrationDone();
        } else if (auto jyData = QuantLib::ext::dynamic_pointer_cast<InfJyData>(imData)) {
            auto jyParam = QuantLib::ext::dynamic_pointer_cast<InfJyParameterization>(infParameterizations[i]);
            QL_REQUIRE(jyParam, "Expected JY model data to have given a JY parameterisation.");
            const auto& builder = subBuilders_.at(CrossAssetModel::AssetType::INF).at(i);
            const auto& jyBuilder = QuantLib::ext::dynamic_pointer_cast<InfJyBuilder>(builder);
            if (!jyBuilder->requiresRecalibration() &&
                recalibratedCurrencies.find(infParameterizations[i]->currency().code()) ==
                    recalibratedCurrencies.end()) {
                DLOG("Skipping inf jy calibration "
                     << i << " since neither inf builder nor ir model in inf ccy were recalibrated.");
                continue;
            }
            calibrateInflation(*jyData, i, jyBuilder, jyParam);
            jyBuilder->setCalibrationDone();
        } else {
            QL_FAIL("CrossAssetModelBuilder expects either DK or JY inflation model data.");
        }
    }

    /*************************
     * Relink LGM discount curves to final model curves
     */

    for (Size i = 0; i < irParametrizations.size(); i++) {
        auto p = irParametrizations[i];
        irDiscountCurves[i].linkTo(*market_->discountCurve(p->currency().code(), configurationFinalModel_));
        DLOG("Relinked discounting curve for " << p->currency().code() << " as final model curves");
    }

    DLOG("Building CrossAssetModel done");
}

void CrossAssetModelBuilder::forceRecalculate() {
    forceCalibration_ = true;
    ModelBuilder::forceRecalculate();
    forceCalibration_ = false;
}

void CrossAssetModelBuilder::calibrateInflation(const InfDkData& data, Size modelIdx,
                                                const vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>>& cb,
                                                const QuantLib::ext::shared_ptr<InfDkParametrization>& inflationParam) const {

    LOG("Calibrate DK inflation model for inflation index " << data.index());

    if ((!data.volatility().calibrate() && !data.reversion().calibrate()) ||
        (data.calibrationType() == CalibrationType::None)) {
        LOG("Calibration of DK inflation model for inflation index " << data.index() << " not requested.");
        return;
    }

    Handle<ZeroInflationIndex> zInfIndex =
        market_->zeroInflationIndex(model_->infdk(modelIdx)->name(), configurationInfCalibration_);
    Real baseCPI = dontCalibrate_ ? 100. : zInfIndex->fixing(zInfIndex->zeroInflationTermStructure()->baseDate());
    auto engine = QuantLib::ext::make_shared<QuantExt::AnalyticDkCpiCapFloorEngine>(*model_, modelIdx, baseCPI);
    for (Size j = 0; j < cb.size(); j++)
        cb[j]->setPricingEngine(engine);

    if (dontCalibrate_)
        return;

    if (data.volatility().calibrate() && !data.reversion().calibrate()) {
        // reset to initial params to ensure identical calibration outcomes for identical baskets
        resetModelParams(CrossAssetModel::AssetType::INF, 0, modelIdx, Null<Size>());
        if (data.calibrationType() == CalibrationType::Bootstrap && data.volatility().type() == ParamType::Piecewise) {
            model_->calibrateInfDkVolatilitiesIterative(modelIdx, cb, *optimizationMethod_, endCriteria_);
        } else {
            model_->calibrateInfDkVolatilitiesGlobal(modelIdx, cb, *optimizationMethod_, endCriteria_);
        }
    } else if (!data.volatility().calibrate() && data.reversion().calibrate()) {
        // reset to initial params to ensure identical calibration outcomes for identical baskets
        resetModelParams(CrossAssetModel::AssetType::INF, 1, modelIdx, Null<Size>());
        if (data.calibrationType() == CalibrationType::Bootstrap && data.reversion().type() == ParamType::Piecewise) {
            model_->calibrateInfDkReversionsIterative(modelIdx, cb, *optimizationMethod_, endCriteria_);
        } else {
            model_->calibrateInfDkReversionsGlobal(modelIdx, cb, *optimizationMethod_, endCriteria_);
        }
    } else {
        model_->calibrate(cb, *optimizationMethod_, endCriteria_);
    }

    DLOG("INF (DK) " << data.index() << " calibration errors:");
    inflationCalibrationErrors_[modelIdx] = getCalibrationError(cb);
    if (data.calibrationType() == CalibrationType::Bootstrap) {
        if (fabs(inflationCalibrationErrors_[modelIdx]) < config_->bootstrapTolerance()) {
            TLOGGERSTREAM("Calibration details:");
            TLOGGERSTREAM(getCalibrationDetails(cb, inflationParam, false));
            TLOGGERSTREAM("rmse = " << inflationCalibrationErrors_[modelIdx]);
        } else {
            string exceptionMessage = "INF (DK) " + std::to_string(modelIdx) + " calibration error " +
                                      std::to_string(inflationCalibrationErrors_[modelIdx]) + " exceeds tolerance " +
                                      std::to_string(config_->bootstrapTolerance());
            StructuredModelWarningMessage("Failed to calibrate INF DK Model", exceptionMessage, id_).log();
            WLOGGERSTREAM("Calibration details:");
            WLOGGERSTREAM(getCalibrationDetails(cb, inflationParam, false));
            WLOGGERSTREAM("rmse = " << inflationCalibrationErrors_[modelIdx]);
            if (!continueOnError_)
                QL_FAIL(exceptionMessage);
        }
    }
}

void CrossAssetModelBuilder::calibrateInflation(const InfJyData& data, Size modelIdx,
                                                const QuantLib::ext::shared_ptr<InfJyBuilder>& jyBuilder,
                                                const QuantLib::ext::shared_ptr<InfJyParameterization>& inflationParam) const {

    LOG("Calibrate JY inflation model for inflation index " << data.index());

    const auto& rrVol = data.realRateVolatility();
    const auto& rrRev = data.realRateReversion();
    const auto& idxVol = data.indexVolatility();

    // Check if calibration is needed at all.
    if ((!rrVol.calibrate() && !rrRev.calibrate() && !idxVol.calibrate()) ||
        (data.calibrationType() == CalibrationType::None)) {
        LOG("Calibration of JY inflation model for inflation index " << data.index() << " not requested.");
        return;
    }

    Handle<ZeroInflationIndex> zInfIndex =
        market_->zeroInflationIndex(model_->infjy(modelIdx)->name(), configurationInfCalibration_);

    // We will need the 2 baskets of helpers
    auto rrBasket = jyBuilder->realRateBasket();
    auto idxBasket = jyBuilder->indexBasket();

    // Attach engines to the helpers.
    setJyPricingEngine(modelIdx, rrBasket, false);
    setJyPricingEngine(modelIdx, idxBasket, false);

    if (dontCalibrate_)
        return;

    // Single basket of helpers is useful in various places below.
    vector<QuantLib::ext::shared_ptr<CalibrationHelper>> allHelpers = rrBasket;
    allHelpers.insert(allHelpers.end(), idxBasket.begin(), idxBasket.end());

    // Calibration configuration.
    const auto& cc = data.calibrationConfiguration();

    // if we link the real rate params to the nominal rate params, we copy them over now (ir calibration is done at this point)
    if(data.linkRealRateParamsToNominalRateParams()) {
        Size irIdx = model_->ccyIndex(model_->infjy(modelIdx)->currency());
        copyModelParams(CrossAssetModel::AssetType::IR, 0, irIdx, Null<Size>(), CrossAssetModel::AssetType::INF, 0,
                        modelIdx, Null<Size>(), data.linkedRealRateVolatilityScaling());
        copyModelParams(CrossAssetModel::AssetType::IR, 1, irIdx, Null<Size>(), CrossAssetModel::AssetType::INF, 1,
                        modelIdx, Null<Size>(), 1.0);
    }

    if (data.calibrationType() == CalibrationType::BestFit) {

        // If calibration type is BestFit, do a global optimisation on the parameters that need to be calibrated.
        DLOG("Calibration BestFit of JY inflation model for inflation index " << data.index() << " requested.");

        // Indicate the parameters to calibrate
        map<Size, bool> toCalibrate;
        toCalibrate[0] = rrVol.calibrate();
        toCalibrate[1] = rrRev.calibrate();
        toCalibrate[2] = idxVol.calibrate();

        // Calibrate the model.
        resetModelParams(CrossAssetModel::AssetType::INF, 0, modelIdx, Null<Size>());
        resetModelParams(CrossAssetModel::AssetType::INF, 1, modelIdx, Null<Size>());
        resetModelParams(CrossAssetModel::AssetType::INF, 2, modelIdx, Null<Size>());
        model_->calibrateInfJyGlobal(modelIdx, allHelpers, *optimizationMethod_, endCriteria_, toCalibrate);

    } else {

        // Calibration type is now Bootstrap, there are multiple options.
        QL_REQUIRE(data.calibrationType() == CalibrationType::Bootstrap,
                   "JY inflation calibration expected a "
                       << "calibration type of None, BestFit or Bootstrap.");
        QL_REQUIRE(!(rrRev.calibrate() && rrVol.calibrate()),
                   "Calibrating both the "
                       << "real rate reversion and real rate volatility using Bootstrap is not supported.");

        if ((!rrVol.calibrate() && !rrRev.calibrate()) && idxVol.calibrate()) {

            // Bootstrap the inflation index volatility only.
            DLOG("Bootstrap calibration of JY index volatility for index " << data.index() << ".");
            QL_REQUIRE(idxVol.type() == ParamType::Piecewise, "Index volatility parameter should be Piecewise for "
                                                                  << "a Bootstrap calibration.");
            // reset to initial params to ensure identical calibration outcomes for identical baskets
            resetModelParams(CrossAssetModel::AssetType::INF, 2, modelIdx, Null<Size>());
            model_->calibrateInfJyIterative(modelIdx, 2, idxBasket, *optimizationMethod_, endCriteria_);

        } else if (rrVol.calibrate() && !idxVol.calibrate()) {

            // Bootstrap the real rate volatility only
            DLOG("Bootstrap calibration of JY real rate volatility for index " << data.index() << ".");
            QL_REQUIRE(rrVol.type() == ParamType::Piecewise, "Real rate volatility parameter should be "
                                                                 << "Piecewise for a Bootstrap calibration.");
            // reset to initial params to ensure identical calibration outcomes for identical baskets
            resetModelParams(CrossAssetModel::AssetType::INF, 0, modelIdx, Null<Size>());
            model_->calibrateInfJyIterative(modelIdx, 0, rrBasket, *optimizationMethod_, endCriteria_);

        } else if (rrRev.calibrate() && !idxVol.calibrate()) {

            // Bootstrap the real rate reversion only
            DLOG("Bootstrap calibration of JY real rate reversion for index " << data.index() << ".");
            QL_REQUIRE(rrRev.type() == ParamType::Piecewise, "Real rate reversion parameter should be "
                                                                 << "Piecewise for a Bootstrap calibration.");
            // reset to initial params to ensure identical calibration outcomes for identical baskets
            resetModelParams(CrossAssetModel::AssetType::INF, 1, modelIdx, Null<Size>());
            model_->calibrateInfJyIterative(modelIdx, 1, rrBasket, *optimizationMethod_, endCriteria_);

        } else if ((rrVol.calibrate() && idxVol.calibrate()) || (rrRev.calibrate() && idxVol.calibrate())) {

            if (rrVol.calibrate()) {
                DLOG("Bootstrap calibration of JY real rate volatility and index volatility for index " << data.index()
                                                                                                        << ".");
            } else {
                DLOG("Bootstrap calibration of JY real rate reversion and index volatility for index " << data.index()
                                                                                                       << ".");
            }

            // Bootstrap the real rate volatility and the index volatility
            Size rrIdx = rrVol.calibrate() ? 0 : 1;
            Size numIts = 0;
            inflationCalibrationErrors_[modelIdx] = getCalibrationError(allHelpers);

            // reset to initial params to ensure identical calibration outcomes for identical baskets
            resetModelParams(CrossAssetModel::AssetType::INF, 2, modelIdx, Null<Size>());
            resetModelParams(CrossAssetModel::AssetType::INF, rrIdx, modelIdx, Null<Size>());

            while (inflationCalibrationErrors_[modelIdx] >
                       std::min(cc.rmseTolerance(), config_->bootstrapTolerance()) &&
                   numIts < cc.maxIterations()) {
                model_->calibrateInfJyIterative(modelIdx, 2, idxBasket, *optimizationMethod_, endCriteria_);
                model_->calibrateInfJyIterative(modelIdx, rrIdx, rrBasket, *optimizationMethod_, endCriteria_);
                numIts++;
                inflationCalibrationErrors_[modelIdx] = getCalibrationError(allHelpers);
            }

            DLOG("Bootstrap calibration of JY model stopped with number of iterations "
                 << numIts << " and rmse equal to " << std::scientific << std::setprecision(6)
                 << inflationCalibrationErrors_[modelIdx] << ".");

        } else {
            QL_FAIL("JY inflation bootstrap calibration does not support the combination of real rate volatility = "
                    << std::boolalpha << rrVol.calibrate() << ", real rate reversion = " << rrRev.calibrate() << " and "
                    << "index volatility = " << idxVol.calibrate() << ".");
        }
    }

    // Log the calibration details.
    TLOG("INF (JY) " << data.index() << " model parameters after calibration:");
    TLOG("Real    rate vol times   : " << inflationParam->parameterTimes(0));
    TLOG("Real    rate vol values  : " << inflationParam->parameterValues(0));
    TLOG("Real    rate rev times   : " << inflationParam->parameterTimes(1));
    TLOG("Real    rate rev values  : " << inflationParam->parameterValues(1));
    TLOG("R/N conversion   times   : " << inflationParam->parameterTimes(2));
    TLOG("R/N conversion   values  : " << inflationParam->parameterValues(2));
    DLOG("INF (JY) " << data.index() << " calibration errors:");
    inflationCalibrationErrors_[modelIdx] = getCalibrationError(allHelpers);
    if (data.calibrationType() == CalibrationType::Bootstrap) {
        if (fabs(inflationCalibrationErrors_[modelIdx]) < config_->bootstrapTolerance()) {
            TLOGGERSTREAM("Calibration details:");
            TLOGGERSTREAM(getCalibrationDetails(rrBasket, idxBasket, inflationParam, rrVol.calibrate()));
            TLOGGERSTREAM("rmse = " << inflationCalibrationErrors_[modelIdx]);
        } else {
            std::stringstream ss;
            ss << "INF (JY) " << modelIdx << " calibration error " << std::scientific
               << inflationCalibrationErrors_[modelIdx] << " exceeds tolerance " << config_->bootstrapTolerance();
            string exceptionMessage = ss.str();
            StructuredModelWarningMessage("Failed to calibrate INF JY Model", exceptionMessage, id_).log();
            WLOGGERSTREAM("Calibration details:");
            WLOGGERSTREAM(getCalibrationDetails(rrBasket, idxBasket, inflationParam, rrVol.calibrate()));
            WLOGGERSTREAM("rmse = " << inflationCalibrationErrors_[modelIdx]);
            if (!continueOnError_)
                QL_FAIL(exceptionMessage);
        }
    }

    LOG("Finished calibrating JY inflation model for inflation index " << data.index());
}

void CrossAssetModelBuilder::setJyPricingEngine(Size modelIdx,
                                                const vector<QuantLib::ext::shared_ptr<CalibrationHelper>>& calibrationBasket,
                                                bool indexIsInterpolated) const {

    DLOG("Start setting pricing engines on JY calibration instruments.");

    // JY supports three types of calibration helpers. Generally, all of the calibration instruments in a basket will
    // be of the same type but we support all three here.
    QuantLib::ext::shared_ptr<PricingEngine> cpiCapFloorEngine;
    QuantLib::ext::shared_ptr<PricingEngine> yoyCapFloorEngine;
    QuantLib::ext::shared_ptr<PricingEngine> yoySwapEngine;
    QuantLib::ext::shared_ptr<InflationCouponPricer> yoyCouponPricer;

    for (auto& ci : calibrationBasket) {

        if (QuantLib::ext::shared_ptr<CpiCapFloorHelper> h = QuantLib::ext::dynamic_pointer_cast<CpiCapFloorHelper>(ci)) {
            if (!cpiCapFloorEngine) {
                cpiCapFloorEngine = QuantLib::ext::make_shared<AnalyticJyCpiCapFloorEngine>(*model_, modelIdx);
            }
            h->setPricingEngine(cpiCapFloorEngine);
            continue;
        }

        if (QuantLib::ext::shared_ptr<YoYCapFloorHelper> h = QuantLib::ext::dynamic_pointer_cast<YoYCapFloorHelper>(ci)) {
            if (!yoyCapFloorEngine) {
                yoyCapFloorEngine =
                    QuantLib::ext::make_shared<AnalyticJyYoYCapFloorEngine>(*model_, modelIdx, indexIsInterpolated);
            }
            h->setPricingEngine(yoyCapFloorEngine);
            continue;
        }

        if (QuantLib::ext::shared_ptr<YoYSwapHelper> h = QuantLib::ext::dynamic_pointer_cast<YoYSwapHelper>(ci)) {
            // Here we need to attach the coupon pricer to all the YoY coupons and then the generic discounting swap
            // engine to the helper.
            if (!yoyCouponPricer) {
                yoyCouponPricer = QuantLib::ext::make_shared<JyYoYInflationCouponPricer>(*model_, modelIdx);

                Size irIdx = model_->ccyIndex(model_->infjy(modelIdx)->currency());
                auto yts = model_->irlgm1f(irIdx)->termStructure();
                yoySwapEngine = QuantLib::ext::make_shared<DiscountingSwapEngine>(yts);
            }

            const auto& yoyLeg = h->yoySwap()->yoyLeg();
            for (const auto& cf : yoyLeg) {
                if (auto yoyCoupon = QuantLib::ext::dynamic_pointer_cast<YoYInflationCoupon>(cf))
                    yoyCoupon->setPricer(yoyCouponPricer);
            }

            h->setPricingEngine(yoySwapEngine);
            continue;
        }

        QL_FAIL("Only CPI cap floors, YoY cap floors and YoY swaps are supported for JY calibration.");
    }

    DLOG("Finished setting pricing engines on JY calibration instruments.");
}

} // namespace data
} // namespace ore
