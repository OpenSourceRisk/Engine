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
#include <ored/utilities/conventionsbasedfutureexpiry.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>

#include <qle/indexes/fallbackovernightindex.hpp>
#include <qle/models/cpicapfloorhelper.hpp>
#include <qle/models/futureoptionhelper.hpp>
#include <qle/models/fxeqoptionhelper.hpp>
#include <qle/models/yoycapfloorhelper.hpp>
#include <qle/models/yoyswaphelper.hpp>
#include <qle/utilities/inflation.hpp>

#include <ql/exercise.hpp>
#include <ql/models/shortrate/calibrationhelpers/swaptionhelper.hpp>

using QuantExt::InfJyParameterization;
using std::map;
using std::ostringstream;
using std::right;
using std::setprecision;
using std::setw;
using std::string;
using std::vector;

namespace ore {
namespace data {

namespace {
Real impliedVolatility(const QuantLib::ext::shared_ptr<BlackCalibrationHelper>& h) {
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

// Struct to store some helper values needed below when printing out calibration details.
struct HelperValues {
    Real modelValue;
    Real marketValue;
    Real error;
    Time maturity;
};

// Deal with possible JY inflation helpers. Use Date key to order the results so as to avoid re-calculating the
// time in the parameterisation.
map<Date, HelperValues> jyHelperValues(const vector<QuantLib::ext::shared_ptr<CalibrationHelper>>& cb,
                                       const Array& times) {

    map<Date, HelperValues> result;

    for (const auto& ci : cb) {

        if (QuantLib::ext::shared_ptr<CpiCapFloorHelper> h =
                QuantLib::ext::dynamic_pointer_cast<CpiCapFloorHelper>(ci)) {
            HelperValues hv;
            hv.modelValue = h->modelValue();
            hv.marketValue = h->marketValue();
            hv.error = hv.modelValue - hv.marketValue;
            auto d = h->instrument()->fixingDate();
            result[d] = hv;
            continue;
        }

        if (QuantLib::ext::shared_ptr<YoYCapFloorHelper> h =
                QuantLib::ext::dynamic_pointer_cast<YoYCapFloorHelper>(ci)) {
            HelperValues hv;
            hv.modelValue = h->modelValue();
            hv.marketValue = h->marketValue();
            hv.error = hv.modelValue - hv.marketValue;
            auto d = h->yoyCapFloor()->lastYoYInflationCoupon()->fixingDate();
            result[d] = hv;
            continue;
        }

        if (QuantLib::ext::shared_ptr<YoYSwapHelper> h = QuantLib::ext::dynamic_pointer_cast<YoYSwapHelper>(ci)) {
            HelperValues hv;
            hv.modelValue = h->modelRate();
            hv.marketValue = h->marketRate();
            hv.error = hv.modelValue - hv.marketValue;
            auto d = h->yoySwap()->maturityDate();
            result[d] = hv;
            continue;
        }

        QL_FAIL("Expected JY calibration instruments to be one of: CPI cap floor, YoY cap floor or YoY swap.");
    }

    QL_REQUIRE(result.size() == times.size() + 1, "Expected JY times to be 1 less the number of instruments.");

    Size ctr = 0;
    for (auto& kv : result) {
        if (ctr < times.size())
            kv.second.maturity = times[ctr++];
        else
            kv.second.maturity = times.empty() ? 0.0 : times.back();
    }

    return result;
}

} // namespace

std::string getCalibrationDetails(LgmCalibrationInfo& info,
                                  const std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>>& basket,
                                  const QuantLib::ext::shared_ptr<IrLgm1fParametrization>& parametrization) {
    std::ostringstream log;
    log << std::right << std::setw(3) << "#" << std::setw(14) << "time" << std::setw(14) << "modelVol" << std::setw(14)
        << "marketVol" << std::setw(14) << "(diff)" << std::setw(14) << "modelValue" << std::setw(14) << "marketValue"
        << std::setw(14) << "(diff)" << std::setw(14) << "irlgm1fAlpha" << std::setw(14) << "irlgm1fKappa"
        << std::setw(16) << "irlgm1fHwSigma\n";
    Real t = 0.0, modelAlpha = 0.0, modelKappa = 0.0, modelHwSigma = 0.0;
    info.lgmCalibrationData.clear();
    for (Size j = 0; j < basket.size(); j++) {
        Real modelValue = basket[j]->modelValue();
        Real marketValue = basket[j]->marketValue();
        Real valueDiff = modelValue - marketValue;
        Volatility modelVol = 0, marketVol = 0, volDiff = 0;
        QuantLib::ext::shared_ptr<SwaptionHelper> swaption =
            QuantLib::ext::dynamic_pointer_cast<SwaptionHelper>(basket[j]);
        if (swaption != nullptr && parametrization != nullptr) {
            // report alpha, kappa at t_expiry^-
            t = parametrization->termStructure()->timeFromReference(swaption->swaption()->exercise()->date(0));
            modelAlpha = parametrization->alpha(t - 1E-4);
            modelKappa = parametrization->kappa(t - 1E-4);
            modelHwSigma = parametrization->hullWhiteSigma(t - 1E-4);
        }
        // TODO handle other calibration helpers, too (capfloor)
        marketVol = basket[j]->volatility()->value();
        modelVol = impliedVolatility(basket[j]);
        volDiff = modelVol - marketVol;
        log << std::setw(3) << j << std::setprecision(6) << std::setw(14) << t << std::setw(14) << modelVol
            << std::setw(14) << marketVol << std::setw(14) << volDiff << std::setw(14) << modelValue << std::setw(14)
            << marketValue << std::setw(14) << valueDiff << std::setw(14) << modelAlpha << std::setw(14) << modelKappa
            << std::setw(16) << modelHwSigma << "\n";
        info.lgmCalibrationData.push_back(
            LgmCalibrationData{t, modelVol, marketVol, modelValue, marketValue, modelAlpha, modelKappa, modelHwSigma});
    }
    if (parametrization != nullptr) {
        // report alpha, kappa at t_expiry^+ for last expiry
        modelAlpha = parametrization->alpha(t + 1E-4);
        modelKappa = parametrization->kappa(t + 1E-4);
        modelHwSigma = parametrization->hullWhiteSigma(t + 1E-4);
    }
    log << "t >= " << t << ": irlgm1fAlpha = " << modelAlpha << " irlgm1fKappa = " << modelKappa
        << " irlgm1fHwSigma = " << modelHwSigma << "\n";
    return log.str();
}

std::string getCalibrationDetails(HwCalibrationInfo& info,
                                  const std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>>& basket,
                                  const QuantLib::ext::shared_ptr<IrHwParametrization>& parametrization) {
    std::ostringstream log;
    Real t = 0.0;
    Matrix modelSigma;
    Array modelKappa;
    info.hwCalibrationData.clear();
    for (Size j = 0; j < basket.size(); j++) {
        Real modelValue = basket[j]->modelValue();
        Real marketValue = basket[j]->marketValue();
        Real valueDiff = modelValue - marketValue;
        Volatility modelVol = 0, marketVol = 0, volDiff = 0;
        QuantLib::ext::shared_ptr<SwaptionHelper> swaption =
            QuantLib::ext::dynamic_pointer_cast<SwaptionHelper>(basket[j]);
        if (swaption != nullptr && parametrization != nullptr) {
            // report alpha, kappa at t_expiry^-
            t = parametrization->termStructure()->timeFromReference(swaption->swaption()->exercise()->date(0));
            modelSigma = parametrization->sigma_x(t - 1E-4);
            modelKappa = parametrization->kappa(t - 1E-4);
        }
        marketVol = basket[j]->volatility()->value();
        modelVol = impliedVolatility(basket[j]);
        volDiff = modelVol - marketVol;
        log << "#" << j << ", t=" << std::setprecision(6) << t << ", modelVol = " << modelVol
            << ", marketVol = " << marketVol << ", volDiff = " << volDiff << "\n";
        log << "      modelValue = " << modelValue << ", marketValue = " << marketValue << ", valueDiff = " << valueDiff
            << "\n";
        log << "modelkappa = " << modelKappa << "\nmodelSigma =\n" << modelSigma << "\n";
        info.hwCalibrationData.push_back(
            HwCalibrationData{t, modelVol, marketVol, modelValue, marketValue, modelSigma, modelKappa});
    }
    if (parametrization != nullptr) {
        // report alpha, kappa at t_expiry^+ for last expiry
        modelSigma = parametrization->sigma_x(t + 1E-4);
        modelKappa = parametrization->kappa(t + 1E-4);
    }
    log << "t >= " << t << ": kappa = " << modelKappa << "\nsigma = \n" << modelSigma << "\n";
    return log.str();
}

std::string getCalibrationDetails(const std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>>& basket,
                                  const QuantLib::ext::shared_ptr<FxBsParametrization>& parametrization,
                                  const QuantLib::ext::shared_ptr<Parametrization>& domesticIrModel) {
    auto lgmParametrization = QuantLib::ext::dynamic_pointer_cast<IrLgm1fParametrization>(domesticIrModel);
    if (lgmParametrization) {
        return getCalibrationDetails(basket, parametrization, lgmParametrization);
    } else {
        return std::string();
    }
}

std::string getCalibrationDetails(const std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>>& basket,
                                  const QuantLib::ext::shared_ptr<FxBsParametrization>& parametrization,
                                  const QuantLib::ext::shared_ptr<IrLgm1fParametrization>& domesticLgm) {
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
        QuantLib::ext::shared_ptr<FxEqOptionHelper> fxoption =
            QuantLib::ext::dynamic_pointer_cast<FxEqOptionHelper>(basket[j]);
        if (fxoption != nullptr && parametrization != nullptr && domesticLgm != nullptr) {
            // report alpha, kappa at t_expiry^-
            t = domesticLgm->termStructure()->timeFromReference(fxoption->option()->exercise()->date(0));
            modelSigma = parametrization->sigma(t - 1E-4);
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
        modelSigma = parametrization->sigma(t + 1E-4);
    }
    log << "t >= " << t << ": fxbsSigma = " << modelSigma << "\n";
    return log.str();
}

std::string getCalibrationDetails(const std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>>& basket,
                                  const QuantLib::ext::shared_ptr<EqBsParametrization>& parametrization,
                                  const QuantLib::ext::shared_ptr<Parametrization>& domesticIrModel) {
    auto lgmParametrization = QuantLib::ext::dynamic_pointer_cast<IrLgm1fParametrization>(domesticIrModel);
    if (lgmParametrization) {
        return getCalibrationDetails(basket, parametrization, lgmParametrization);
    } else {
        return std::string();
    }
}

std::string getCalibrationDetails(const std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>>& basket,
                                  const QuantLib::ext::shared_ptr<EqBsParametrization>& parametrization,
                                  const QuantLib::ext::shared_ptr<IrLgm1fParametrization>& domesticLgm) {
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
        QuantLib::ext::shared_ptr<FxEqOptionHelper> eqoption =
            QuantLib::ext::dynamic_pointer_cast<FxEqOptionHelper>(basket[j]);
        if (eqoption != nullptr && parametrization != nullptr && domesticLgm != nullptr) {
            t = domesticLgm->termStructure()->timeFromReference(eqoption->option()->exercise()->date(0));
            modelSigma = parametrization->sigma(t - 1E-4);
        }
        marketVol = basket[j]->volatility()->value();
        modelVol = impliedVolatility(basket[j]);
        volDiff = modelVol - marketVol;
        log << std::setw(3) << j << std::setprecision(6) << std::setw(14) << t << std::setw(14) << modelVol
            << std::setw(14) << marketVol << std::setw(14) << volDiff << std::setw(14) << modelValue << std::setw(14)
            << marketValue << std::setw(14) << valueDiff << std::setw(14) << modelSigma << "\n";
    }
    if (parametrization != nullptr) {
        modelSigma = parametrization->sigma(t + 1E-4);
    }
    log << "t >= " << t << ": eqbsSigma = " << modelSigma << "\n";
    return log.str();
}

std::string getCalibrationDetails(const std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>>& basket,
                                  const QuantLib::ext::shared_ptr<CommoditySchwartzParametrization>& parametrization) {
    std::ostringstream log;
    log << std::right << std::setw(3) << "#" << std::setw(14) << "time" << std::setw(14) << "modelVol" << std::setw(14)
        << "marketVol" << std::setw(14) << "(diff)" << std::setw(14) << "modelValue" << std::setw(14) << "marketValue"
        << std::setw(14) << "(diff)" << std::setw(14) << "Sigma" << setw(14) << "Kappa" << setw(14) << "Seasonality"
        << setw(14) << "a\n";
    Real t = 0.0;
    Real modelSigma = parametrization->sigmaParameter();
    Real modelKappa = parametrization->kappaParameter();
    Real modelSeasonality, a;
    for (Size j = 0; j < basket.size(); j++) {
        Real modelValue = basket[j]->modelValue();
        Real marketValue = basket[j]->marketValue();
        Real valueDiff = modelValue - marketValue;
        Volatility modelVol = 0, marketVol = 0, volDiff = 0;
        QuantLib::ext::shared_ptr<FutureOptionHelper> option =
            QuantLib::ext::dynamic_pointer_cast<FutureOptionHelper>(basket[j]);
        if (option != nullptr && parametrization != nullptr) {
            t = option->priceCurve()->timeFromReference(option->option()->exercise()->date(0));
            // modelSigma = parametrization->sigma(t - 1E-4);
        }
        marketVol = basket[j]->volatility()->value();
        modelVol = impliedVolatility(basket[j]);
        volDiff = modelVol - marketVol;
        modelSeasonality = parametrization->m(t);
        a = parametrization->a(t);
        log << std::setw(3) << j << std::setprecision(6) << std::setw(14) << t << std::setw(14) << modelVol
            << std::setw(14) << marketVol << std::setw(14) << volDiff << std::setw(14) << modelValue << std::setw(14)
            << marketValue << std::setw(14) << valueDiff << std::setw(14) << modelSigma << std::setw(14) << modelKappa
            << std::setw(14) << modelSeasonality << std::setw(14) << a << "\n";
    }
    if (parametrization != nullptr) {
        modelSigma = parametrization->sigma(t + 1E-4);
    }
    log << "t >= " << t << ": Sigma = " << modelSigma << ", Kappa = " << modelKappa << "\n";
    return log.str();
}

std::string getCalibrationDetails(const std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>>& basket,
                                  const QuantLib::ext::shared_ptr<InfDkParametrization>& parametrization,
                                  bool indexIsInterpolated) {
    std::ostringstream log;
    log << std::right << std::setw(3) << "#" << std::setw(14) << "time" << std::setw(14) << "modelValue"
        << std::setw(14) << "marketValue" << std::setw(14) << "(diff)" << std::setw(14) << "infdkAlpha" << std::setw(14)
        << "infdkH\n";
    Real t = 0.0, modelAlpha = 0.0, modelH = 0.0;
    for (Size j = 0; j < basket.size(); j++) {
        Real modelValue = basket[j]->modelValue();
        Real marketValue = basket[j]->marketValue();
        Real valueDiff = (modelValue - marketValue);
        QuantLib::ext::shared_ptr<CpiCapFloorHelper> instr =
            QuantLib::ext::dynamic_pointer_cast<CpiCapFloorHelper>(basket[j]);
        if (instr != nullptr && parametrization != nullptr) {
            // report alpha, H at t_expiry^-
            t = inflationYearFraction(
                parametrization->termStructure()->frequency(), indexIsInterpolated,
                parametrization->termStructure()->dayCounter(), parametrization->termStructure()->baseDate(),
                instr->instrument()->payDate() - parametrization->termStructure()->observationLag());
            modelAlpha = parametrization->alpha(t - 1.0 / 250.0);
            modelH = parametrization->H(t - 1.0 / 250.0);
        }
        // TODO handle other calibration helpers, too (capfloor)
        log << std::setw(3) << j << std::setprecision(6) << std::setw(14) << t << std::setw(14) << modelValue
            << std::setw(14) << marketValue << std::setw(14) << valueDiff << std::setw(14) << modelAlpha
            << std::setw(14) << modelH << "\n";
    }
    if (parametrization != nullptr) {
        // report alpha, kappa at t_expiry^+ for last expiry
        modelAlpha = parametrization->alpha(t + 1.0 / 250.0);
        modelH = parametrization->H(t + 1.0 / 2500.0);
    }
    log << "t >= " << t << ": infDkAlpha = " << modelAlpha << " infDkH = " << modelH << "\n";
    return log.str();
}

string getCalibrationDetails(const vector<QuantLib::ext::shared_ptr<CalibrationHelper>>& rrBasket,
                             const vector<QuantLib::ext::shared_ptr<CalibrationHelper>>& idxBasket,
                             const QuantLib::ext::shared_ptr<InfJyParameterization>& p, bool calibrateRealRateVol) {

    ostringstream log;
    Real epsTime = 0.0001;

    // Real rate basket: if it has instruments, the real rate was calibrated.
    if (!rrBasket.empty()) {

        // Header rows
        log << "Real rate calibration:\n";
        log << right << setw(3) << "#" << setw(5) << "](-" << setw(12) << "inst_date" << setw(12) << "time" << setw(14)
            << "modelValue" << setw(14) << "marketValue" << setw(14) << "(diff)" << setw(14) << "infJyAlpha" << setw(14)
            << "infJyH\n";

        // Parameter values for corresponding to maturity of each instrument
        Array times = calibrateRealRateVol ? p->realRate()->parameterTimes(0) : p->realRate()->parameterTimes(1);
        auto helperValues = jyHelperValues(rrBasket, times);
        Size ctr = 0;
        Time t = 0.0;
        for (const auto& kv : helperValues) {
            auto hv = kv.second;
            t = hv.maturity - epsTime;
            string bound = "<=";
            if (helperValues.size() == 1) {
                bound = " -";
            } else if (ctr == helperValues.size() - 1) {
                t += 2 * epsTime;
                bound = " >";
            }
            auto alpha = p->realRate()->alpha(t);
            auto h = p->realRate()->H(t);
            log << setw(3) << ctr++ << setw(5) << bound << setw(6) << io::iso_date(kv.first) << setprecision(6)
                << setw(12) << hv.maturity << setw(14) << hv.modelValue << setw(14) << hv.marketValue << setw(14)
                << hv.error << setw(14) << alpha << setw(14) << h << "\n";
        }
    }

    // Inflation index basket: if it has instruments, the inflation index was calibrated.
    if (!idxBasket.empty()) {

        // Header rows
        log << "Inflation index calibration:\n";
        log << right << setw(3) << "#" << setw(5) << "](-" << setw(12) << "inst_date" << setw(12) << "time" << setw(14)
            << "modelValue" << setw(14) << "marketValue" << setw(14) << "(diff)" << setw(14) << "infJySigma\n";

        // Parameter values for corresponding to maturity of each instrument
        Array times = p->index()->parameterTimes(0);
        auto helperValues = jyHelperValues(idxBasket, times);
        Size ctr = 0;
        Time t = 0.0;
        for (const auto& kv : helperValues) {
            auto hv = kv.second;
            t = hv.maturity - epsTime;
            string bound = "<=";
            if (helperValues.size() == 1) {
                bound = " -";
            } else if (ctr == helperValues.size() - 1) {
                t += 2 * epsTime;
                bound = " >";
            }
            auto sigma = p->index()->sigma(t);
            log << setw(3) << ctr++ << setw(5) << bound << setw(6) << io::iso_date(kv.first) << setprecision(6)
                << setw(12) << hv.maturity << setw(14) << hv.modelValue << setw(14) << hv.marketValue << setw(14)
                << hv.error << setw(14) << sigma << "\n";
        }
    }

    return log.str();
}

string getCalibrationDetails(const QuantLib::ext::shared_ptr<IrLgm1fParametrization>& parametrization) {
    std::ostringstream log;

    log << right << setw(3) << "#" << setw(14) << "time" << setw(14) << "irlgm1fAlpha" << setw(14)
        << "irlgm1fHwSigma\n";
    Real t = 0.0;
    Size j = 0;
    for (; j < parametrization->parameterTimes(0).size(); ++j) {
        t = parametrization->parameterTimes(0)[j];
        Real alpha = parametrization->alpha(t - 1E-4);
        Real sigma = parametrization->hullWhiteSigma(t - 1E-4);
        log << setw(3) << j << setprecision(6) << setw(14) << t << setw(14) << alpha << setw(14) << sigma << "\n";
    }
    log << setw(3) << j << setprecision(6) << setw(14) << (std::to_string(t) + "+") << setw(14)
        << parametrization->alpha(t + 1E-4) << setw(14) << parametrization->hullWhiteSigma(t + 1E-4) << "\n";

    log << right << setw(3) << "#" << setw(14) << "time" << setw(14) << "irlgm1fKappa" << setw(14) << "irlgm1fH\n";
    t = 0.0;
    j = 0;
    for (; j < parametrization->parameterTimes(1).size(); ++j) {
        t = parametrization->parameterTimes(0)[j];
        Real kappa = parametrization->kappa(t - 1E-4);
        Real H = parametrization->H(t - 1E-4);
        log << setw(3) << j << setprecision(6) << setw(14) << t << setw(14) << kappa << setw(14) << H << "\n";
    }
    log << setw(3) << j << setprecision(6) << setw(14) << (std::to_string(t) + "+") << setw(14)
        << parametrization->kappa(t + 1E-4) << setw(14) << parametrization->H(t + 1E-4) << "\n";

    return log.str();
}

namespace {

struct MaturityGetter : boost::static_visitor<Date> {

