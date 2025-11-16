/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

#include <ored/model/hestonmodelcalibration.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/dategrid.hpp>
#include <ored/utilities/to_string.hpp>

#include <ql/exercise.hpp>
#include <ql/instruments/payoffs.hpp>
#include <ql/instruments/vanillaoption.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <ql/pricingengines/vanilla/analyticeuropeanengine.hpp>
#include <ql/processes/hestonprocess.hpp>
#include <ql/processes/hestonslvprocess.hpp>
#include <ql/math/randomnumbers/haltonrsg.hpp>
#include <ql/math/optimization/differentialevolution.hpp>
#include <ql/math/optimization/levenbergmarquardt.hpp>
#include <ql/models/equity/hestonmodel.hpp>
#include <ql/models/equity/hestonmodelhelper.hpp>
#include <ql/pricingengines/vanilla/analytichestonengine.hpp>
#include <ql/pricingengines/vanilla/analyticpdfhestonengine.hpp>
#include <ql/pricingengines/vanilla/analyticptdhestonengine.hpp>
#include <ql/pricingengines/vanilla/coshestonengine.hpp>
#include <ql/pricingengines/vanilla/exponentialfittinghestonengine.hpp>
#include <ql/pricingengines/vanilla/fdblackscholesvanillaengine.hpp>
#include <ql/pricingengines/vanilla/fdhestonvanillaengine.hpp>
#include <ql/quotes/simplequote.hpp>

namespace ore {
namespace data {

/*!
  Annualised Variance in Heston

        V(t) = theta + (v0 - theta) * (1 - exp(-kappa * t)) / (kappa * t)

	lim_{t->0} V(t) = v0

	lim_{t->infty} V(t) = theta

  Cost Function:

        cf = sum_{i=1}^n (f(t_i) - V_i)^2

  with time grid t_i and market variances V_i, i=1,...,n
  and three parameters a b c,

*/
class HestonVarianceCostFunction : public CostFunction {
public:
    HestonVarianceCostFunction(const std::vector<Real>& times, const std::vector<Real>& annualisedMarketVariances,
                               Real theta, Real kappa, Real v0, bool fixedTheta = false, bool fixedKappa = false,
                               bool fixedV0 = false)
        : times_(times), annualisedMarketVariances_(annualisedMarketVariances), initialValues_(3), fixed_(3) {
        QL_REQUIRE(times_.size() == annualisedMarketVariances_.size(), "vector size mismatch");
        initialValues_[0] = theta;
        initialValues_[1] = kappa;
        initialValues_[2] = v0;
        fixed_[0] = fixedTheta;
        fixed_[1] = fixedKappa;
        fixed_[2] = fixedV0;
    }

    Array initialValues() const { return initialValues_; }

    Real annualisedModelVariance(Real t, Array x) const {
        QL_REQUIRE(x.size() == 3, "3 parameters expected");
	// keep theta fixed
        Real theta = fixed_[0] ? initialValues_[0] : x[0];
        Real kappa = fixed_[1] ? initialValues_[1] : x[1];
        Real v0 = fixed_[2] ? initialValues_[2] : x[2];
        return theta + (v0 - theta) * (1.0 - exp(-kappa * t)) / (kappa * t);
    }

    // sum of error^2 across all time points 
    Real value(const Array& x) const override {
        QL_REQUIRE(x.size() == 3, "3 parameters expected");
        Real res = 0;
        for (Size i = 0; i < times_.size(); ++i)
            res += pow(annualisedModelVariance(times_[i], x) - annualisedMarketVariances_[i], 2);
        return res;
    }

