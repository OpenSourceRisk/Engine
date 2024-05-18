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
#include <qle/models/futureoptionhelper.hpp>
#include <qle/models/yoycapfloorhelper.hpp>
#include <qle/models/yoyswaphelper.hpp>

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
map<Date, HelperValues> jyHelperValues(const vector<QuantLib::ext::shared_ptr<CalibrationHelper>>& cb, const Array& times) {

    map<Date, HelperValues> result;

    for (const auto& ci : cb) {

        if (QuantLib::ext::shared_ptr<CpiCapFloorHelper> h = QuantLib::ext::dynamic_pointer_cast<CpiCapFloorHelper>(ci)) {
            HelperValues hv;
            hv.modelValue = h->modelValue();
            hv.marketValue = h->marketValue();
            hv.error = hv.modelValue - hv.marketValue;
            auto d = h->instrument()->fixingDate();
            result[d] = hv;
            continue;
        }

        if (QuantLib::ext::shared_ptr<YoYCapFloorHelper> h = QuantLib::ext::dynamic_pointer_cast<YoYCapFloorHelper>(ci)) {
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
        QuantLib::ext::shared_ptr<SwaptionHelper> swaption = QuantLib::ext::dynamic_pointer_cast<SwaptionHelper>(basket[j]);
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
        QuantLib::ext::shared_ptr<FxEqOptionHelper> fxoption = QuantLib::ext::dynamic_pointer_cast<FxEqOptionHelper>(basket[j]);
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
    auto lgmParametrization = QuantLib::ext::dynamic_pointer_cast<IrLgm1fParametrization>(parametrization);
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
        QuantLib::ext::shared_ptr<FxEqOptionHelper> eqoption = QuantLib::ext::dynamic_pointer_cast<FxEqOptionHelper>(basket[j]);
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
        << std::setw(14) << "(diff)" << std::setw(14) << "Sigma" << setw(14) << "Kappa\n";
    Real t = 0.0;
    Real modelSigma = parametrization->sigmaParameter();
    Real modelKappa = parametrization->kappaParameter();
    for (Size j = 0; j < basket.size(); j++) {
        Real modelValue = basket[j]->modelValue();
        Real marketValue = basket[j]->marketValue();
        Real valueDiff = modelValue - marketValue;
        Volatility modelVol = 0, marketVol = 0, volDiff = 0;
        QuantLib::ext::shared_ptr<FutureOptionHelper> option = QuantLib::ext::dynamic_pointer_cast<FutureOptionHelper>(basket[j]);
        if (option != nullptr && parametrization != nullptr) {
            t = option->priceCurve()->timeFromReference(option->option()->exercise()->date(0));
            //modelSigma = parametrization->sigma(t - 1E-4);
        }
        marketVol = basket[j]->volatility()->value();
        modelVol = impliedVolatility(basket[j]);
        volDiff = modelVol - marketVol;
        log << std::setw(3) << j << std::setprecision(6) << std::setw(14) << t << std::setw(14) << modelVol
            << std::setw(14) << marketVol << std::setw(14) << volDiff << std::setw(14) << modelValue << std::setw(14)
            << marketValue << std::setw(14) << valueDiff << std::setw(14) << modelSigma << " " << modelKappa << "\n";
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
        QuantLib::ext::shared_ptr<CpiCapFloorHelper> instr = QuantLib::ext::dynamic_pointer_cast<CpiCapFloorHelper>(basket[j]);
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
                            const QuantLib::ext::shared_ptr<ZeroInflationTermStructure>& curve,
                            const QuantLib::Date& optionMaturityDate) {
    if (auto abs = QuantLib::ext::dynamic_pointer_cast<AbsoluteStrike>(strike)) {
        return abs->strike();
    } else if (auto atm = QuantLib::ext::dynamic_pointer_cast<AtmStrike>(strike)) {
        QL_REQUIRE(atm->atmType() == DeltaVolQuote::AtmFwd,
                   "only atm forward allowed as atm strike for cpi cap floors");
        return curve->zeroRate(optionMaturityDate);
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
        return curve->yoyRate(optionMaturityDate);
    } else {
        QL_FAIL("yoy cap floor strike type not supported, expected absolute strike or atm fwd strike, got '"
                << strike->toString());
    }
}

Real atmForward(const Real s0, const Handle<YieldTermStructure>& r, const Handle<YieldTermStructure>& q, const Real t) {
    return s0 * q->discount(t) / r->discount(t);
}

} // namespace data
} // namespace ore
