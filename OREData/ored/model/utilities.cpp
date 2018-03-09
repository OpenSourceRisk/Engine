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

#include <ored/model/utilities.hpp>
#include <ored/utilities/log.hpp>

#include <qle/models/cpicapfloorhelper.hpp>
#include <qle/models/fxeqoptionhelper.hpp>

#include <ql/exercise.hpp>
#include <ql/models/shortrate/calibrationhelpers/swaptionhelper.hpp>

namespace ore {
namespace data {

Real logCalibrationErrors(const std::vector<boost::shared_ptr<CalibrationHelper>>& basket,
                          const boost::shared_ptr<IrLgm1fParametrization>& parametrization) {
    LOG("# time   modelVol marketVol (diff) modelValue marketValue (diff) irlgm1fAlpha irlgm1fKappa irlgm1fHwSigma");
    Real rmse = 0;
    Real t = 0.0, modelAlpha = 0.0, modelKappa = 0.0, modelHwSigma = 0.0;
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
            modelHwSigma = parametrization->hullWhiteSigma(t);
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
        LOG(std::setw(2) << j << "  " << std::setprecision(6) << t << " " << modelVol << " " << marketVol << " ("
                         << std::setw(8) << volDiff << ")  " << modelValue << " " << marketValue << " (" << std::setw(8)
                         << valueDiff << ")  " << modelAlpha << " " << modelKappa << " " << modelHwSigma);
    }
    if (parametrization != nullptr) {
        // report alpha, kappa at t_expiry^+ for last expiry
        t += 2 * 1E-4;
        modelAlpha = parametrization->alpha(t);
        modelKappa = parametrization->kappa(t);
        modelHwSigma = parametrization->hullWhiteSigma(t);
    }
    LOG("t >= " << t << ": irlgm1fAlpha = " << modelAlpha << " irlgm1fKappa = " << modelKappa
                << " irlgm1fHwSigma = " << modelHwSigma);
    rmse = sqrt(rmse / basket.size());
    LOG("rmse = " << rmse);
    return rmse;
}

Real logCalibrationErrors(const std::vector<boost::shared_ptr<CalibrationHelper>>& basket,
                          const boost::shared_ptr<FxBsParametrization>& parametrization,
                          const boost::shared_ptr<IrLgm1fParametrization>& domesticLgm) {
    LOG("# time    modelVol marketVol (diff) modelValue marketValue (diff) fxbsSigma");
    Real rmse = 0;
    Real t = 0.0, modelSigma = 0.0;
    for (Size j = 0; j < basket.size(); j++) {
        Real modelValue = basket[j]->modelValue();
        Real marketValue = basket[j]->marketValue();
        Real valueDiff = modelValue - marketValue;
        Volatility modelVol = 0, marketVol = 0, volDiff = 0;
        boost::shared_ptr<FxEqOptionHelper> fxoption = boost::dynamic_pointer_cast<FxEqOptionHelper>(basket[j]);
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
        LOG(std::setw(2) << j << " " << t << "  " << std::setprecision(6) << modelVol << " " << marketVol << " ("
                         << std::setw(8) << volDiff << ")  " << modelValue << " " << marketValue << " (" << std::setw(8)
                         << valueDiff << ")  " << modelSigma);
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

Real logCalibrationErrors(const std::vector<boost::shared_ptr<CalibrationHelper>>& basket,
                          const boost::shared_ptr<EqBsParametrization>& parametrization,
                          const boost::shared_ptr<IrLgm1fParametrization>& domesticLgm) {
    LOG("# modelVol marketVol (diff) modelValue marketValue (diff) eqbsSigma");
    Real rmse = 0;
    Real t = 0.0, modelSigma = 0.0;
    for (Size j = 0; j < basket.size(); j++) {
        Real modelValue = basket[j]->modelValue();
        Real marketValue = basket[j]->marketValue();
        Real valueDiff = modelValue - marketValue;
        Volatility modelVol = 0, marketVol = 0, volDiff = 0;
        boost::shared_ptr<FxEqOptionHelper> eqoption = boost::dynamic_pointer_cast<FxEqOptionHelper>(basket[j]);
        if (eqoption != nullptr && parametrization != nullptr && domesticLgm != nullptr) {
            t = domesticLgm->termStructure()->timeFromReference(eqoption->option()->exercise()->date(0)) - 1E-4;
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
        t += 2 * 1E-4;
        modelSigma = parametrization->sigma(t);
    }
    LOG("t >= " << t << ": eqbsSigma = " << modelSigma);
    rmse = sqrt(rmse / basket.size());
    LOG("rmse = " << rmse);
    return rmse;
}

Real logCalibrationErrors(const std::vector<boost::shared_ptr<CalibrationHelper>>& basket,
                          const boost::shared_ptr<InfDkParametrization>& parametrization,
                          const boost::shared_ptr<IrLgm1fParametrization>& domesticLgm) {
    LOG("# modelValue marketValue (diff) infdkAlpha infdkH");
    Real rmse = 0;
    Real t = 0.0, modelAlpha = 0.0, modelH = 0.0;
    for (Size j = 0; j < basket.size(); j++) {
        Real modelValue = basket[j]->modelValue();
        Real marketValue = basket[j]->marketValue();
        Real valueDiff = (modelValue - marketValue);
        boost::shared_ptr<CpiCapFloorHelper> instr = boost::dynamic_pointer_cast<CpiCapFloorHelper>(basket[j]);
        if (instr != nullptr && parametrization != nullptr && domesticLgm != nullptr) {
            // report alpha, H at t_expiry^-
            t = inflationYearFraction(
                    parametrization->termStructure()->frequency(),
                    parametrization->termStructure()->indexIsInterpolated(),
                    parametrization->termStructure()->dayCounter(), parametrization->termStructure()->baseDate(),
                    instr->instrument()->payDate() - parametrization->termStructure()->observationLag()) -
                1.0 / 250.0;
            modelAlpha = parametrization->alpha(t);
            modelH = parametrization->H(t);
        }
        // TODO handle other calibration helpers, too (capfloor)
        rmse += valueDiff * valueDiff;
        LOG(std::setw(2) << j << "  " << std::setprecision(6) << modelValue << " " << marketValue << " ("
                         << std::setw(8) << valueDiff << ")  " << modelAlpha << " " << modelH);
    }
    if (parametrization != nullptr) {
        // report alpha, kappa at t_expiry^+ for last expiry
        t += 2 * 1.0 / 500.0;
        modelAlpha = parametrization->alpha(t);
        modelH = parametrization->H(t);
    }
    LOG("t >= " << t << ": infDkAlpha = " << modelAlpha << " infDkH = " << modelH);
    rmse = sqrt(rmse / basket.size());
    ;
    LOG("rmse = " << rmse);
    return rmse;
}

} // namespace data
} // namespace ore
