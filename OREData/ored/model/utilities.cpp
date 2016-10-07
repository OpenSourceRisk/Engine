/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of OpenRiskEngine, a free-software/open-source library
 for transparent pricing and risk analysis - http://openriskengine.org

 OpenRiskEngine is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program; if not, please email
 <users@openriskengine.org>. The license is also available online at
 <http://openriskengine.org/license.shtml>.

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

#include <ored/model/utilities.hpp>
#include <ored/utilities/log.hpp>

#include <qle/models/fxoptionhelper.hpp>

#include <ql/exercise.hpp>
#include <ql/models/shortrate/calibrationhelpers/swaptionhelper.hpp>

namespace openriskengine {
namespace data {

Real logCalibrationErrors(const std::vector<boost::shared_ptr<CalibrationHelper>>& basket,
                          const boost::shared_ptr<IrLgm1fParametrization>& parametrization) {
    LOG("# modelVol marketVol (diff) modelValue marketValue (diff) irlgm1fAlpha irlgm1fKappa");
    Real rmse = 0;
    Real t = 0.0, modelAlpha = 0.0, modelKappa = 0.0;
    for (Size j = 0; j < basket.size(); j++) {
        Real modelValue = basket[j]->modelValue();
        Real marketValue = basket[j]->marketValue();
        Real valueDiff = modelValue - marketValue;
        Volatility modelVol = 0, marketVol = 0, volDiff = 0;
        boost::shared_ptr<SwaptionHelper> swaption = boost::dynamic_pointer_cast<SwaptionHelper>(basket[j]);
        if (swaption != nullptr && parametrization != nullptr) {
            // report alpha, kappa at t_expiry^-
            t = parametrization->termStructure()->timeFromReference(swaption->swaption()->exercise()->date(0)) - 1E-4;
            modelAlpha = parametrization->alpha(t);
            modelKappa = parametrization->kappa(t);
        }
        // TODO handle other calibration helpers, too (capfloor)
        try {
            // TODO handle volatility type to switch the tolerance boundaries (available in QuantLib 1.9)
            marketVol = basket[j]->impliedVolatility(marketValue, 1e-4, 1000, 5e-10, 5.0);
        } catch (...) {
            LOG("error implying market vol for instrument " << j);
        }
        try {
            // TODO handle volatility type to switch the tolerance boundaries (available in QuantLib 1.9)
            modelVol = basket[j]->impliedVolatility(modelValue, 1e-4, 1000, 5e-10, 5.0);
            volDiff = modelVol - marketVol;
        } catch (...) {
            LOG("error implying model vol for instrument " << j);
        }
        rmse += volDiff * volDiff;
        LOG(std::setw(2) << j << "  " << std::setprecision(6) << modelVol << " " << marketVol << " (" << std::setw(8)
                         << volDiff << ")  " << modelValue << " " << marketValue << " (" << std::setw(8) << valueDiff
                         << ")  " << modelAlpha << " " << modelKappa);
    }
    if (parametrization != nullptr) {
        // report alpha, kappa at t_expiry^+ for last expiry
        t += 2 * 1E-4;
        modelAlpha = parametrization->alpha(t);
        modelKappa = parametrization->kappa(t);
    }
    LOG("t >= " << t << ": irlgm1fAlpha = " << modelAlpha << " irlgm1fKappa = " << modelKappa);
    rmse = sqrt(rmse / basket.size());
    LOG("rmse = " << rmse);
    return rmse;
}

Real logCalibrationErrors(const std::vector<boost::shared_ptr<CalibrationHelper>>& basket,
                          const boost::shared_ptr<FxBsParametrization>& parametrization,
                          const boost::shared_ptr<IrLgm1fParametrization>& domesticLgm) {
    LOG("# modelVol marketVol (diff) modelValue marketValue (diff) fxbsSigma");
    Real rmse = 0;
    Real t = 0.0, modelSigma = 0.0;
    for (Size j = 0; j < basket.size(); j++) {
        Real modelValue = basket[j]->modelValue();
        Real marketValue = basket[j]->marketValue();
        Real valueDiff = modelValue - marketValue;
        Volatility modelVol = 0, marketVol = 0, volDiff = 0;
        boost::shared_ptr<FxOptionHelper> fxoption = boost::dynamic_pointer_cast<FxOptionHelper>(basket[j]);
        if (fxoption != nullptr && parametrization != nullptr && domesticLgm != nullptr) {
            // report alpha, kappa at t_expiry^-
            t = domesticLgm->termStructure()->timeFromReference(fxoption->option()->exercise()->date(0)) - 1E-4;
            modelSigma = parametrization->sigma(t);
        }
        try {
            marketVol = basket[j]->impliedVolatility(marketValue, 1e-4, 1000, 5e-10, 0.5);
        } catch (...) {
            LOG("error implying market vol for instrument " << j);
        }
        try {
            modelVol = basket[j]->impliedVolatility(modelValue, 1e-4, 1000, 5e-10, 0.5);
            volDiff = modelVol - marketVol;
        } catch (...) {
            LOG("error implying model vol for instrument " << j);
        }
        rmse += volDiff * volDiff;
        LOG(std::setw(2) << j << "  " << std::setprecision(6) << modelVol << " " << marketVol << " (" << std::setw(8)
                         << volDiff << ")  " << modelValue << " " << marketValue << " (" << std::setw(8) << valueDiff
                         << ")  " << modelSigma);
    }
    if (parametrization != nullptr) {
        // report alpha, kappa at t_expiry^+ for last expiry
        t += 2 * 1E-4;
        modelSigma = parametrization->sigma(t);
    }
    LOG("t >= " << t << ": fxbsSigma = " << modelSigma);
    rmse = sqrt(rmse / basket.size());
    LOG("rmse = " << rmse);
    return rmse;
}
}
}
