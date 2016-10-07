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

#include <ored/model/lgmbuilder.hpp>
#include <ored/model/fxbsbuilder.hpp>
#include <ored/model/crossassetmodelbuilder.hpp>
#include <ored/model/utilities.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/correlationmatrix.hpp>
#include <ored/utilities/log.hpp>

#include <qle/models/irlgm1fpiecewiseconstantparametrization.hpp>
#include <qle/models/irlgm1fpiecewiselinearparametrization.hpp>
#include <qle/models/irlgm1fconstantparametrization.hpp>
#include <qle/models/irlgm1fpiecewiseconstanthullwhiteadaptor.hpp>
#include <qle/models/fxbsconstantparametrization.hpp>
#include <qle/models/fxbspiecewiseconstantparametrization.hpp>
#include <qle/pricingengines/analyticlgmswaptionengine.hpp>
#include <qle/pricingengines/analyticcclgmfxoptionengine.hpp>
#include <qle/models/fxoptionhelper.hpp>

#include <ql/math/optimization/levenbergmarquardt.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/models/shortrate/calibrationhelpers/swaptionhelper.hpp>
#include <ql/utilities/dataformatters.hpp>

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/lexical_cast.hpp>

namespace ore {
namespace data {

CrossAssetModelBuilder::CrossAssetModelBuilder(const boost::shared_ptr<ore::data::Market>& market,
                                               const std::string& configurationLgmCalibration,
                                               const std::string& configurationFxCalibration,
                                               const std::string& configurationFinalModel, const DayCounter& dayCounter)
    : market_(market), configurationLgmCalibration_(configurationLgmCalibration),
      configurationFxCalibration_(configurationFxCalibration), configurationFinalModel_(configurationFinalModel),
      dayCounter_(dayCounter),
      optimizationMethod_(boost::shared_ptr<OptimizationMethod>(new LevenbergMarquardt(1E-8, 1E-8, 1E-8))),
      endCriteria_(EndCriteria(1000, 500, 1E-8, 1E-8, 1E-8)) {
    QL_REQUIRE(market != NULL, "CrossAssetModelBuilder: no market given");
}

boost::shared_ptr<QuantExt::CrossAssetModel>
CrossAssetModelBuilder::build(const boost::shared_ptr<CrossAssetModelData>& config) {

    LOG("Start building CrossAssetModel, configurations: LgmCalibration "
        << configurationLgmCalibration_ << ", FxCalibration " << configurationFxCalibration_ << ", FinalModel "
        << configurationFinalModel_);

    QL_REQUIRE(config->irConfigs().size() > 0, "missing IR configurations");
    QL_REQUIRE(config->irConfigs().size() == config->fxConfigs().size() + 1,
               "FX configuration size " << config->fxConfigs().size() << " inconsisitent with IR configuration size "
                                        << config->irConfigs().size());

    swaptionBaskets_.resize(config->irConfigs().size());
    swaptionExpiries_.resize(config->irConfigs().size());
    swaptionMaturities_.resize(config->irConfigs().size());
    swaptionCalibrationErrors_.resize(config->irConfigs().size());
    fxOptionBaskets_.resize(config->fxConfigs().size());
    fxOptionExpiries_.resize(config->fxConfigs().size());
    fxOptionCalibrationErrors_.resize(config->fxConfigs().size());

    /*******************************************************
     * Build the IR parametrizations and calibration baskets
     */
    std::vector<boost::shared_ptr<QuantExt::IrLgm1fParametrization>> irParametrizations;
    std::vector<RelinkableHandle<YieldTermStructure>> irDiscountCurves;
    std::vector<std::string> currencies;
    std::vector<boost::shared_ptr<LgmBuilder>> irBuilder;

    for (Size i = 0; i < config->irConfigs().size(); i++) {
        boost::shared_ptr<LgmData> ir = config->irConfigs()[i];
        LOG("IR Parametrization " << i << " ccy " << ir->ccy());
        boost::shared_ptr<LgmBuilder> builder =
            boost::make_shared<LgmBuilder>(market_, ir, configurationLgmCalibration_, config->bootstrapTolerance());
        irBuilder.push_back(builder);
        boost::shared_ptr<QuantExt::IrLgm1fParametrization> parametrization = builder->parametrization();
        swaptionBaskets_[i] = builder->swaptionBasket();
        currencies.push_back(ir->ccy());
        irParametrizations.push_back(parametrization);
        irDiscountCurves.push_back(builder->discountCurve());
    }

    QL_REQUIRE(irParametrizations.size() > 0, "missing IR parametrizations");

    QuantLib::Currency domesticCcy = irParametrizations[0]->currency();

    /*******************************************************
     * Build the FX parametrizations and calibration baskets
     */
    std::vector<boost::shared_ptr<QuantExt::FxBsParametrization>> fxParametrizations;
    for (Size i = 0; i < config->fxConfigs().size(); i++) {
        LOG("FX Parametrization " << i);
        boost::shared_ptr<FxBsData> fx = config->fxConfigs()[i];
        QuantLib::Currency ccy = ore::data::parseCurrency(fx->foreignCcy());
        QuantLib::Currency domCcy = ore::data::parseCurrency(fx->domesticCcy());

        QL_REQUIRE(ccy.code() == irParametrizations[i + 1]->currency().code(),
                   "FX parametrization currency[" << i << "]=" << ccy << " does not match IR currrency[" << i + 1
                                                  << "]=" << irParametrizations[i + 1]->currency().code());

        QL_REQUIRE(domCcy == domesticCcy, "FX parametrization [" << i << "]=" << ccy << "/" << domCcy
                                                                 << " does not match domestic ccy " << domesticCcy);

        boost::shared_ptr<FxBsBuilder> builder =
            boost::make_shared<FxBsBuilder>(market_, fx, configurationFxCalibration_);
        boost::shared_ptr<QuantExt::FxBsParametrization> parametrization = builder->parametrization();

        fxOptionBaskets_[i] = builder->optionBasket();

        fxParametrizations.push_back(parametrization);
    }

    std::vector<boost::shared_ptr<QuantExt::Parametrization>> parametrizations;
    for (Size i = 0; i < irParametrizations.size(); i++)
        parametrizations.push_back(irParametrizations[i]);
    for (Size i = 0; i < fxParametrizations.size(); i++)
        parametrizations.push_back(fxParametrizations[i]);

    QL_REQUIRE(fxParametrizations.size() == irParametrizations.size() - 1, "mismatch in IR/FX parametrization sizes");

    /******************************
     * Build the correlation matrix
     */

    ore::data::CorrelationMatrixBuilder cmb;
    for (auto it = config->correlations().begin(); it != config->correlations().end(); it++) {
        std::string factor1 = it->first.first;
        std::string factor2 = it->first.second;
        Real corr = it->second;
        LOG("Add correlation for " << factor1 << " " << factor2);
        cmb.addCorrelation(factor1, factor2, corr);
    }

    LOG("Get correlation matrix for currencies:");
    for (auto c : currencies)
        LOG("Currency " << c);

    Matrix corrMatrix = cmb.correlationMatrix(currencies);

    /*****************************
     * Build the cross asset model
     */

    boost::shared_ptr<QuantExt::CrossAssetModel> model =
        boost::make_shared<QuantExt::CrossAssetModel>(parametrizations, corrMatrix);

    /*************************
     * Calibrate IR components
     */

    for (Size i = 0; i < irBuilder.size(); i++) {
        LOG("IR Calibration " << i);
        irBuilder[i]->update();
        swaptionCalibrationErrors_[i] = irBuilder[i]->error();
    }

    /*************************
     * Relink LGM discount curves to curves used for FX calibration
     */

    for (Size i = 0; i < irParametrizations.size(); i++) {
        auto p = irParametrizations[i];
        irDiscountCurves[i].linkTo(*market_->discountCurve(p->currency().code(), configurationFxCalibration_));
        LOG("Relinked discounting curve for " << p->currency().code() << " for FX calibration");
    }

    /*************************
     * Calibrate FX components
     */

    for (Size i = 0; i < fxParametrizations.size(); i++) {
        boost::shared_ptr<FxBsData> fx = config->fxConfigs()[i];
        if (!fx->calibrateSigma()) {
            LOG("FX Calibration " << i << " skipped");
            continue;
        }

        LOG("FX Calibration " << i);

        // attach pricing engines to helpers
        boost::shared_ptr<QuantExt::AnalyticCcLgmFxOptionEngine> engine =
            boost::make_shared<QuantExt::AnalyticCcLgmFxOptionEngine>(model, i);
        // enable caching for calibration
        // TODO: review this
        engine->cache(true);
        for (Size j = 0; j < fxOptionBaskets_[i].size(); j++)
            fxOptionBaskets_[i][j]->setPricingEngine(engine);

        model->calibrateFxBsVolatilitiesIterative(i, fxOptionBaskets_[i], *optimizationMethod_, endCriteria_);

        LOG("FX " << fx->foreignCcy() << " calibration errors:");
        fxOptionCalibrationErrors_[i] =
            logCalibrationErrors(fxOptionBaskets_[i], fxParametrizations[i], irParametrizations[0]);
        if (fx->calibrationType() == CalibrationType::Bootstrap) {
            QL_REQUIRE(fabs(fxOptionCalibrationErrors_[i]) < config->bootstrapTolerance(),
                       "calibration error " << fxOptionCalibrationErrors_[i] << " exceeds tolerance "
                                            << config->bootstrapTolerance());
        }
    }

    /*************************
     * Relink LGM discount curves to final model curves
     */

    for (Size i = 0; i < irParametrizations.size(); i++) {
        auto p = irParametrizations[i];
        irDiscountCurves[i].linkTo(*market_->discountCurve(p->currency().code(), configurationFinalModel_));
        LOG("Relinked discounting curve for " << p->currency().code() << " as final model curves");
    }

    // play safe (although the cache of the model should be empty at
    // this point from all what we know...)
    model->update();

    LOG("Building CrossAssetModel done");

    return model;
}
}
}