    MaturityGetter(const Calendar& calendar, const Date& referenceDate)
        : calendar(calendar), referenceDate(referenceDate) {}

    Date operator()(const Date& d) const { return d; }

    Date operator()(const Period& p) const { return calendar.advance(referenceDate, p); }

    Calendar calendar;
    Date referenceDate;
};

} // namespace

Date optionMaturity(const boost::variant<Date, Period>& maturity, const QuantLib::Calendar& calendar,
                    const QuantLib::Date& referenceDate) {

    return boost::apply_visitor(MaturityGetter(calendar, referenceDate), maturity);
}

Real cpiCapFloorStrikeValue(const QuantLib::ext::shared_ptr<BaseStrike>& strike,
                            const QuantLib::ext::shared_ptr<QuantLib::ZeroInflationIndex>& inflationIndex,
                            const QuantLib::Handle<QuantLib::CPIVolatilitySurface>& volSurface,
                            const QuantLib::Date& maturityDate, const bool dontCalibrate) {
    if (auto abs = QuantLib::ext::dynamic_pointer_cast<AbsoluteStrike>(strike)) {
        return abs->strike();
    } else if (auto atm = QuantLib::ext::dynamic_pointer_cast<AtmStrike>(strike)) {
        QL_REQUIRE(atm->atmType() == DeltaVolQuote::AtmFwd,
                   "only atm forward allowed as atm strike for cpi cap floors");
        DLOG("Calculating forward strike for CPI cap floor with maturity "
             << maturityDate << " and observation lag " << volSurface->observationLag() << " and index interpolation "
             << volSurface->indexIsInterpolated());
        auto zits = inflationIndex->zeroInflationTermStructure();
        auto fixingDate = ZeroInflation::fixingDate(maturityDate, volSurface->observationLag(), volSurface->frequency(),
                                                    volSurface->indexIsInterpolated());

        auto forwardCPI = dontCalibrate
                              ? 100 * std::pow(1 + zits->zeroRate(maturityDate - volSurface->observationLag()),
                                               zits->dayCounter().yearFraction(zits->baseDate(), fixingDate))
                              : ZeroInflation::cpiFixing(inflationIndex, maturityDate, volSurface->observationLag(),
                                                         volSurface->indexIsInterpolated());
        DLOG("Forward CPI is " << forwardCPI);
        auto baseFixing =
            dontCalibrate ? 100.
                          : ZeroInflation::cpiFixing(inflationIndex, volSurface->referenceDate(),
                                                     volSurface->observationLag(), volSurface->indexIsInterpolated());
        DLOG("Base CPI fixing is " << baseFixing);
        auto growth = forwardCPI / baseFixing;
        auto baseDate = ZeroInflation::fixingDate(volSurface->referenceDate(), volSurface->observationLag(),
                                                  volSurface->frequency(), volSurface->indexIsInterpolated());
        auto ttm = zits->dayCounter().yearFraction(baseDate, fixingDate);
        auto strike = std::pow(growth, 1.0 / ttm) - 1.0;
        return strike;
    } else {
        QL_FAIL("cpi cap floor strike type not supported, expected absolute strike or atm fwd strike, got '"
                << strike->toString());
    }
}

Real yoyCapFloorStrikeValue(const QuantLib::ext::shared_ptr<BaseStrike>& strike,
                            const QuantLib::ext::shared_ptr<YoYInflationTermStructure>& curve,
                            const QuantLib::Date& optionMaturityDate) {
    if (auto abs = QuantLib::ext::dynamic_pointer_cast<AbsoluteStrike>(strike)) {
        return abs->strike();
    } else if (auto atm = QuantLib::ext::dynamic_pointer_cast<AtmStrike>(strike)) {
        QL_REQUIRE(atm->atmType() == DeltaVolQuote::AtmFwd,
                   "only atm forward allowed as atm strike for cpi cap floors");
        return curve->yoyRate(optionMaturityDate - curve->observationLag());
    } else {
        QL_FAIL("yoy cap floor strike type not supported, expected absolute strike or atm fwd strike, got '"
                << strike->toString());
    }
}

Real atmForward(const Real s0, const Handle<YieldTermStructure>& r, const Handle<YieldTermStructure>& q, const Real t) {
    return s0 * q->discount(t) / r->discount(t);
}

std::pair<std::string, Period> convertIndexToCamCorrelationEntry(const std::string& i) {
    IndexInfo info(i);
    if (info.isIr()) {
        return std::make_pair("IR#" + info.ir()->currency().code(), info.ir()->tenor());
    } else if (info.isInf()) {
        return std::make_pair("INF#" + info.infName(), 0 * Days);
    } else if (info.isFx()) {
        return std::make_pair("FX#" + info.fx()->sourceCurrency().code() + info.fx()->targetCurrency().code(),
                              0 * Days);
    } else if (info.isEq()) {
        return std::make_pair("EQ#" + info.eq()->name(), 0 * Days);
    } else if (info.isComm()) {
        return std::make_pair("COM#" + info.commName(), 0 * Days);
    } else {
        QL_FAIL("convertIndextoCamCorrelationEntry(): index '" << i << "' not recognised");
    }
}

IndexInfo::IndexInfo(const std::string& name, const QuantLib::ext::shared_ptr<Market>& market)
    : name_(name), market_(market) {
    isFx_ = isEq_ = isComm_ = isIr_ = isInf_ = isIrIbor_ = isIrSwap_ = isGeneric_ = infIsInterpolated_ = false;
    bool done = false;
    // first handle the index types that we can recognise by a prefix
    if (boost::starts_with(name, "COMM-")) {
        // index will be created on the fly, since it depends on the obsDate in general
        isComm_ = done = true;
        std::vector<std::string> tokens;
        boost::split(tokens, name, boost::is_any_of("#!"));
        QL_REQUIRE(!tokens.empty(), "IndexInfo: no commodity name found for '" << name << "'");
        commName_ = parseCommodityIndex(tokens.front())->underlyingName();
    } else if (boost::starts_with(name, "FX-")) {
        // parse fx index using conventions
        fx_ = parseFxIndex(name, Handle<Quote>(), Handle<YieldTermStructure>(), Handle<YieldTermStructure>(), true);
        isFx_ = done = true;
    } else if (boost::starts_with(name, "EQ-")) {
        eq_ = parseEquityIndex(name);
        if (market_ != nullptr) {
            try {
                eq_ = *market_->equityCurve(eq_->name());
            } catch (...) {
            }
        }
        isEq_ = done = true;
    } else if (boost::starts_with(name, "GENERIC-")) {
        generic_ = parseGenericIndex(name);
        isGeneric_ = done = true;
    }
    // no easy way to see if it is an Ibor index, so try and error
    if (!done) {
        try {
            irIbor_ = parseIborIndex(name);
            ir_ = irIbor_;
            isIr_ = isIrIbor_ = done = true;
        } catch (...) {
        }
    }
    // same for swap
    if (!done) {
        try {
            irSwap_ = parseSwapIndex(name, Handle<YieldTermStructure>(), Handle<YieldTermStructure>());
            ir_ = irSwap_;
            isIr_ = isIrSwap_ = done = true;
        } catch (...) {
        }
    }
    // and same for inflation
    if (!done) {
        try {
            std::tie(inf_, infName_, infIsInterpolated_) = parseScriptedInflationIndex(name);
            isInf_ = done = true;
        } catch (...) {
        }
    }
    QL_REQUIRE(done, "Could not build index info for '"
                         << name
                         << "', expected a valid COMM, FX, EQ, GENERIC, Ibor, Swap, Inflation index identifier.");
}

QuantLib::ext::shared_ptr<Index> IndexInfo::index(const Date& obsDate) const {
    if (isFx_)
        return fx_;
    else if (isEq_)
        return eq_;
    else if (isIr_)
        return ir_;
    else if (isInf_)
        return inf_;
    else if (isGeneric_)
        return generic_;
    else if (isComm_) {
        return comm(obsDate);
    } else {
        QL_FAIL("could not parse index '" << name_ << "'");
    }
}

QuantLib::ext::shared_ptr<CommodityIndex> IndexInfo::comm(const Date& obsDate) const {
    if (isComm())
        return parseScriptedCommodityIndex(name(), obsDate);
    else
        return nullptr;
}

std::string IndexInfo::commName() const {
    QL_REQUIRE(isComm(), "IndexInfo::commName(): commodity index required, got " << *this);
    return commName_;
}

std::string IndexInfo::infName() const {
    QL_REQUIRE(isInf(), "IndexInfo::infName(): inflation index required, got " << *this);
    return infName_;
}

std::ostream& operator<<(std::ostream& o, const IndexInfo& i) {
    o << "index '" << i.name() << "'";
    if (i.isFx())
        o << ", type FX, index name '" << i.fx()->name() << "'";
    if (i.isEq())
        o << ", type EQ, index name '" << i.eq()->name() << "'";
    if (i.isComm())
        o << ", type COMM";
    if (i.isIrIbor())
        o << ", type IR Ibor, index name '" << i.irIbor()->name() << "'";
    if (i.isIrSwap())
        o << ", type IR Swap, index name '" << i.irSwap()->name() << "'";
    if (i.isGeneric())
        o << ", type Generic, index name '" << i.generic()->name() << "'";
    return o;
}

QuantLib::ext::shared_ptr<FallbackIborIndex>
IndexInfo::irIborFallback(const QuantLib::ext::shared_ptr<IborFallbackConfig>& iborFallbackConfig,
                          const Date& asof) const {
    if (isIrIbor_ && iborFallbackConfig->isIndexReplaced(name_, asof)) {
        auto data = iborFallbackConfig->fallbackData(name_);
        // we don't support convention based rfr fallback indices, with ore ticket 1758 this might change
        auto on = QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(parseIborIndex(data.rfrIndex));
        QL_REQUIRE(on, "IndexInfo::irIborFallback(): could not cast rfr index '"
                           << data.rfrIndex << "' for ibor fallback index '" << name_ << "' to an overnight index");
        return QuantLib::ext::make_shared<FallbackIborIndex>(irIbor_, on, data.spread, data.switchDate,
                                                             irIbor_->forwardingTermStructure());
    }
    return nullptr;
}

QuantLib::ext::shared_ptr<FallbackOvernightIndex>
IndexInfo::irOvernightFallback(const QuantLib::ext::shared_ptr<IborFallbackConfig>& iborFallbackConfig,
                               const Date& asof) const {
    if (isIrIbor_ && iborFallbackConfig->isIndexReplaced(name_, asof)) {
        auto data = iborFallbackConfig->fallbackData(name_);
        // we don't support convention based rfr fallback indices, with ore ticket 1758 this might change
        auto on = QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(parseIborIndex(data.rfrIndex));
        QL_REQUIRE(on, "IndexInfo::irIborFallback(): could not cast rfr index '"
                           << data.rfrIndex << "' for ibor fallback index '" << name_ << "' to an overnight index");
        if (auto original = QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(irIbor_))
            return QuantLib::ext::make_shared<FallbackOvernightIndex>(original, on, data.spread, data.switchDate,
                                                                      original->forwardingTermStructure());
        else
            return nullptr;
    }
    return nullptr;
}

QuantLib::ext::shared_ptr<QuantExt::CommodityIndex> parseScriptedCommodityIndex(const std::string& indexName,
                                                                                const QuantLib::Date& obsDate) {

    QL_REQUIRE(!indexName.empty(), "parseScriptedCommodityIndex(): empty index name");
    QuantLib::ext::shared_ptr<Conventions> conventions = InstrumentConventions::instance().conventions();
    std::vector<std::string> tokens;
    boost::split(tokens, indexName, boost::is_any_of("#!"));
    std::string plainIndexName = tokens.front();
    std::string commName = parseCommodityIndex(plainIndexName)->underlyingName();

    QuantLib::ext::shared_ptr<CommodityFutureConvention> convention;
    if (conventions->has(commName)) {
        convention = QuantLib::ext::dynamic_pointer_cast<CommodityFutureConvention>(conventions->get(commName));
    }
    Calendar fixingCalendar = convention ? convention->calendar() : NullCalendar();

    std::vector<std::string> tokens1;
    boost::split(tokens1, indexName, boost::is_any_of("#"));
    std::vector<std::string> tokens2;
    boost::split(tokens2, indexName, boost::is_any_of("!"));

    QuantLib::ext::shared_ptr<CommodityIndex> res;
    if (tokens1.size() >= 2) {
        // handle form 3)- 5), i.e. COMM-name#N#D#Cal, COMM-name#N#D, COMM-name#N
        QL_REQUIRE(tokens.size() <= 4,
                   "parseScriptedCommodityIndex(): expected COMM-Name#N, Comm-Name#N#D, Comm-Name#N#D#Cal, got '"
                       << indexName << "'");
        QL_REQUIRE(convention,
                   "parseScriptedCommodityIndex(): commodity future convention required for '" << indexName << "'");
        QL_REQUIRE(obsDate != Date(), "parseScriptedCommodityIndex(): obsDate required for '" << indexName << "'");
        int offset = std::stoi(tokens[1]);
        int deliveryRollDays = 0;
        if (tokens.size() >= 3)
            deliveryRollDays = parseInteger(tokens[2]);
        Calendar rollCal = tokens.size() == 4 ? parseCalendar(tokens[3]) : fixingCalendar;
        ConventionsBasedFutureExpiry expiryCalculator(*convention);
        Date adjustedObsDate = deliveryRollDays != 0 ? rollCal.advance(obsDate, deliveryRollDays * Days) : obsDate;
        res = parseCommodityIndex(commName, false, Handle<PriceTermStructure>(), fixingCalendar, true);
        res = res->clone(expiryCalculator.nextExpiry(true, adjustedObsDate, offset));
    } else if (tokens2.size() >= 2) {
        // handle form 6), i.e. COMM-name!N
        QL_REQUIRE(tokens.size() <= 2,
                   "parseScriptedCommodityIndex(): expected COMM-Name!N, got '" << indexName << "'");
        QL_REQUIRE(convention,
                   "parseScriptedCommodityIndex(): commodity future convention required for '" << indexName << "'");
        QL_REQUIRE(obsDate != Date(), "parseScriptedCommodityIndex(): obsDate required for '" << indexName << "'");
        int offset = std::stoi(tokens[1]);
        ConventionsBasedFutureExpiry expiryCalculator(*convention);
        res = parseCommodityIndex(commName, false, Handle<PriceTermStructure>(), fixingCalendar, true);
        res = res->clone(expiryCalculator.expiryDate(obsDate, offset));
    } else {
        // handle 0), 1) and 2)
        res = parseCommodityIndex(indexName, true, QuantLib::Handle<QuantExt::PriceTermStructure>(), fixingCalendar,
                                  false);
    }

    TLOG("parseScriptCommodityIndex(" << indexName << "," << QuantLib::io::iso_date(obsDate) << ") = " << res->name());
    return res;
}

// Remove in the next release, interpolation has to happen in the coupon (script) and not in the index
std::tuple<QuantLib::ext::shared_ptr<QuantLib::ZeroInflationIndex>, std::string, bool>
parseScriptedInflationIndex(const std::string& indexName) {
    QL_REQUIRE(!indexName.empty(), "parseScriptedInflationIndex(): empty index name");
    std::vector<std::string> tokens;
    boost::split(tokens, indexName, boost::is_any_of("#"));
    std::string plainIndexName = tokens.front();
    bool interpolated = false;
    if (tokens.size() == 1) {
        interpolated = false;
    } else if (tokens.size() == 2) {
        QL_REQUIRE(tokens[1] == "F" || tokens[1] == "L", "parseScriptedInflationIndex(): expected ...#[L|F], got ...#"
                                                             << tokens[1] << " in '" << indexName << "'");
        interpolated = tokens[1] == "L";
    } else {
        QL_FAIL("parseScriptedInflationIndex(): expected IndexName or IndexName#[F|L], got '" << indexName << "'");
    }
    return std::make_tuple(parseZeroInflationIndex(plainIndexName, Handle<ZeroInflationTermStructure>()),
                           plainIndexName, interpolated);
}

} // namespace data
} // namespace ore
