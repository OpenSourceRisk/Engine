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
#include <ored/model/structuredmodelwarning.hpp>
#include <ored/utilities/dategrid.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/to_string.hpp>
#include <ored/utilities/parsers.hpp>

#include <ql/exercise.hpp>
#include <ql/instruments/payoffs.hpp>
#include <ql/instruments/vanillaoption.hpp>
#include <ql/math/optimization/differentialevolution.hpp>
#include <ql/math/optimization/levenbergmarquardt.hpp>
#include <ql/math/randomnumbers/haltonrsg.hpp>
#include <ql/models/equity/hestonmodel.hpp>
#include <ql/models/equity/hestonmodelhelper.hpp>
#include <ql/pricingengines/vanilla/analyticeuropeanengine.hpp>
#include <ql/pricingengines/vanilla/analytichestonengine.hpp>
#include <ql/pricingengines/vanilla/analyticptdhestonengine.hpp>
#include <ql/pricingengines/vanilla/exponentialfittinghestonengine.hpp>
#include <ql/pricingengines/vanilla/fdblackscholesvanillaengine.hpp>
#include <ql/pricingengines/vanilla/fdhestonvanillaengine.hpp>
#include <ql/processes/hestonprocess.hpp>
#include <ql/processes/hestonslvprocess.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/time/daycounters/actualactual.hpp>


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

