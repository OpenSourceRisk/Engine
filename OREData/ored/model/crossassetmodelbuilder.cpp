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

#include <ored/model/crossassetmodelbuilder.hpp>
#include <ored/model/eqbsbuilder.hpp>
#include <ored/model/fxbsbuilder.hpp>
#include <ored/model/infdkbuilder.hpp>
#include <ored/model/lgmbuilder.hpp>
#include <ored/model/structuredmodelerror.hpp>
#include <ored/model/utilities.hpp>
#include <ored/utilities/correlationmatrix.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>

#include <qle/models/fxbsconstantparametrization.hpp>
#include <qle/models/fxbspiecewiseconstantparametrization.hpp>
#include <qle/models/fxeqoptionhelper.hpp>
#include <qle/models/irlgm1fconstantparametrization.hpp>
#include <qle/models/irlgm1fpiecewiseconstanthullwhiteadaptor.hpp>
#include <qle/models/irlgm1fpiecewiseconstantparametrization.hpp>
#include <qle/models/irlgm1fpiecewiselinearparametrization.hpp>
#include <qle/pricingengines/analyticcclgmfxoptionengine.hpp>
#include <qle/pricingengines/analyticdkcpicapfloorengine.hpp>
#include <qle/pricingengines/analyticlgmswaptionengine.hpp>
#include <qle/pricingengines/analyticxassetlgmeqoptionengine.hpp>

#include <ql/math/optimization/levenbergmarquardt.hpp>
#include <ql/models/shortrate/calibrationhelpers/swaptionhelper.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/utilities/dataformatters.hpp>

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/lexical_cast.hpp>