    // difference per time point
    Array values(const Array& x) const override {
        QL_REQUIRE(x.size() == 3, "3 parameters expected");
        Array values(times_.size());
        for (Size i = 0; i < times_.size(); ++i)
            values[i] = annualisedModelVariance(times_[i], x) - annualisedMarketVariances_[i];
        return values;
    }

private:
    std::vector<Real> times_;
    std::vector<Real> annualisedMarketVariances_;
    Array initialValues_;
    std::vector<bool> fixed_;
};

std::vector<Real> HestonModelCalibration::buildHelpers(const QuantLib::ext::shared_ptr<PricingEngine>& engine) {
    Handle<Quote> s0 = process_->stateVariable();
    Handle<YieldTermStructure> yts = process_->riskFreeRate();
    Handle<YieldTermStructure> dts = process_->dividendYield();
    Handle<BlackVolTermStructure> vts = process_->blackVolatility();
    std::vector<Real> moneyness = moneyness_.size() > 0 ? moneyness_ : std::vector<Real>(1, 0.0);
    QL_REQUIRE(expiries_.size() > 0, "no option expiries given");

    DayCounter dc = vts->dayCounter();
    Calendar cal = vts->calendar();

    // FIXME: Expose the error type ?
    // BlackCalibrationHelper::CalibrationErrorType errorType = BlackCalibrationHelper::RelativePriceError;
    // BlackCalibrationHelper::CalibrationErrorType errorType = BlackCalibrationHelper::PriceError;
    BlackCalibrationHelper::CalibrationErrorType errorType = BlackCalibrationHelper::ImpliedVolError;
    helpers_.clear();

    for (auto& optionMaturity : expiries_) {

        DLOG("option expiry " << optionMaturity);
        Date mat = cal.advance(yts->referenceDate(), optionMaturity);
        const Time tau = dc.yearFraction(yts->referenceDate(), mat);
        // FIXME: Check that we have forward prices consistent with the option surface (put-call partiy)
        const Real fwdPrice = atmForward(s0->value(), yts, dts, tau);
        const Real atmVol = std::max(1e-4, vts->blackVol(tau, fwdPrice));
        DLOG("atm: strike " << fwdPrice << " vol " << atmVol);

        for (auto& m : moneyness) {
            const Real strikePrice = fwdPrice * std::exp(-m * atmVol * std::sqrt(tau));
            Handle<Quote> vol(QuantLib::ext::make_shared<SimpleQuote>(vts->blackVol(mat, strikePrice, true)));
            auto helper = QuantLib::ext::make_shared<HestonModelHelper>(optionMaturity, cal, s0, strikePrice, vol, yts,
                                                                        dts, errorType);
            helper->setPricingEngine(engine);
            helpers_.push_back(helper);
            DLOG("added helper: strike " << strikePrice << " vol " << vol->value());
        }
    }

    return moneyness;
}


void HestonModelCalibration::buildExpectedVariances() {
    varianceTimes_.clear();
    annualisedVariances_.clear();

    Handle<BlackVolTermStructure> vts = process_->blackVolatility();
    DayCounter dc = vts->dayCounter();
    Calendar cal = vts->calendar();
    Real current, previous;
    auto varianceCalculator = QuantLib::ext::make_shared<VarianceCalculator>(process_);
    for (auto p : varianceTerms_) {
        Date mat = cal.advance(vts->referenceDate(), p);
        Time t = dc.yearFraction(vts->referenceDate(), mat);
        Real v = varianceCalculator->futureVariance(mat);
	varianceTimes_.push_back(t);
        annualisedVariances_.push_back(v);
        DLOG("annualised variance at " << p << " : " << v << " total " << v * t);
        current = v * t;
        if (p != varianceTerms_.front() && current <= previous) {
            ALOG("arbitrage at " << p << " current " << current << " <= previous " << previous);
        }
        previous = current;
    }
}

void HestonModelCalibration::logCalibration(const std::vector<Real>& moneyness) {
    Real rmse = 0;
    Size count = 0;
    DLOG("helperCount,expiry,moneyness,error,marketValue,modelValue,diffValue,marketVol,modelVol");
    for (Size j = 0; j < expiries_.size(); j++) {
        for (Size k = 0; k < moneyness.size(); k++) {
            auto option = QuantLib::ext::dynamic_pointer_cast<BlackCalibrationHelper>(helpers_[count]);
            const Real diff = option->calibrationError();
            rmse += diff * diff;
            Real mktValue = option->marketValue();
            Real modelValue = option->modelValue();
            Real marketVol = option->volatility()->value();
            Real modelVol = option->impliedVolatility(modelValue, 1e-4, 1000, 1e-4, 3.0);
            DLOG(count + 1 << "/" << helpers_.size() << "," << expiries_[j] << ","
                                          << moneyness[k] << "," << diff << "," << mktValue << "," << modelValue << ","
                                          << modelValue - mktValue << "," << marketVol << "," << modelVol);
            count++;
        }
    }
    rmse = sqrt(rmse / helpers_.size());

    DLOG("rmse: " << rmse);
}

QuantLib::ext::shared_ptr<HestonModel> HestonModelCalibration::model() {
    if (restarts_ > 0)
        return model2();
    else
        return model1();
}

QuantLib::ext::shared_ptr<HestonModel> HestonModelCalibration::model1() {

    DLOG("model1 called");
    DLOG("dontCalibrate: " << dontCalibrate_);
    DLOG("initial values: " << to_string(initialValues_));

    QL_REQUIRE(initialValues_.size() == 5, "5 initial values expected, found " << initialValues_.size());

    Real theta = initialValues_[0];
    Real kappa = initialValues_[1];
    Real sigma = initialValues_[2];
    Real rho = initialValues_[3];
    Real v0 = initialValues_[4];

    if (dontCalibrate_) {
        auto hestonProcess =
            QuantLib::ext::make_shared<HestonProcess>(process_->riskFreeRate(), process_->dividendYield(),
                                                      process_->stateVariable(), v0, kappa, theta, sigma, rho);
        return QuantLib::ext::make_shared<HestonModel>(hestonProcess);
    }

    /**********************************************************************
     * 1. Calibrate theta, kappa and v0 to the market variance curve
     *    New initial values for theta and v0 taken from the variance curve
     **********************************************************************/

    // Compute annualised variances and check no-arbitrage (increasing variance)
    buildExpectedVariances();

    theta = annualisedVariances_.back();
    v0 = annualisedVariances_.front();
    
    bool fixTheta = true;   // FIXME: Keep theta fixed to find reasonable kappa?
    bool fixKappa = false;
    bool fixVo = false;
    HestonVarianceCostFunction cost(varianceTimes_, annualisedVariances_, theta, kappa, v0, fixTheta, fixKappa, fixVo);
    Constraint constraint = PositiveConstraint(); 
    Problem problem(cost, constraint, cost.initialValues());
    LevenbergMarquardt method;
    EndCriteria endCriteria(1000, 100, 1e-8, 1e-8, 1e-8);

    DLOG("variance curve fit, initial: " << problem.currentValue());
    try {
        method.minimize(problem, endCriteria);
    } catch (std::exception& e) {
        ALOG("variance curve error: " << e.what());
    }
    DLOG("variance curve fit, final:   " << problem.currentValue());

    Array result = problem.currentValue();
    for (Size i = 0; i < varianceTimes_.size(); ++i) {
        DLOG("variance curve: " << varianceTimes_[i] << " " << annualisedVariances_[i] << " "
                                << cost.annualisedModelVariance(varianceTimes_[i], result));
    }

    /****************************************************************************
     * 2. Update initial values
     *    Check that sigma's initial value is valid (Feller), otherwise adjust it
     ****************************************************************************/
    theta = result[0];
    kappa = result[1];
    v0 = result[2];
    DLOG("theta, kappa, sigma_max: " << theta << " " << kappa << " " << sqrt(2.0 * kappa * theta));

    // If initial sigma does not satisfy the feller condition, then modify it
    // FIXME: If we use the relaxed feller constraint below, should we use a larger starting point?
    if (2.0 * kappa * theta < sigma * sigma) {
        sigma = 0.5 * sqrt(2.0 * kappa * theta);
        WLOG("update sigma to satisfy Feller: " << initialValues_[2] << " -> " << sigma);
    }
    
    /**********************************************************
     * 3. Calibrate the full model to the Equity option surface
     **********************************************************/
    Handle<Quote> s0 = process_->stateVariable();
    Handle<YieldTermStructure> yts = process_->riskFreeRate();
    Handle<YieldTermStructure> dts = process_->dividendYield();

    auto hestonProcess = QuantLib::ext::make_shared<HestonProcess>(yts, dts, s0, v0, kappa, theta, sigma, rho);
    auto hestonModel = QuantLib::ext::make_shared<HestonModel>(hestonProcess);
    auto hestonEngine = QuantLib::ext::make_shared<AnalyticHestonEngine>(hestonModel, 64);
    // auto hestonEngine = QuantLib::ext::make_shared<COSHestonEngine>(hestonModel, 12, 75);
    // auto hestonEngine = QuantLib::ext::make_shared<ExponentialFittingHestonEngine>(hestonModel);
    Constraint fellerConstraint = HestonModel::FellerConstraint();
    //Constraint hestonConstraint = NoConstraint();
    //Constraint hestonConstraint = fellerConstraint;
    Constraint hestonConstraint = RelaxedFellerConstraint(relaxedFellerConstraint_);
    LevenbergMarquardt om; 
    EndCriteria hestonEndCriteria(400, 40, 1.0e-8, 1.0e-8, 1.0e-8);
    std::vector<Real> weights;

    std::vector<Real> moneyness = buildHelpers(hestonEngine);

    DLOG("heston initial: " << hestonModel->params());
    try {
        hestonModel->calibrate(helpers_, om, hestonEndCriteria, hestonConstraint, weights, fixedValues_);
    } catch (std::exception& e) {
        ALOG("heston calibration error: " << e.what());
    }
    DLOG("heston final: " << hestonModel->params());
    Real feller = 2.0 * hestonModel->params()[0] * hestonModel->params()[1] / pow(hestonModel->params()[2], 2);
    DLOG("feller: " << feller);
    if (feller < 1) {
        ALOG("feller constraint violated");
    }

    logCalibration(moneyness);

    return hestonModel;
}

Real HestonModelCalibration::getRMSE() {
    Real rmse = 0.0;
    for (Size i = 0; i < helpers_.size(); ++i) {
        auto option = QuantLib::ext::dynamic_pointer_cast<BlackCalibrationHelper>(helpers_[i]);
        Real diff = option->calibrationError();
        rmse += diff * diff;
    }
    rmse = std::sqrt(rmse / helpers_.size());
    return rmse;
}

QuantLib::ext::shared_ptr<HestonModel> HestonModelCalibration::model2() {

    DLOG("model2 called");

    QL_REQUIRE(initialValues_.size() == 5, "5 initial values expected, found " << initialValues_.size());

    Real theta = initialValues_[0];
    Real kappa = initialValues_[1];
    Real sigma = initialValues_[2];
    Real rho = initialValues_[3];
    Real v0 = initialValues_[4];

    if (dontCalibrate_) {
        // with default HestonProcess::Discretization
        auto hestonProcess =
            QuantLib::ext::make_shared<HestonProcess>(process_->riskFreeRate(), process_->dividendYield(),
                                                      process_->stateVariable(), v0, kappa, theta, sigma, rho);
        return QuantLib::ext::make_shared<HestonModel>(hestonProcess);
    }

    // FIXME: Move parameters to constructor
    int restarts = restarts_; 
    int seed = 42;

    Handle<Quote> s0 = process_->stateVariable();
    Handle<YieldTermStructure> yts = process_->riskFreeRate();
    Handle<YieldTermStructure> dts = process_->dividendYield();

    auto hestonProcess = QuantLib::ext::make_shared<HestonProcess>(yts, dts, s0, v0, kappa, theta, sigma, rho);
    auto model = QuantLib::ext::make_shared<HestonModel>(hestonProcess);
    auto hestonEngine = QuantLib::ext::make_shared<AnalyticHestonEngine>(model, 64);
    // auto hestonEngine = QuantLib::ext::make_shared<COSHestonEngine>(model, 12, 75);
    // auto hestonEngine = QuantLib::ext::make_shared<ExponentialFittingHestonEngine>(model);
    Constraint fellerConstraint = HestonModel::FellerConstraint();
    //Constraint constraint = fellerConstraint;
    //Constraint constraint = NoConstraint();
    Constraint constraint = RelaxedFellerConstraint(relaxedFellerConstraint_);
    LevenbergMarquardt method; 
    EndCriteria hestonEndCriteria(400, 40, 1.0e-8, 1.0e-8, 1.0e-8);

    std::vector<Real> moneyness = buildHelpers(hestonEngine);

    Array bestParams(5);
    Real bestRmse = QL_MAX_REAL;
    HaltonRsg halton(4, seed);
    do {
        // DLOG("starting with parameters " << model->params() << " (" << restarts << " restarts to go)");
        Real rmse = QL_MAX_REAL;
        try {
            model->calibrate(helpers_, method, hestonEndCriteria, constraint);
        } catch (std::exception& e) {
            ALOG("heston calibration error: " << e.what());
        }
        rmse = getRMSE();
        if (rmse < bestRmse) {
            bestRmse = rmse;
            bestParams = model->params();
        }

        DLOG("rmse = " << rmse << " params = " << model->params() << " (" << restarts << " restarts to go)");

        // if below acceptTolerance, no more restarts
        if (rmse < tolerance_)
            break;
        // new random initial parameters
        if (restarts > 0) {
            Array newParams(5);
            std::vector<Real> sample = halton.nextSequence().value;
	    // FIXME: Expose the parameter ranges for theta, kappa, sigma and rho
            newParams[0] = 0.0001 + 0.1 * sample[0];                                         // theta
            newParams[1] = 0.0001 + 20.0 * sample[1];                                        // kappa
            newParams[2] = std::sqrt(newParams[1] * newParams[0] / (0.5 + 1.0 * sample[2])); // sigma
            newParams[3] = -0.9 + 1.8 * sample[3];                                           // rho
            newParams[4] = newParams[0];                                                     // v0
            model->setParams(newParams);
        }
    } while (restarts-- > 0);

    model->setParams(bestParams);

    DLOG("best rmse   = " << bestRmse);
    DLOG("best params = " << bestParams);
    Real feller = 2.0 * bestParams[0] * bestParams[1] / pow(bestParams[2], 2);
    DLOG("feller: " << feller);
    if (feller < 1) {
        ALOG("feller constraint violated");
    }

    DLOG("Model Calibration Results:");
    DLOG("theta : " << model->theta());
    DLOG("kappa : " << model->kappa());
    DLOG("sigma : " << model->sigma());
    DLOG("rho   : " << model->rho());
    DLOG("v0    : " << model->v0());

    logCalibration(moneyness);

    return model;
}

} // namespace data
} // namespace ore