void HestonModelCalibration::buildHelpers(const QuantLib::ext::shared_ptr<PricingEngine>& engine) {

    Handle<Quote> s0 = process_->stateVariable();
    Handle<YieldTermStructure> yts = process_->riskFreeRate();
    Handle<YieldTermStructure> dts = process_->dividendYield();
    Handle<BlackVolTermStructure> vts = process_->blackVolatility();
    QL_REQUIRE(moneyness_.size() > 0, "non-zero moneyness vector size required");
    DayCounter dc = vts->dayCounter();
    // The NullCalendar is needed below to convert Periods in Days into Dates
    Calendar cal = NullCalendar(); 
    Date refDate = yts->referenceDate();
    
    expiryDates_.clear();
    if (expiries_.size() > 0) {
        // explicit expiries provided: use these to compile expiry date vector
        for (auto e : expiries_)
            expiryDates_.push_back(cal.advance(refDate, e));
    } else {
        // explicit expiries missing: use effective simulation dates
        // FIXME: This may need to be cut down, e.g. imagine daily observation/simulation dates in a barrier option
        for (auto d : effectiveSimulationDates_)
            if (d > refDate)
                expiryDates_.push_back(d);
    }

    // FIXME: Expose the error type ?
    // BlackCalibrationHelper::CalibrationErrorType errorType = BlackCalibrationHelper::RelativePriceError;
    // BlackCalibrationHelper::CalibrationErrorType errorType = BlackCalibrationHelper::PriceError;
    BlackCalibrationHelper::CalibrationErrorType errorType = BlackCalibrationHelper::ImpliedVolError;
    // FIXME: Make this a script engine parameter ?
    Real premiumThreshold = 0.0; 
    helpers_.clear();
    weights_.clear();
    strikes_.clear();
    // for (const auto& optionMaturity : expiries_) {
    for (const auto& mat : expiryDates_) {
        Period period = (mat - refDate) * Days;
	// double check that this will be interpreted correctly by the HestonModelHelper
	Date date = cal.advance(refDate, period);
	QL_REQUIRE(mat == date, "date mismatch " << io::iso_date(mat) << " vs " << io::iso_date(date));
	const Time tau = dc.yearFraction(yts->referenceDate(), mat);
        DLOG("option expiry " << tau);
        // FIXME: Check that we have forward prices consistent with the option surface
	// i.e. that dts has been build from options using put-call parity. 
        const Real fwdPrice = atmForward(s0->value(), yts, dts, tau);
        const Real atmVol = std::max(1e-4, vts->blackVol(tau, fwdPrice));
        DLOG("atm: strike " << fwdPrice << " vol " << atmVol);
        for (const auto& m : moneyness_) {
            const Real strikePrice = fwdPrice * std::exp(-m * atmVol * std::sqrt(tau));
            Handle<Quote> vol(QuantLib::ext::make_shared<SimpleQuote>(vts->blackVol(mat, strikePrice, true)));
            auto helper = QuantLib::ext::make_shared<HestonModelHelper>(period, cal, s0, strikePrice, vol, yts,
                                                                        dts, errorType);
            helper->setPricingEngine(engine);
            helpers_.push_back(helper);
	    strikes_.push_back(strikePrice);
            weights_.push_back(helper->marketValue() > premiumThreshold ? 1.0 : 0.0);
            DLOG("added helper: expiry " << tau << " strike " << strikePrice << " vol " << vol->value() << " price "
                                         << helper->marketValue() << " weight " << weights_.back());
        }
    }
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

void HestonModelCalibration::logCalibration(AssetModelCalibrationResults& results) {
    results.clear();
    Size count = 0;
    DLOG("helperCount,expiry,moneyness,strike,error,marketValue,modelValue,diffValue,marketVol,modelVol");
    for (Size j = 0; j < expiries_.size(); j++) {
        for (Size k = 0; k < moneyness_.size(); k++) {
            auto option = QuantLib::ext::dynamic_pointer_cast<BlackCalibrationHelper>(helpers_[count]);
            const Real err = option->calibrationError();
            results.rmse += err * err;
	    AssetModelCalibrationResults::InstrumentResults result;
            result.expiry = expiries_[j];
            result.moneyness = moneyness_[k];
            result.strike = strikes_[moneyness_.size() * j + k];
            result.marketValue = option->marketValue();
            result.modelValue = option->modelValue();
            result.marketVol = option->volatility()->value();
            result.modelVol =
                option->impliedVolatility(result.modelValue, 1e-8, 1000, 1e-4, 3.0); // FIXME: expose parameters?
            DLOG(++count << "/" << helpers_.size() << "," << result.expiry << "," << result.moneyness << ","
                         << result.strike << "," << err << "," << result.marketValue << "," << result.modelValue << ","
                         << result.modelValue - result.marketValue << "," << result.marketVol << "," << result.modelVol
                         << "," << result.marketVol - result.modelVol);
            results.data.push_back(result);
        }
    }
    results.rmse = sqrt(results.rmse / helpers_.size());

    DLOG("rmse: " << results.rmse);
}

QuantLib::ext::shared_ptr<HestonModel> HestonModelCalibration::model() {
    DLOG("dontCalibrate: " << dontCalibrate_);
    DLOG("initial values: " << to_string(initialValues_));
    DLOG("fixed values: " << to_string(fixedValues_));
    DLOG("discretization: " << to_string(discretization_));

    results_.clear();
    QuantLib::ext::shared_ptr<HestonModel> model;

    QL_REQUIRE(initialValues_.size() == 5, "5 initial values expected, found " << initialValues_.size());

    Real theta = initialValues_[0];
    Real kappa = initialValues_[1];
    Real sigma = initialValues_[2];
    Real rho = initialValues_[3];
    Real v0 = initialValues_[4];    
    bool allFixed = fixedValues_[0] && fixedValues_[1] && fixedValues_[2] && fixedValues_[3] && fixedValues_[4];
    
    if (dontCalibrate_ || allFixed) {
        auto hestonProcess = QuantLib::ext::make_shared<HestonProcess>(
            process_->riskFreeRate(), process_->dividendYield(), process_->stateVariable(), v0, kappa, theta, sigma,
            rho, discretization_);
        model = QuantLib::ext::make_shared<HestonModel>(hestonProcess);
    } else {
        if (maxCalibrationAttempts_ > 0)
            model = model2();
        else
            model = model1();
    }
    results_.indexName = indexName_;

    results_.constantParameters.clear();
    results_.constantParameters.push_back(std::make_pair("theta", model->theta()));
    results_.constantParameters.push_back(std::make_pair("kappa", model->kappa()));
    results_.constantParameters.push_back(std::make_pair("sigma", model->sigma()));
    results_.constantParameters.push_back(std::make_pair("rho", model->rho()));
    results_.constantParameters.push_back(std::make_pair("v0", model->v0()));
    Real feller = 2.0 * model->theta() * model->kappa() / pow(model->sigma(), 2);
    results_.constantParameters.push_back(std::make_pair("feller", feller));

    DLOG("Model Calibration Results:");
    DLOG("theta  : " << model->theta());
    DLOG("kappa  : " << model->kappa());
    DLOG("sigma  : " << model->sigma());
    DLOG("rho    : " << model->rho());
    DLOG("v0     : " << model->v0());
    DLOG("Feller : " << feller);

    if (feller < 1) {
        ALOG("Feller constraint violated: 2*theta*kappa/sigma^2 = " << feller);
    }

    return model;
}

QuantLib::ext::shared_ptr<HestonModel> HestonModelCalibration::model1() {
    DLOG("model1 called");

    Real theta = initialValues_[0];
    Real kappa = initialValues_[1];
    Real sigma = initialValues_[2];
    Real rho = initialValues_[3];
    Real v0 = initialValues_[4];

    if (varianceTerms_.size() > 0) {
        try {
            /**********************************************************************
             * 1. Calibrate theta, kappa and v0 to the market variance curve
             *    New initial values for theta and v0 taken from the variance curve
             **********************************************************************/

            // Compute annualised variances and check no-arbitrage (increasing variance)
            buildExpectedVariances();

            theta = annualisedVariances_.back();
            v0 = annualisedVariances_.front();

            bool fixTheta = true; // Keep theta fixed to find reasonable kappa
            bool fixKappa = fixedValues_[1];
            bool fixVo = fixedValues_[4];
            HestonVarianceCostFunction cost(varianceTimes_, annualisedVariances_, theta, kappa, v0, fixTheta, fixKappa,
                                            fixVo);
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
            theta = fixedValues_[0] ? initialValues_[0] : result[0];
            kappa = fixedValues_[1] ? initialValues_[1] : result[1];
            v0 = fixedValues_[4] ? initialValues_[4] : result[2];
            DLOG("theta, kappa, sigma_max: " << theta << " " << kappa << " " << sqrt(2.0 * kappa * theta));

            // If initial sigma does not satisfy the (relaxed) feller condition, then modify it
            if (fixedValues_[2] == false && 2.0 * kappa * theta / (sigma * sigma) < relaxedFellerConstraint_) {
                sigma = 0.5 * sqrt(2.0 * kappa * theta / relaxedFellerConstraint_);
                WLOG("update sigma to satisfy Feller: " << initialValues_[2] << " -> " << sigma);
            }

        } catch (std::exception& e) {
            ALOG("error adjusting heston initial values: " << e.what() << ", continue with initial values");
            theta = initialValues_[0];
            kappa = initialValues_[1];
            sigma = initialValues_[2];
            rho = initialValues_[3];
            v0 = initialValues_[4];
        }
    } else {
        WLOG("no variance terms proided, skip the variance curve fit");
    }

    /**********************************************************
     * 3. Calibrate the full model to the Equity option surface
     **********************************************************/
    Handle<Quote> s0 = process_->stateVariable();
    Handle<YieldTermStructure> yts = process_->riskFreeRate();
    Handle<YieldTermStructure> dts = process_->dividendYield();

    auto hestonProcess =
        QuantLib::ext::make_shared<HestonProcess>(yts, dts, s0, v0, kappa, theta, sigma, rho, discretization_);
    auto hestonModel = QuantLib::ext::make_shared<HestonModel>(hestonProcess);
    auto hestonEngine = QuantLib::ext::make_shared<AnalyticHestonEngine>(hestonModel, 64);
    // auto hestonEngine = QuantLib::ext::make_shared<COSHestonEngine>(hestonModel, 12, 75);
    // auto hestonEngine = QuantLib::ext::make_shared<ExponentialFittingHestonEngine>(hestonModel);
    Constraint fellerConstraint = HestonModel::FellerConstraint();
    // Constraint hestonConstraint = NoConstraint();
    // Constraint hestonConstraint = fellerConstraint;
    Constraint hestonConstraint = RelaxedFellerConstraint(relaxedFellerConstraint_);
    LevenbergMarquardt om;
    EndCriteria hestonEndCriteria(400, 40, 1.0e-8, 1.0e-8, 1.0e-8);
    std::vector<Real> weights;

    buildHelpers(hestonEngine);

    DLOG("heston initial: " << hestonModel->params());
    try {
        hestonModel->calibrate(helpers_, om, hestonEndCriteria, hestonConstraint, weights, fixedValues_);
    } catch (std::exception& e) {
        ALOG("heston calibration error: " << e.what());
    }
    DLOG("heston final: " << hestonModel->params());

    logCalibration(results_);

    Real rmse = getRMSE();
    if (rmse > maxAcceptableError_) {
        std::string msg = "Heston calibration error " + to_string(rmse) + " exceeds maximum acceptable error " +
                          to_string(maxAcceptableError_);
        StructuredModelWarningMessage("Failed to calibrate Heston model", msg, indexName_).log();
    }

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

    DLOG("HestonModelCalibration::model2() called");

    Real theta = initialValues_[0];
    Real kappa = initialValues_[1];
    Real sigma = initialValues_[2];
    Real rho = initialValues_[3];
    Real v0 = initialValues_[4];

    int restarts = maxCalibrationAttempts_;
    // FIXME: Move to configuration
    int seed = 42;

    Handle<Quote> s0 = process_->stateVariable();
    Handle<YieldTermStructure> yts = process_->riskFreeRate();
    Handle<YieldTermStructure> dts = process_->dividendYield();

    auto hestonProcess =
        QuantLib::ext::make_shared<HestonProcess>(yts, dts, s0, v0, kappa, theta, sigma, rho, discretization_);
    auto model = QuantLib::ext::make_shared<HestonModel>(hestonProcess);
    auto hestonEngine = QuantLib::ext::make_shared<AnalyticHestonEngine>(model, 64);
    // auto hestonEngine = QuantLib::ext::make_shared<COSHestonEngine>(model, 12, 75);
    // auto hestonEngine = QuantLib::ext::make_shared<ExponentialFittingHestonEngine>(model);
    // Constraint constraint = HestonModel::FellerConstraint();
    // Constraint constraint = NoConstraint();
    Constraint constraint = RelaxedFellerConstraint(relaxedFellerConstraint_);
    LevenbergMarquardt method;
    EndCriteria hestonEndCriteria(400, 40, 1.0e-8, 1.0e-8, 1.0e-8);

    buildHelpers(hestonEngine);

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
        if (rmse < earlyExitThreshold_)
            break;
        // new random initial parameters
        if (restarts > 0) {
            Array newParams(5);
            std::vector<Real> sample = halton.nextSequence().value;
            // FIXME: Expose the parameter ranges for theta, kappa, sigma and rho
            // newParams[0] = 0.0001 + 0.1 * sample[0];                                         // theta
            // newParams[1] = 0.0001 + 20.0 * sample[1];                                        // kappa
            // newParams[2] = std::sqrt(newParams[1] * newParams[0] / (0.5 + 1.0 * sample[2])); // sigma
            // newParams[3] = -0.9 + 1.8 * sample[3];                                           // rho
            // newParams[4] = newParams[0];                                                     // v0
            // model->setParams(newParams);
            newParams[0] = 0.0001 + maximumInitialValues_[0] * sample[0];                        // theta
            newParams[1] = 0.0001 + maximumInitialValues_[1] * sample[1];                        // kappa
            if (relaxedFellerConstraint_ < 0.01) {
                newParams[2] = std::sqrt(newParams[1] * newParams[0] / (0.5 + 1.0 * sample[2])); // sigma
            } else {
                Real epsilon = relaxedFellerConstraint_ + maximumInitialValues_[2] * sample[2];
                // 2 kappa theta / sigma^2 = epsilon > feller, sigma^2 = 2 kappa theta / epsilon
                newParams[2] = std::sqrt(2.0 * newParams[1] * newParams[0] / epsilon);           // sigma
            }
            newParams[3] = -maximumInitialValues_[3] + 2 * maximumInitialValues_[3] * sample[3]; // rho
            // FIXME: equal to theta or random?
	    //newParams[4] = 0.0001 + maximumInitialValues_[0] * sample[4];                      // v0
            newParams[4] = newParams[0];                                                         // v0
            model->setParams(newParams);
        }
    } while (restarts-- > 0);

    model->setParams(bestParams);

    DLOG("best rmse   = " << bestRmse);
    DLOG("best params = " << bestParams);

    logCalibration(results_);

    // FIXME: pass and use continueOnError flag
    if (bestRmse > maxAcceptableError_) {
        std::string msg = "Heston calibration error " + to_string(bestRmse) + " exceeds maximum acceptable error " +
                          to_string(maxAcceptableError_);
        StructuredModelWarningMessage("Failed to calibrate Heston model", msg, indexName_).log();
    }

    return model;
}

