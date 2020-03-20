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

Real getCalibrationError(const std::vector<boost::shared_ptr<BlackCalibrationHelper>>& basket) {
    Real rmse = 0.0;
    for (auto const& h : basket) {
        Real tmp = h->calibrationError();
        rmse += tmp * tmp;
    }
    return std::sqrt(rmse / static_cast<Real>(basket.size()));
}

namespace {
Real impliedVolatility(const boost::shared_ptr<BlackCalibrationHelper>& h) {
    try {
        Real minVol, maxVol;
        if (h->volatilityType() == QuantLib::ShiftedLognormal) {
            minVol = 1.0e-7;
            maxVol = 4.0;
        } else {
            minVol = 1.0e-7;
            maxVol = 0.05;
        }
        return h->impliedVolatility(h->modelValue(), 1e-4, 1000, minVol, maxVol);
    } catch (...) {
        // vol could not be implied, return 0.0
        return 0.0;
    }
}
} // namespace

std::string getCalibrationDetails(const std::vector<boost::shared_ptr<BlackCalibrationHelper>>& basket,
                                  const boost::shared_ptr<IrLgm1fParametrization>& parametrization) {
    std::ostringstream log;
    log << std::right << std::setw(3) << "#" << std::setw(14) << "time" << std::setw(14) << "modelVol" << std::setw(14)
        << "marketVol" << std::setw(14) << "(diff)" << std::setw(14) << "modelValue" << std::setw(14) << "marketValue"
        << std::setw(14) << "(diff)" << std::setw(14) << "irlgm1fAlpha" << std::setw(14) << "irlgm1fKappa"
        << std::setw(16) << "irlgm1fHwSigma\n";
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
        marketVol = basket[j]->volatility()->value();
        modelVol = impliedVolatility(basket[j]);
        volDiff = modelVol - marketVol;
        log << std::setw(3) << j << std::setprecision(6) << std::setw(14) << t << std::setw(14) << modelVol
            << std::setw(14) << marketVol << std::setw(14) << volDiff << std::setw(14) << modelValue << std::setw(14)
            << marketValue << std::setw(14) << valueDiff << std::setw(14) << modelAlpha << std::setw(14) << modelKappa
            << std::setw(16) << modelHwSigma << "\n";
    }
    if (parametrization != nullptr) {
        // report alpha, kappa at t_expiry^+ for last expiry
        t += 2 * 1E-4;
        modelAlpha = parametrization->alpha(t);
        modelKappa = parametrization->kappa(t);
        modelHwSigma = parametrization->hullWhiteSigma(t);
    }
    log << "t >= " << t << ": irlgm1fAlpha = " << modelAlpha << " irlgm1fKappa = " << modelKappa
        << " irlgm1fHwSigma = " << modelHwSigma << "\n";
    return log.str();
}

std::string getCalibrationDetails(const std::vector<boost::shared_ptr<BlackCalibrationHelper>>& basket,
                                  const boost::shared_ptr<FxBsParametrization>& parametrization,
                                  const boost::shared_ptr<IrLgm1fParametrization>& domesticLgm) {
    std::ostringstream log;
    log << std::right << std::setw(3) << "#" << std::setw(14) << "time" << std::setw(14) << "modelVol" << std::setw(14)
        << "marketVol" << std::setw(14) << "(diff)" << std::setw(14) << "modelValue" << std::setw(14) << "marketValue"
        << std::setw(14) << "(diff)" << std::setw(14) << "fxbsSigma\n";
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
        marketVol = basket[j]->volatility()->value();
        modelVol = impliedVolatility(basket[j]);
        volDiff = modelVol - marketVol;
        log << std::setw(3) << j << std::setprecision(6) << std::setw(14) << t << std::setw(14) << modelVol
            << std::setw(14) << marketVol << std::setw(14) << volDiff << std::setw(14) << modelValue << std::setw(14)
            << marketValue << std::setw(14) << valueDiff << std::setw(14) << modelSigma << "\n";
    }
    if (parametrization != nullptr) {
        // report alpha, kappa at t_expiry^+ for last expiry
        t += 2 * 1E-4;
        modelSigma = parametrization->sigma(t);
    }
    log << "t >= " << t << ": fxbsSigma = " << modelSigma << "\n";
    return log.str();
}

std::string getCalibrationDetails(const std::vector<boost::shared_ptr<BlackCalibrationHelper>>& basket,
                                  const boost::shared_ptr<EqBsParametrization>& parametrization,
                                  const boost::shared_ptr<IrLgm1fParametrization>& domesticLgm) {
    std::ostringstream log;
    log << std::right << std::setw(3) << "#" << std::setw(14) << "time" << std::setw(14) << "modelVol" << std::setw(14)
        << "marketVol" << std::setw(14) << "(diff)" << std::setw(14) << "modelValue" << std::setw(14) << "marketValue"
        << std::setw(14) << "(diff)" << std::setw(14) << "eqbsSigma\n";
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
        marketVol = basket[j]->volatility()->value();
        modelVol = impliedVolatility(basket[j]);
        volDiff = modelVol - marketVol;
        log << std::setw(3) << j << std::setprecision(6) << std::setw(14) << t << std::setw(14) << modelVol
            << std::setw(14) << marketVol << std::setw(14) << volDiff << std::setw(14) << modelValue << std::setw(14)
            << marketValue << std::setw(14) << valueDiff << std::setw(14) << modelSigma << "\n";
    }
    if (parametrization != nullptr) {
        t += 2 * 1E-4;
        modelSigma = parametrization->sigma(t);
    }
    log << "t >= " << t << ": eqbsSigma = " << modelSigma << "\n";
    return log.str();
}

std::string getCalibrationDetails(const std::vector<boost::shared_ptr<BlackCalibrationHelper>>& basket,
                                  const boost::shared_ptr<InfDkParametrization>& parametrization,
                                  const boost::shared_ptr<IrLgm1fParametrization>& domesticLgm) {
    std::ostringstream log;
    log << std::right << std::setw(3) << "#" << std::setw(14) << "modelValue" << std::setw(14) << "marketValue"
        << std::setw(14) << "(diff)" << std::setw(14) << "infdkAlpha" << std::setw(14) << "infdkH\n";
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
        log << std::setw(3) << j << std::setprecision(6) << std::setw(14) << modelValue << std::setw(14) << marketValue
            << std::setw(14) << valueDiff << std::setw(14) << modelAlpha << std::setw(14) << modelH << "\n";
    }
    if (parametrization != nullptr) {
        // report alpha, kappa at t_expiry^+ for last expiry
        t += 2 * 1.0 / 500.0;
        modelAlpha = parametrization->alpha(t);
        modelH = parametrization->H(t);
    }
    log << "t >= " << t << ": infDkAlpha = " << modelAlpha << " infDkH = " << modelH << "\n";
    return log.str();
}

} // namespace data
} // namespace ore