namespace ore {
namespace data {

CrossAssetModelBuilder::CrossAssetModelBuilder(
    const boost::shared_ptr<ore::data::Market>& market, const boost::shared_ptr<CrossAssetModelData>& config,
    const std::string& configurationLgmCalibration, const std::string& configurationFxCalibration,
    const std::string& configurationEqCalibration, const std::string& configurationInfCalibration,
    const std::string& configurationFinalModel, const DayCounter& dayCounter, const bool dontCalibrate,
    const bool continueOnError, const std::string& referenceCalibrationGrid)
    : market_(market), config_(config), configurationLgmCalibration_(configurationLgmCalibration),
      configurationFxCalibration_(configurationFxCalibration), configurationEqCalibration_(configurationEqCalibration),
      configurationInfCalibration_(configurationInfCalibration), configurationFinalModel_(configurationFinalModel),
      dayCounter_(dayCounter), dontCalibrate_(dontCalibrate), continueOnError_(continueOnError),
      referenceCalibrationGrid_(referenceCalibrationGrid),
      optimizationMethod_(boost::shared_ptr<OptimizationMethod>(new LevenbergMarquardt(1E-8, 1E-8, 1E-8))),
      endCriteria_(EndCriteria(1000, 500, 1E-8, 1E-8, 1E-8)) {
    buildModel();
    registerWithSubBuilders();
    // register market observer with correlations
    marketObserver_ = boost::make_shared<MarketObserver>();
    for (auto const& c : config->correlations())
        marketObserver_->addObservable(c.second);
    // reset market observer's updated flag
    marketObserver_->hasUpdated(true);
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
const std::vector<Real>& CrossAssetModelBuilder::infCapFloorCalibrationErrors() {
    calculate();
    return infCapFloorCalibrationErrors_;
}

void CrossAssetModelBuilder::unregisterWithSubBuilders() {
    for (auto b : subBuilders_)
        unregisterWith(b);
}

void CrossAssetModelBuilder::registerWithSubBuilders() {
    for (auto b : subBuilders_)
        registerWith(b);
}

bool CrossAssetModelBuilder::requiresRecalibration() const {
    for (auto const &b : subBuilders_)
        if (b->requiresRecalibration())
            return true;
    return marketObserver_->hasUpdated(false);
}

void CrossAssetModelBuilder::performCalculations() const {
    // if any of the sub models requires a recalibration, we rebuilt the model
    // TODO we could do this more selectively
    if (!dontCalibrate_ && requiresRecalibration()) {
        // reset market observer update flag
        marketObserver_->hasUpdated(true);
        // the cast is a bit ugly, but we pretty much know what we are doing here
        const_cast<CrossAssetModelBuilder*>(this)->unregisterWithSubBuilders();
        buildModel();
        const_cast<CrossAssetModelBuilder*>(this)->registerWithSubBuilders();
    }
}

void CrossAssetModelBuilder::buildModel() const {

    QL_REQUIRE(market_ != NULL, "CrossAssetModelBuilder: no market given");
    LOG("Start building CrossAssetModel");
    DLOG("configurations: LgmCalibration "
         << configurationLgmCalibration_ << ", FxCalibration " << configurationFxCalibration_ << ", EqCalibration "
         << configurationEqCalibration_ << ", InfCalibration " << configurationInfCalibration_ << ", FinalModel "
         << configurationFinalModel_);
    if (dontCalibrate_) {
        DLOG("Calibration of the model is disabled.");
    }

    QL_REQUIRE(config_->irConfigs().size() > 0, "missing IR configurations");
    QL_REQUIRE(config_->irConfigs().size() == config_->fxConfigs().size() + 1,
               "FX configuration size " << config_->fxConfigs().size() << " inconsisitent with IR configuration size "
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
    infCapFloorBaskets_.resize(config_->infConfigs().size());
    infCapFloorExpiries_.resize(config_->infConfigs().size());
    infCapFloorCalibrationErrors_.resize(config_->infConfigs().size());

    subBuilders_.clear();

    /*******************************************************
     * Build the IR parametrizations and calibration baskets
     */
    std::vector<boost::shared_ptr<QuantExt::IrLgm1fParametrization>> irParametrizations;
    std::vector<RelinkableHandle<YieldTermStructure>> irDiscountCurves;
    std::vector<std::string> currencies, regions, crNames, eqNames, infIndices;
    std::vector<boost::shared_ptr<LgmBuilder>> irBuilder;

    for (Size i = 0; i < config_->irConfigs().size(); i++) {
        boost::shared_ptr<IrLgmData> ir = config_->irConfigs()[i];
        DLOG("IR Parametrization " << i << " ccy " << ir->ccy());
        boost::shared_ptr<LgmBuilder> builder =
            boost::make_shared<LgmBuilder>(market_, ir, configurationLgmCalibration_, config_->bootstrapTolerance(),
                                           continueOnError_, referenceCalibrationGrid_);
        if (dontCalibrate_)
            builder->freeze();
        irBuilder.push_back(builder);
        boost::shared_ptr<QuantExt::IrLgm1fParametrization> parametrization = builder->parametrization();
        swaptionBaskets_[i] = builder->swaptionBasket();
        currencies.push_back(ir->ccy());
        irParametrizations.push_back(parametrization);
        irDiscountCurves.push_back(builder->discountCurve());
        subBuilders_.insert(builder);
    }

    QL_REQUIRE(irParametrizations.size() > 0, "missing IR parametrizations");

    QuantLib::Currency domesticCcy = irParametrizations[0]->currency();

    /*******************************************************
     * Build the FX parametrizations and calibration baskets
     */
    std::vector<boost::shared_ptr<QuantExt::FxBsParametrization>> fxParametrizations;
    for (Size i = 0; i < config_->fxConfigs().size(); i++) {
        DLOG("FX Parametrization " << i);
        boost::shared_ptr<FxBsData> fx = config_->fxConfigs()[i];
        QuantLib::Currency ccy = ore::data::parseCurrency(fx->foreignCcy());
        QuantLib::Currency domCcy = ore::data::parseCurrency(fx->domesticCcy());

        QL_REQUIRE(ccy.code() == irParametrizations[i + 1]->currency().code(),
                   "FX parametrization currency[" << i << "]=" << ccy << " does not match IR currrency[" << i + 1
                                                  << "]=" << irParametrizations[i + 1]->currency().code());

        QL_REQUIRE(domCcy == domesticCcy, "FX parametrization [" << i << "]=" << ccy << "/" << domCcy
                                                                 << " does not match domestic ccy " << domesticCcy);

        boost::shared_ptr<FxBsBuilder> builder =
            boost::make_shared<FxBsBuilder>(market_, fx, configurationFxCalibration_, referenceCalibrationGrid_);
        boost::shared_ptr<QuantExt::FxBsParametrization> parametrization = builder->parametrization();

        fxOptionBaskets_[i] = builder->optionBasket();
        fxParametrizations.push_back(parametrization);
        subBuilders_.insert(builder);
    }

    /*******************************************************
     * Build the EQ parametrizations and calibration baskets
     */
    std::vector<boost::shared_ptr<QuantExt::EqBsParametrization>> eqParametrizations;
    for (Size i = 0; i < config_->eqConfigs().size(); i++) {
        DLOG("EQ Parametrization " << i);
        boost::shared_ptr<EqBsData> eq = config_->eqConfigs()[i];
        string eqName = eq->eqName();
        QuantLib::Currency eqCcy = ore::data::parseCurrency(eq->currency());
        QL_REQUIRE(std::find(currencies.begin(), currencies.end(), eqCcy.code()) != currencies.end(),
                   "Currency (" << eqCcy << ") for equity " << eqName << " not covered by CrossAssetModelData");
        boost::shared_ptr<EqBsBuilder> builder = boost::make_shared<EqBsBuilder>(
            market_, eq, domesticCcy, configurationEqCalibration_, referenceCalibrationGrid_);
        boost::shared_ptr<QuantExt::EqBsParametrization> parametrization = builder->parametrization();
        eqOptionBaskets_[i] = builder->optionBasket();
        eqParametrizations.push_back(parametrization);
        eqNames.push_back(eqName);
        subBuilders_.insert(builder);
    }

    /*******************************************************
     * Build the INF parametrizations and calibration baskets
     */
    std::vector<boost::shared_ptr<QuantExt::InfDkParametrization>> infParametrizations;
    for (Size i = 0; i < config_->infConfigs().size(); i++) {
        DLOG("INF Parametrization " << i);
        boost::shared_ptr<InfDkData> inf = config_->infConfigs()[i];
        string infIndex = inf->infIndex();
        boost::shared_ptr<InfDkBuilder> builder =
            boost::make_shared<InfDkBuilder>(market_, inf, configurationInfCalibration_, referenceCalibrationGrid_);
        boost::shared_ptr<QuantExt::InfDkParametrization> parametrization = builder->parametrization();
        infCapFloorBaskets_[i] = builder->optionBasket();
        infParametrizations.push_back(parametrization);
        infIndices.push_back(infIndex);
        subBuilders_.insert(builder);
    }

    std::vector<boost::shared_ptr<QuantExt::Parametrization>> parametrizations;
    for (Size i = 0; i < irParametrizations.size(); i++)
        parametrizations.push_back(irParametrizations[i]);
    for (Size i = 0; i < fxParametrizations.size(); i++)
        parametrizations.push_back(fxParametrizations[i]);
    for (Size i = 0; i < eqParametrizations.size(); i++)
        parametrizations.push_back(eqParametrizations[i]);
    for (Size i = 0; i < infParametrizations.size(); i++)
        parametrizations.push_back(infParametrizations[i]);

    QL_REQUIRE(fxParametrizations.size() == irParametrizations.size() - 1, "mismatch in IR/FX parametrization sizes");

    /******************************
     * Build the correlation matrix
     */

    ore::data::CorrelationMatrixBuilder cmb;
    for (auto it = config_->correlations().begin(); it != config_->correlations().end(); it++) {
        std::string factor1 = it->first.first;
        std::string factor2 = it->first.second;
        Handle<Quote> corr = it->second;
        DLOG("Add correlation for " << factor1 << " " << factor2);
        cmb.addCorrelation(factor1, factor2, corr);
    }

    DLOG("Get correlation matrix for currencies:");
    for (auto c : currencies)
        DLOG("Currency " << c);

    Matrix corrMatrix = cmb.correlationMatrix(currencies, infIndices, crNames, eqNames);

    /*****************************
     * Build the cross asset model
     */

    model_.linkTo(boost::make_shared<QuantExt::CrossAssetModel>(parametrizations, corrMatrix));

    /*************************
     * Calibrate IR components
     */

    for (Size i = 0; i < irBuilder.size(); i++) {
        DLOG("IR Calibration " << i);
        swaptionCalibrationErrors_[i] = irBuilder[i]->error();
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
        boost::shared_ptr<FxBsData> fx = config_->fxConfigs()[i];

        if (fx->calibrationType() == CalibrationType::None || !fx->calibrateSigma()) {
            DLOG("FX Calibration " << i << " skipped");
            continue;
        }

        DLOG("FX Calibration " << i);

        // attach pricing engines to helpers
        boost::shared_ptr<QuantExt::AnalyticCcLgmFxOptionEngine> engine =
            boost::make_shared<QuantExt::AnalyticCcLgmFxOptionEngine>(*model_, i);
        // enable caching for calibration
        // TODO: review this
        engine->cache(true);
        for (Size j = 0; j < fxOptionBaskets_[i].size(); j++)
            fxOptionBaskets_[i][j]->setPricingEngine(engine);

        if (!dontCalibrate_) {

            if (fx->calibrationType() == CalibrationType::Bootstrap && fx->sigmaParamType() == ParamType::Piecewise)
                model_->calibrateBsVolatilitiesIterative(CrossAssetModelTypes::FX, i, fxOptionBaskets_[i],
                                                         *optimizationMethod_, endCriteria_);
            else
                model_->calibrateBsVolatilitiesGlobal(CrossAssetModelTypes::FX, i, fxOptionBaskets_[i],
                                                      *optimizationMethod_, endCriteria_);

            DLOG("FX " << fx->foreignCcy() << " calibration errors:");
            fxOptionCalibrationErrors_[i] = getCalibrationError(fxOptionBaskets_[i]);
            if (fx->calibrationType() == CalibrationType::Bootstrap) {
                if (fabs(fxOptionCalibrationErrors_[i]) < config_->bootstrapTolerance()) {
                    // we check the log level here to avoid unncessary computations
                    if(Log::instance().filter(ORE_DATA)) {
                        TLOGGERSTREAM << "Calibration details:";
                        TLOGGERSTREAM << getCalibrationDetails(fxOptionBaskets_[i], fxParametrizations[i],
                                                               irParametrizations[0]);
                        TLOGGERSTREAM << "rmse = " << fxOptionCalibrationErrors_[i];
                    }
                } else {
                    std::string exceptionMessage = "FX BS " + std::to_string(i) + " calibration error " +
                                                   std::to_string(fxOptionCalibrationErrors_[i]) +
                                                   " exceeds tolerance " +
                                                   std::to_string(config_->bootstrapTolerance());
                    WLOG(StructuredModelErrorMessage("Failed to calibrate FX BS Model", exceptionMessage));
                    WLOGGERSTREAM << "Calibration details:";
                    WLOGGERSTREAM << getCalibrationDetails(fxOptionBaskets_[i], fxParametrizations[i],
                                                           irParametrizations[0]);
                    WLOGGERSTREAM << "rmse = " << fxOptionCalibrationErrors_[i];
                    if(!continueOnError_)
                        QL_FAIL(exceptionMessage);
                }
            }
        }
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
        boost::shared_ptr<EqBsData> eq = config_->eqConfigs()[i];
        if (!eq->calibrateSigma()) {
            DLOG("EQ Calibration " << i << " skipped");
            continue;
        }
        DLOG("EQ Calibration " << i);
        // attach pricing engines to helpers
        Currency eqCcy = eqParametrizations[i]->currency();
        Size eqCcyIdx = model_->ccyIndex(eqCcy);
        boost::shared_ptr<QuantExt::AnalyticXAssetLgmEquityOptionEngine> engine =
            boost::make_shared<QuantExt::AnalyticXAssetLgmEquityOptionEngine>(*model_, i, eqCcyIdx);
        for (Size j = 0; j < eqOptionBaskets_[i].size(); j++)
            eqOptionBaskets_[i][j]->setPricingEngine(engine);

        if (!dontCalibrate_) {

            if (eq->calibrationType() == CalibrationType::Bootstrap && eq->sigmaParamType() == ParamType::Piecewise)
                model_->calibrateBsVolatilitiesIterative(CrossAssetModelTypes::EQ, i, eqOptionBaskets_[i],
                                                         *optimizationMethod_, endCriteria_);
            else
                model_->calibrateBsVolatilitiesGlobal(CrossAssetModelTypes::EQ, i, eqOptionBaskets_[i],
                                                      *optimizationMethod_, endCriteria_);
            DLOG("EQ " << eq->eqName() << " calibration errors:");
            eqOptionCalibrationErrors_[i] = getCalibrationError(eqOptionBaskets_[i]);
            if (eq->calibrationType() == CalibrationType::Bootstrap) {
                if (fabs(eqOptionCalibrationErrors_[i]) < config_->bootstrapTolerance()) {
                    // we check the log level here to avoid unncessary computations
                    if (Log::instance().filter(ORE_DATA)) {
                        TLOGGERSTREAM << "Calibration details:";
                        TLOGGERSTREAM << getCalibrationDetails(eqOptionBaskets_[i], eqParametrizations[i],
                                                               irParametrizations[0]);
                        TLOGGERSTREAM << "rmse = " << eqOptionCalibrationErrors_[i];
                    }
                } else {
                    std::string exceptionMessage = "EQ BS " + std::to_string(i) + " calibration error " +
                                                   std::to_string(eqOptionCalibrationErrors_[i]) +
                                                   " exceeds tolerance " +
                                                   std::to_string(config_->bootstrapTolerance());
                    WLOG(StructuredModelErrorMessage("Failed to calibrate EQ BS Model", exceptionMessage));
                    WLOGGERSTREAM << "Calibration details:";
                    WLOGGERSTREAM << getCalibrationDetails(eqOptionBaskets_[i], eqParametrizations[i],
                                                           irParametrizations[0]);
                    WLOGGERSTREAM << "rmse = " << eqOptionCalibrationErrors_[i];
                    if (!continueOnError_)
                        QL_FAIL(exceptionMessage);
                }
            }
        }
    }

    /*************************
     * Relink LGM discount curves to curves used for INF calibration
     */

    for (Size i = 0; i < irParametrizations.size(); i++) {
        auto p = irParametrizations[i];
        irDiscountCurves[i].linkTo(*market_->discountCurve(p->currency().code(), configurationFinalModel_));
        DLOG("Relinked discounting curve for " << p->currency().code() << " as final model curves");
    }

    /*************************
    * Calibrate INF components

    */

    for (Size i = 0; i < infParametrizations.size(); i++) {
        boost::shared_ptr<InfDkData> inf = config_->infConfigs()[i];
        if ((!inf->calibrateA() && !inf->calibrateH()) || (inf->calibrationType() == CalibrationType::None)) {
            DLOG("INF Calibration " << i << " skipped");
            continue;
        }
        DLOG("INF Calibration " << i);
        // attach pricing engines to helpers

        Handle<ZeroInflationIndex> zInfIndex =
            market_->zeroInflationIndex(model_->infdk(i)->name(), configurationInfCalibration_);
        Real baseCPI = zInfIndex->fixing(zInfIndex->zeroInflationTermStructure()->baseDate());

        boost::shared_ptr<QuantExt::AnalyticDkCpiCapFloorEngine> engine =
            boost::make_shared<QuantExt::AnalyticDkCpiCapFloorEngine>(*model_, i, baseCPI);
        for (Size j = 0; j < infCapFloorBaskets_[i].size(); j++)
            infCapFloorBaskets_[i][j]->setPricingEngine(engine);

        if (!dontCalibrate_) {

            if (inf->calibrateA() && !inf->calibrateH()) {
                if (inf->calibrationType() == CalibrationType::Bootstrap && inf->aParamType() == ParamType::Piecewise) {
                    model_->calibrateInfDkVolatilitiesIterative(i, infCapFloorBaskets_[i], *optimizationMethod_,
                                                                endCriteria_);
                } else {
                    model_->calibrateInfDkVolatilitiesGlobal(i, infCapFloorBaskets_[i], *optimizationMethod_,
                                                             endCriteria_);
                }
            } else if (!inf->calibrateA() && inf->calibrateH()) {
                if (inf->calibrationType() == CalibrationType::Bootstrap && inf->hParamType() == ParamType::Piecewise) {
                    model_->calibrateInfDkReversionsIterative(i, infCapFloorBaskets_[i], *optimizationMethod_,
                                                              endCriteria_);
                } else {
                    model_->calibrateInfDkReversionsGlobal(i, infCapFloorBaskets_[i], *optimizationMethod_,
                                                           endCriteria_);
                }
            } else {
                model_->calibrate(infCapFloorBaskets_[i], *optimizationMethod_, endCriteria_);
            }

            DLOG("INF " << inf->infIndex() << " calibration errors:");
            infCapFloorCalibrationErrors_[i] = getCalibrationError(infCapFloorBaskets_[i]);
            if (inf->calibrationType() == CalibrationType::Bootstrap) {
                if (fabs(infCapFloorCalibrationErrors_[i]) < config_->bootstrapTolerance()) {
                    // we check the log level here to avoid unncessary computations
                    if(Log::instance().filter(ORE_DATA)) {
                        TLOGGERSTREAM << "Calibration details:";
                        TLOGGERSTREAM << getCalibrationDetails(infCapFloorBaskets_[i], infParametrizations[i],
                                                               irParametrizations[0]);
                        TLOGGERSTREAM << "rmse = " << infCapFloorCalibrationErrors_[i];
                    }
                } else {
                    std::string exceptionMessage = "INF DK " + std::to_string(i) + " calibration error " +
                                                   std::to_string(infCapFloorCalibrationErrors_[i]) +
                                                   " exceeds tolerance " +
                                                   std::to_string(config_->bootstrapTolerance());
                    WLOG(StructuredModelErrorMessage("Failed to calibrate INF DK Model", exceptionMessage));
                    WLOGGERSTREAM << "Calibration details:";
                    WLOGGERSTREAM << getCalibrationDetails(infCapFloorBaskets_[i], infParametrizations[i],
                                                           irParametrizations[0]);
                    WLOGGERSTREAM << "rmse = " << infCapFloorCalibrationErrors_[i];
                    if(!continueOnError_)
                        QL_FAIL(exceptionMessage);
                }
            }
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

    // play safe (although the cache of the model should be empty at
    // this point from all what we know...)
    model_->update();

    DLOG("Building CrossAssetModel done");
}

void CrossAssetModelBuilder::forceRecalculate() {
    forceCalibration_ = true;
    ModelBuilder::forceRecalculate();
    forceCalibration_ = false;
}

} // namespace data
} // namespace ore