ext::shared_ptr<PiecewiseTimeDependentHestonModel> HestonModelCalibration::ptdModel(const ext::shared_ptr<HestonModel>& m) {

    DLOG("HestonModelCalibration::ptdModel called");

    Handle<Quote> s0 = process_->stateVariable();
    Handle<YieldTermStructure> yts = process_->riskFreeRate();
    Handle<YieldTermStructure> dts = process_->dividendYield();

    // The times vector corresponds to the calibration option expiries
    std::vector<Real> times;
    for (auto d : expiryDates_) {
        Real t = yts->timeFromReference(d);
        if (t > 0.0)
            times.push_back(t);
    }

    // TimeGrid inserts the t = 0 point
    TimeGrid timeGrid(times.begin(), times.end());

    DLOG("times:    size " << times.size() << " start " << times.front() << " end " << times.back());
    DLOG("timeGrid: size " << timeGrid.size() << " start " << timeGrid.front() << " end " << timeGrid.back());

    // Piecewise parameters, with previous best fit of constant parameters as initial values
    PiecewiseConstantParameter ptdTheta(times, PositiveConstraint());
    PiecewiseConstantParameter ptdKappa(times, PositiveConstraint());
    PiecewiseConstantParameter ptdSigma(times, PositiveConstraint());
    PiecewiseConstantParameter ptdRho(times, BoundaryConstraint(-1.0, 1.0));

    // The PiecewiseConstantParameter's array has an additional point
    // times: 0  <  t(0)  <  t(1)  <  t(2)  <  ....  <  t(n)
    // params:      a(0)     a(1)     a(2)    ....      a(n)     a(n+1)
    //
    // Evaluation of the PiecewiseConstantParameter:
    // p(t) = a(0)   for t < t(0)
    //        a(1)   for t(0) <= t < t(1)
    //        a(2)   for t(1) <= t < t(2)
    //        ...
    //        a(n)   for t(n-1) <= t < t(n)
    //        a(n+1) for t(n) <= t
    
    for (Size i = 0; i <= times.size(); ++i) {
        ptdTheta.setParam(i, m->theta());
        ptdKappa.setParam(i, m->kappa());
        ptdSigma.setParam(i, m->sigma());
        ptdRho.setParam(i, m->rho());
    }

    DLOG("PiecewiseConstantParameters initialised");

    boost::shared_ptr<PiecewiseTimeDependentHestonModel> ptdModel =
        boost::make_shared<PiecewiseTimeDependentHestonModel>(yts, dts, s0, m->v0(), ptdTheta, ptdKappa,
                                                              ptdSigma, ptdRho, timeGrid);
    boost::shared_ptr<PricingEngine> ptdEngine = boost::make_shared<AnalyticPTDHestonEngine>(
        boost::static_pointer_cast<PiecewiseTimeDependentHestonModel>(ptdModel));

    DLOG("Piecewise Heston Model and Engine constructed");

    for (Size i = 0; i < helpers_.size(); ++i) {
        auto h = ext::dynamic_pointer_cast<HestonModelHelper>(helpers_[i]);
	QL_REQUIRE(h, "cast to HestonModelHelper failed for i=" << i); 
	h->setPricingEngine(ptdEngine);
    }

    DLOG("Helpers updated");

    LevenbergMarquardt method;
    EndCriteria endCriteria(5000, 100, 1E-8, 1E-8, 1E-8);
    // FIXME: Force relaxed Feller constraint with time-dependent sigma on each segment ?
    Constraint constraint = Constraint(); 

    if (calibrationMethod_ == "PiecewiseBootstrap") {
        // Bootstrap piecewise parameters (sigma and rho) on each smile section's helpers.
        // Parameters theta, kappa and v0 are not changed and constant through time.
        DLOG("PiecewiseBootstrap selected");
        for (Size i = 0; i < times.size(); ++i) {
	    DLOG("expiry time " << i << ": " << times[i]);
            std::vector<boost::shared_ptr<CalibrationHelper>> localHelper;
            std::vector<Real> localWeights;
            Size nonZeroInstr = 0;
            for (Size k = 0; k < moneyness_.size(); ++k) {
	        Size idx = moneyness_.size() * i + k;
                QL_REQUIRE(idx < helpers_.size(), "index " << idx << " out of range");
	        localHelper.push_back(helpers_[idx]);
		auto helper = QuantLib::ext::dynamic_pointer_cast<HestonModelHelper>(helpers_[idx]);
		QL_REQUIRE(helper, "dynamic cast to HestonModelHelper failed");
                DLOG("time[" << i << "]=" << times[i]
		     << " strike[" << k << "]=" << strikes_[idx]
		     << " maturity " << helper->maturity());
                localWeights.push_back(weights_[idx]);
                if (!close_enough(localWeights.back(), 0.0))
		    ++nonZeroInstr;
            }
            // Calibrate sigma / rho only
            std::vector<bool> fixed(1 + 4 * (times.size() + 1), true);
            fixed[i + (times.size() + 1) * 2] = false; // vary sigma
            fixed[i + (times.size() + 1) * 3] = false; // vary rho
            if (nonZeroInstr >= 2) {
                DLOG("calibrate smile section at time " << times[i]);
                ptdModel->calibrate(localHelper, method, endCriteria, constraint, localWeights, fixed);
            } else {
                ALOG("not enough helpers to calibrate sigma/rho");
            }
        }
        DLOG("PiecewiseBootstrap done");
    } else if (calibrationMethod_ == "PiecewiseBestFit") {
        DLOG("PiecewiseBestFit selected");
        // Apply a single best fit across all helpers to find the optimal piecewise parameters
        // We fix theta, kappa and v0, i.e. all except sigma and rho, as above.
        std::vector<bool> fixed(1 + 4 * (times.size() + 1), true);
        for (Size i = 0; i < times.size(); ++i) {
            // vary sigma and rho only, as above
            fixed[i + (times.size() + 1) * 2] = false; // vary sigma
            fixed[i + (times.size() + 1) * 3] = false; // vary rho
	}
        // std::vector<bool> fixed(1 + 4 * (times.size() + 1), false);
        // for (Size i = 0; i <= times.size(); ++i) {
        //     // fix theta and kappa
        //     fixed[i + (times.size() + 1) * 0] = true; // theta
        //     fixed[i + (times.size() + 1) * 1] = true; // kappa
        //     // fixed[i + (times.size() + 1) * 2] = false; // sigma
        //     // fixed[i + (times.size() + 1) * 3] = false; // rho
        // }
        ptdModel->calibrate(helpers_, method, endCriteria, constraint, weights_, fixed);
	DLOG("PiecewiseBestFit done");
    } else {
        QL_FAIL("unknown calibration method \"" << calibrationMethod_ << "\"");
    }

    // log to a separate result structure, keep constant parameters results
    logCalibration(piecewiseResults_);

    std::vector<Real> theta(times.size(), 0.0);
    std::vector<Real> kappa(times.size(), 0.0);
    std::vector<Real> sigma(times.size(), 0.0);
    std::vector<Real> rho(times.size(), 0.0);
    std::vector<Real> feller(times.size(), 0.0);

    Real dt = times.size() == 1 ? times.back() : times.back() - times[times.size() - 2];
    // Note:
    // - If there are N expiry times > 0, then we have N material optimization parameters, the last (additional)
    //   piecewise parameter is not used. Why is this parameter array length = number of times + 1 ?
    // - Below we loop over n times, from t(0)=0, ..., t(n-1), i.e. the starting points of each piecewise segment.
    //   Evaluation of a piecewise parameter at these starting points yields the segment's parameter value.
    // - Adding QL_EPSILON is just an extra safety measure, to ensure we evaluate within the segment. In fact, the
    //   piecewise parameter does that correctly already.
    // - See the Analytical PTD Heston engine: Klaus is especially careful, avoids jump times and evaluates the
    //   parameters at segment mid points only. 
    for (Size i = 0; i < times.size(); ++i) {
        Real time = i == 0 ? 0.0 : times[i];
        theta[i] = ptdModel->theta(time + QL_EPSILON);
        kappa[i] = ptdModel->kappa(time + QL_EPSILON);
        sigma[i] = ptdModel->sigma(time + QL_EPSILON);
        rho[i] = ptdModel->rho(time + QL_EPSILON);
	feller[i] = 2.0 * theta[i] * kappa[i] / pow(sigma[i], 2);
        if (feller[i] < 1) {
            ALOG("Feller constraint 2*theta*kappa/sigma^2 violated at time " << time << ": " << feller[i]);
        }

        Real tp = i == 0 ? 0.0 : times[i - 1];
        Real tc = i == times.size() - 1? tp + dt : times[i];
	// just a few steps between time grid points and a few beyond the upper end
	for (Size j = 0; j < 4; ++j) {
            Real time = tp + (tc - tp) * j / 4;
            DLOG("PTD Heston at " << time << ":" 
		 << " theta " << ptdModel->theta(time + QL_EPSILON)
		 << " kappa " << ptdModel->kappa(time + QL_EPSILON)
		 << " sigma " << ptdModel->sigma(time + QL_EPSILON)
		 << " rho " << ptdModel->rho(time + QL_EPSILON));
        }
    }
    Real v0 = ptdModel->v0();

    DLOG("times  : " << to_string(times));
    DLOG("theta  : " << to_string(theta));
    DLOG("kappa  : " << to_string(kappa));
    DLOG("sigma  : " << to_string(sigma));
    DLOG("rho    : " << to_string(rho));
    DLOG("v0     : " << to_string(v0));
    DLOG("Feller : " << to_string(feller));

    piecewiseResults_.indexName = indexName_;
    piecewiseResults_.piecewiseParameters.clear();
    piecewiseResults_.piecewiseParameters.push_back(std::make_pair("times", to_string(times)));
    piecewiseResults_.piecewiseParameters.push_back(std::make_pair("theta", to_string(theta)));
    piecewiseResults_.piecewiseParameters.push_back(std::make_pair("kappa", to_string(kappa)));
    piecewiseResults_.piecewiseParameters.push_back(std::make_pair("sigma", to_string(sigma)));
    piecewiseResults_.piecewiseParameters.push_back(std::make_pair("rho", to_string(rho)));
    piecewiseResults_.piecewiseParameters.push_back(std::make_pair("v0", to_string(v0)));
    piecewiseResults_.piecewiseParameters.push_back(std::make_pair("feller", to_string(feller)));

    std::vector<Real> times2(3);
    times2[0] = 1;
    times2[1] = 2;
    times2[2] = 3;
    PiecewiseConstantParameter pcp(times2);
    pcp.setParam(0, 1.0);
    pcp.setParam(1, 2.0);
    pcp.setParam(2, 3.0);
    pcp.setParam(3, 0.0);
    for (Size i = 0; i <= 60; ++i) {
      Real t = 5.0 * i / 60;
      DLOG("PCP " << i << " " << t << " " << pcp(t-QL_EPSILON));
      DLOG("PCP " << i << " " << t << " " << pcp(t));
      DLOG("PCP " << i << " " << t << " " << pcp(t+QL_EPSILON));
    }
    
    return ptdModel;
}

} // namespace data
} // namespace ore
