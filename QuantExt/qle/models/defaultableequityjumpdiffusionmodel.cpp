/*
 Copyright (C) 2021 Quaternion Risk Management Ltd.

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

#include <qle/methods/fdmdefaultableequityjumpdiffusionfokkerplanckop.hpp>
#include <qle/models/defaultableequityjumpdiffusionmodel.hpp>

#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/math/optimization/levenbergmarquardt.hpp>
#include <ql/methods/finitedifferences/meshers/concentrating1dmesher.hpp>
#include <ql/methods/finitedifferences/meshers/fdmmeshercomposite.hpp>
#include <ql/methods/finitedifferences/meshers/uniform1dmesher.hpp>
#include <ql/methods/finitedifferences/solvers/fdmbackwardsolver.hpp>
#include <ql/pricingengines/blackformula.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <ql/timegrid.hpp>

namespace QuantExt {

using namespace QuantLib;

DefaultableEquityJumpDiffusionModelBuilder::DefaultableEquityJumpDiffusionModelBuilder(
    const std::vector<Real>& stepTimes, const QuantLib::ext::shared_ptr<QuantExt::EquityIndex2>& equity,
    const Handle<BlackVolTermStructure>& volatility, const Handle<DefaultProbabilityTermStructure>& creditCurve,
    const Real p, const Real eta, const bool staticMesher, const Size timeStepsPerYear, const Size stateGridPoints,
    const Real mesherEpsilon, const Real mesherScaling, const Real mesherConcentration,
    const BootstrapMode bootstrapMode, const bool enforceFokkerPlanckBootstrap, const bool calibrate,
    const bool adjustEquityVolatility, const bool adjustEquityForward)
    : stepTimes_(stepTimes), equity_(equity), volatility_(volatility), creditCurve_(creditCurve), p_(p), eta_(eta),
      staticMesher_(staticMesher), timeStepsPerYear_(timeStepsPerYear), stateGridPoints_(stateGridPoints),
      mesherEpsilon_(mesherEpsilon), mesherScaling_(mesherScaling), mesherConcentration_(mesherConcentration),
      bootstrapMode_(bootstrapMode), enforceFokkerPlanckBootstrap_(enforceFokkerPlanckBootstrap), calibrate_(calibrate),
      adjustEquityVolatility_(adjustEquityVolatility), adjustEquityForward_(adjustEquityForward) {

    // checks

    QL_REQUIRE(!stepTimes.empty(), "DefaultableEquityJumpDiffusionModel: at least one step time required");
    QL_REQUIRE(close_enough(p_, 0.0) || adjustEquityVolatility_,
               "DefaultableEquityJumpDiffusionModel: for p != 0 (" << p_ << ") adjustEquityVolatility must be true");

    // register with observables

    marketObserver_ = QuantLib::ext::make_shared<MarketObserver>();
    marketObserver_->registerWith(equity_);
    marketObserver_->registerWith(creditCurve_);

    registerWith(volatility_);
    registerWith(marketObserver_);

    // notify observers of all market data changes, not only when not calculated
    alwaysForwardNotifications();
}

Handle<DefaultableEquityJumpDiffusionModel> DefaultableEquityJumpDiffusionModelBuilder::model() const {
    calculate();
    return model_;
}

void DefaultableEquityJumpDiffusionModelBuilder::forceRecalculate() {
    forceCalibration_ = true;
    ModelBuilder::forceRecalculate();
    forceCalibration_ = false;
}

bool DefaultableEquityJumpDiffusionModelBuilder::requiresRecalibration() const {
    return calibrationPointsChanged(false) || marketObserver_->hasUpdated(false) || forceCalibration_;
}

bool DefaultableEquityJumpDiffusionModelBuilder::calibrationPointsChanged(const bool updateCache) const {

    // get the current foewards and vols

    std::vector<Real> forwards;
    std::vector<Real> variances;
    for (auto const t : stepTimes_) {
        forwards.push_back(equity_->equitySpot()->value() * equity_->equityDividendCurve()->discount(t) /
                           equity_->equityForecastCurve()->discount(t));
        variances.push_back(volatility_->blackVariance(t, forwards.back()));
    }

    // check for differences

    bool changed = false;
    if (stepTimes_.size() != cachedForwards_.size() || stepTimes_.size() != cachedVariances_.size()) {
        changed = true;
    } else {
        for (Size i = 0; i < stepTimes_.size() && !changed; ++i) {
            // strict comparison is deliberate here!
            if (cachedForwards_[i] != forwards[i])
                changed = true;
            else if (cachedVariances_[i] != variances[i])
                changed = true;
        }
    }

    // update cache if caller desires so

    if (updateCache) {
        cachedForwards_ = forwards;
        cachedVariances_ = variances;
    }

    // return the result

    return changed;
}

void DefaultableEquityJumpDiffusionModelBuilder::performCalculations() const {

    if (requiresRecalibration()) {

        calibrationPointsChanged(true);

        // reset market observer's updated flag

        marketObserver_->hasUpdated(true);

        // setup model and bootstrap the model parameters

        std::vector<Real> h0(stepTimes_.size(), 0.0);
        std::vector<Real> sigma(stepTimes_.size(), 0.10);
        model_.linkTo(QuantLib::ext::make_shared<DefaultableEquityJumpDiffusionModel>(
            stepTimes_, h0, sigma, equity_, creditCurve_, volatility_->dayCounter(), p_, eta_, adjustEquityForward_));
        if (calibrate_) {
            model_->bootstrap(volatility_, staticMesher_, timeStepsPerYear_, stateGridPoints_, mesherEpsilon_,
                              mesherScaling_, mesherConcentration_, bootstrapMode_, enforceFokkerPlanckBootstrap_,
                              adjustEquityVolatility_);
        }

        // notify model observers
        model_->notifyObservers();
    }
}

DefaultableEquityJumpDiffusionModel::DefaultableEquityJumpDiffusionModel(
    const std::vector<Real>& stepTimes, const std::vector<Real>& h0, const std::vector<Real>& sigma,
    const QuantLib::ext::shared_ptr<QuantExt::EquityIndex2>& equity,
    const Handle<QuantLib::DefaultProbabilityTermStructure>& creditCurve, const DayCounter& volDayCounter, const Real p,
    const Real eta, const bool adjustEquityForward)
    : stepTimes_(stepTimes), h0_(h0), sigma_(sigma), equity_(equity), creditCurve_(creditCurve),
      volDayCounter_(volDayCounter), p_(p), eta_(eta), adjustEquityForward_(adjustEquityForward) {
    registerWith(equity);
    registerWith(creditCurve);
}

void DefaultableEquityJumpDiffusionModel::update() { notifyObservers(); }

const std::vector<Real>& DefaultableEquityJumpDiffusionModel::stepTimes() const { return stepTimes_; }

QuantLib::ext::shared_ptr<QuantExt::EquityIndex2> DefaultableEquityJumpDiffusionModel::equity() const { return equity_; }

Real DefaultableEquityJumpDiffusionModel::totalBlackVariance() const { return totalBlackVariance_; }

const DayCounter& DefaultableEquityJumpDiffusionModel::volDayCounter() const { return volDayCounter_; }

Handle<DefaultProbabilityTermStructure> DefaultableEquityJumpDiffusionModel::creditCurve() const {
    return creditCurve_;
}

Real DefaultableEquityJumpDiffusionModel::eta() const { return eta_; }

Real DefaultableEquityJumpDiffusionModel::p() const { return p_; }

bool DefaultableEquityJumpDiffusionModel::adjustEquityForward() const { return adjustEquityForward_; }

Real DefaultableEquityJumpDiffusionModel::timeFromReference(const Date& d) const {
    return volDayCounter_.yearFraction(creditCurve_->referenceDate(), d);
}

const std::vector<Real>& DefaultableEquityJumpDiffusionModel::h0() const { return h0_; }

const std::vector<Real>& DefaultableEquityJumpDiffusionModel::sigma() const { return sigma_; }

Size DefaultableEquityJumpDiffusionModel::getTimeIndex(const Real t) const {
    Size m = std::distance(stepTimes_.begin(),
                           std::lower_bound(stepTimes_.begin(), stepTimes_.end(), t,
                                            [](Real x, Real y) { return x < y && !close_enough(x, y); }));
    return std::min(m, stepTimes_.size() - 1);
}

Real DefaultableEquityJumpDiffusionModel::h(const Real t, const Real S) const {
    return h0_[getTimeIndex(t)] * std::pow(equity_->equitySpot()->value() / S, p_);
}

Real DefaultableEquityJumpDiffusionModel::sigma(const Real t) const { return sigma_[getTimeIndex(t)]; }

Real DefaultableEquityJumpDiffusionModel::r(const Real t) const {
    if (t > fh_) {
        return -std::log(equity_->equityForecastCurve()->discount(t + fh_) /
                         equity_->equityForecastCurve()->discount(t - fh_)) /
               (2.0 * fh_);
    } else {
        return -std::log(equity_->equityForecastCurve()->discount(t + fh_) /
                         equity_->equityForecastCurve()->discount(t)) /
               (fh_);
    }
}

Real DefaultableEquityJumpDiffusionModel::q(const Real t) const {
    if (t > fh_) {
        return -std::log(equity_->equityDividendCurve()->discount(t + fh_) /
                         equity_->equityDividendCurve()->discount(t - fh_)) /
               (2.0 * fh_);
    } else {
        return -std::log(equity_->equityDividendCurve()->discount(t + fh_) /
                         equity_->equityDividendCurve()->discount(t)) /
               (fh_);
    }
}

Real DefaultableEquityJumpDiffusionModel::dividendYield(const Real s, const Real t) const {
    QL_REQUIRE(t > s || close_enough(s, t), "DefaultableEquityJumppDiffusionModel::dividendYield(): start time ("
                                                << s << ") must be less or equal than end time (" << t << ")");
    Real tmp = t;
    if (close_enough(s, t)) {
        tmp = s + fh_;
    }
    return -std::log(equity_->equityDividendCurve()->discount(tmp) / equity_->equityDividendCurve()->discount(s)) /
           (tmp - s);
}

void DefaultableEquityJumpDiffusionModel::bootstrap(
    const Handle<QuantLib::BlackVolTermStructure>& volatility, const bool staticMesher, const Size timeStepsPerYear,
    const Size stateGridPoints, const Real mesherEpsilon, const Real mesherScaling, const Real mesherConcentration,
    const DefaultableEquityJumpDiffusionModelBuilder::BootstrapMode bootstrapMode,
    const bool enforceFokkerPlanckBootstrap, const bool adjustEquityVolatility) const {

    // set total black variance

    Real forward = equity_->equitySpot()->value() * equity_->equityDividendCurve()->discount(stepTimes_.back()) /
                   equity_->equityForecastCurve()->discount(stepTimes_.back());
    totalBlackVariance_ = volatility->blackVariance(stepTimes_.back(), forward);

    // check validity of credit curve, otherwise we might divide by zero below

    for (Size i = 0; i < stepTimes_.size(); ++i) {
        QL_REQUIRE(!close_enough(creditCurve_->survivalProbability(stepTimes_[i]), 0.0),
                   "DefaultableEquityJumpDiffusionModel: creditCurve implies zero survival probability at t = "
                       << stepTimes_[i]
                       << ", this can not be handled. Check the credit curve / security spread provided in the market "
                          "data. If this happens during a spread imply, the target price might not be attainable even "
                          "for high spreads.");
    }

    if (close_enough(p_, 0.0) && !enforceFokkerPlanckBootstrap) {

        // 1. case p = 0:

        Real accumulatedVariance = 0.0;

        for (Size i = 0; i < stepTimes_.size(); ++i) {

            // 1.1 the stepwise model h is just the average hazard rate over each interval

            h0_[i] = -std::log(creditCurve_->survivalProbability(stepTimes_[i]) /
                               (i == 0 ? 1.0 : creditCurve_->survivalProbability(stepTimes_[i - 1]))) /
                     (stepTimes_[i] - (i == 0 ? 0.0 : stepTimes_[i - 1]));

            // 1.2 determine the implied equity vol in our model

            Real impliedModelVol = 0.0;
            Real forward = equity_->equitySpot()->value() / equity_->equityForecastCurve()->discount(stepTimes_[i]) *
                           equity_->equityDividendCurve()->discount(stepTimes_[i]);

            if (!adjustEquityVolatility) {

                // 1.2.1 if we do not adjust the equity vol, we just read it from the market surface

                impliedModelVol = volatility->blackVol(stepTimes_[i], forward);

            } else {

                // 1.2.2 if we adjust the equity vol, we first compute the market atm option price

                Real marketPrice = blackFormula(Option::Call, forward, forward,
                                                std::sqrt(volatility->blackVariance(stepTimes_[i], forward)),
                                                equity_->equityForecastCurve()->discount(stepTimes_[i]));

                // 1.2.2 adjust the forward and the discount factor by the credit drift / jump intensity and back out
                // the model implied vol using these and the market price computed above

                // TODO a tacit assumption here is that a defaulted equity does not contribute signficantly to the ATM
                // call option payoff, i.e. that eta is sufficiently close to 1, review this and compare to the "full"
                // implementation via Fokker-Planck below

                Real adjustedForward = adjustEquityForward_
                                           ? forward / std::pow(creditCurve_->survivalProbability(stepTimes_[i]), eta_)
                                           : forward;
                try {
                    impliedModelVol =
                        blackFormulaImpliedStdDev(Option::Call, forward, adjustedForward, marketPrice,
                                                  equity_->equityForecastCurve()->discount(stepTimes_[i]) *
                                                      creditCurve_->survivalProbability(stepTimes_[i])) /
                        std::sqrt(stepTimes_[i]);
                } catch (...) {
                }
            }

            impliedModelVol = std::max(1E-4, impliedModelVol);

            // 1.3 bootstrap the stepwise model sigma

            sigma_[i] =
                i == 0
                    ? impliedModelVol
                    : std::sqrt(std::max(impliedModelVol * impliedModelVol * stepTimes_[i] - accumulatedVariance, 0.0) /
                                (stepTimes_[i] - stepTimes_[i - 1]));

            accumulatedVariance += sigma_[i] * sigma_[i] * (stepTimes_[i] - (i == 0 ? 0.0 : stepTimes_[i - 1]));
        }

    } else {

        // 2. case p != 0 (or enforced Fokker-Planck method for bootstrap)

        // 2.1 set up solver for the Fokker-Planck PDE

        // 2.1.1 build mesher

        Real spot = equity()->equitySpot()->value();
        Real logSpot = std::log(spot);

        if (mesher_ == nullptr || !staticMesher) {

            TimeGrid tmpGrid(stepTimes_.begin(), stepTimes_.end(),
                             std::max<Size>(std::lround(timeStepsPerYear * stepTimes_.back() + 0.5), 1));

            Real mi = spot;
            Real ma = spot;

            Real forward = spot;
            for (Size i = 1; i < tmpGrid.size(); ++i) {
                forward = equity()->equitySpot()->value() * equity()->equityDividendCurve()->discount(tmpGrid[i]) /
                          equity()->equityForecastCurve()->discount(tmpGrid[i]) /
                          std::pow(creditCurve()->survivalProbability(tmpGrid[i]), eta());
                mi = std::min(mi, forward);
                ma = std::max(ma, forward);
            }

            Real sigmaSqrtT = std::max(1E-4, std::sqrt(volatility->blackVariance(tmpGrid.back(), forward)));
            Real normInvEps = InverseCumulativeNormal()(1.0 - mesherEpsilon);
            Real xMin = std::log(mi) - sigmaSqrtT * normInvEps * mesherScaling;
            Real xMax = std::log(ma) + sigmaSqrtT * normInvEps * mesherScaling;

            if (mesherConcentration != Null<Real>()) {
                mesher_ = QuantLib::ext::make_shared<Concentrating1dMesher>(xMin, xMax, stateGridPoints,
                                                                    std::make_pair(logSpot, mesherConcentration), true);
            } else {
                mesher_ = QuantLib::ext::make_shared<Uniform1dMesher>(xMin, xMax, stateGridPoints);
            }
        }

        // 2.1.2 build operator

        auto fdmOp = QuantLib::ext::make_shared<FdmDefaultableEquityJumpDiffusionFokkerPlanckOp>(
            stepTimes_.back(), QuantLib::ext::make_shared<FdmMesherComposite>(mesher_), shared_from_this());

        // 2.1.3 build solver

        // TODO which schema / how many damping steps should we take? For the moment we follow the recommendation from
        // the Andersen paper and use Crank-Nicholson after a "few" Rannacher steps (i.e. implicit Euler)
        // TrBDF2 seems to be a specialised scheme for initial Dirac conditons though. Some research required...

        auto solver = QuantLib::ext::make_shared<FdmBackwardSolver>(
            fdmOp, std::vector<QuantLib::ext::shared_ptr<BoundaryCondition<FdmLinearOp>>>(), nullptr, FdmSchemeDesc::Douglas());

        constexpr Size dampingSteps = 5;

        // 2.2 boostrap h0 and sigma functions

        // 2.2.1 set integration length dy for each grid point (= center of integration interval) and initial condition

        Array p(mesher_->size(), 0.0), dy(mesher_->size(), 0.0);

        for (Size i = 0; i < mesher_->size(); ++i) {
            if (i > 0) {
                dy[i] += 0.5 * mesher_->dminus(i);
            } else {
                dy[i] += 0.5 * mesher_->dminus(i + 1);
            }
            if (i < mesher_->size() - 1) {
                dy[i] += 0.5 * mesher_->dplus(i);
            } else {
                dy[i] += 0.5 * mesher_->dplus(i - 1);
            }
        }

        for (Size i = 0; i < mesher_->size(); ++i) {
            if (i > 0) {
                if ((logSpot > mesher_->location(i - 1) || close_enough(logSpot, mesher_->location(i - 1))) &&
                    (logSpot < mesher_->location(i) && !close_enough(logSpot, mesher_->location(i)))) {
                    Real alpha = (mesher_->location(i) - logSpot) / mesher_->dplus(i - 1);
                    p[i - 1] = alpha / dy[i - 1];
                    p[i] = (1 - alpha) / dy[i];
                }
            }
        }

        Array guess(2);

        // bookkeeping to maange optimization guesses (see below)
        constexpr Real thresholdSuccessfulOptimization = 1E-5;
        Real lastOptimizationError = 0.0;
        Array lastValidGuess;

        for (Size i = 0; i < stepTimes_.size(); ++i) {

            // 2.2.2 we roll from timestep i-1 (t=0 for i=0) to i and calibrate (h0, sigma) to the market

            Real marketDefaultableBond = creditCurve_->survivalProbability(stepTimes_[i]) *
                                         equity_->equityForecastCurve()->discount(stepTimes_[i]);
            Real forward = equity_->equitySpot()->value() / equity_->equityForecastCurve()->discount(stepTimes_[i]) *
                           equity_->equityDividendCurve()->discount(stepTimes_[i]);
            Real marketEquityOption = blackFormula(Option::Call, forward, forward,
                                                   std::sqrt(volatility->blackVariance(stepTimes_[i], forward)),
                                                   equity_->equityForecastCurve()->discount(stepTimes_[i]));

            struct TargetFunction : public QuantLib::CostFunction {
                static Real hToOpt(const Real x) { return std::log(std::max(x, 1E-6)); }
                static Real sigmaToOpt(const Real x) { return std::log(std::max(x, 1E-6)); }
                static Real hToReal(const Real x) { return std::exp(x); }
                static Real sigmaToReal(const Real x) { return std::exp(x); }

                enum class Mode { h0, sigma, both };
                TargetFunction(const Mode mode, const Real strike, const Real marketEquityOption,
                               const Real marketDefaultableBond, Real& h0, Real& sigma, const Array& p,
                               const std::vector<Real>& locations, const Array& dy,
                               const QuantLib::ext::shared_ptr<FdmBackwardSolver>& solver, const Real t_from, const Real t_to,
                               const Size steps, const Size dampingSteps)
                    : mode_(mode), strike_(strike), marketEquityOption_(marketEquityOption),
                      marketDefaultableBond_(marketDefaultableBond), h0_(h0), sigma_(sigma), locations_(locations),
                      dy_(dy), p_(p), solver_(solver), t_from_(t_from), t_to_(t_to), steps_(steps),
                      dampingSteps_(dampingSteps) {}

                Array values(const Array& x) const override {

                    // 2.2.2.0 set trial values from optimiser for h0 and sigma, we square the values to ensure
                    // positivity, this transformation is reverted when setting the guess and reading the solution

                    if (mode_ == Mode::h0) {
                        h0_ = hToReal(x[0]);
                    } else if (mode_ == Mode::sigma) {
                        sigma_ = sigmaToReal(x[0]);
                    } else {
                        h0_ = hToReal(x[0]);
                        sigma_ = sigmaToReal(x[1]);
                    }

                    // 2.2.2.1 roll density back (in "time to maturity", not in calendar time, which rolls forward)

                    Array pTmp = p_;
                    solver_->rollback(pTmp, t_from_, t_to_, steps_, dampingSteps_);

                    // 2.2.2.2 compute model defaultable zero bond and equity call option

                    Real defaultableBond = 0.0;
                    if (mode_ == Mode::h0 || mode_ == Mode::both) {
                        for (Size i = 0; i < pTmp.size(); ++i) {
                            defaultableBond += pTmp[i] * dy_[i];
                        }
                    }

                    Real equityOption = 0.0;
                    if (mode_ == Mode::sigma || mode_ == Mode::both) {
                        bool knockInLocation = true;
                        for (Size i = 0; i < pTmp.size(); ++i) {
                            if (locations_[i] > std::log(strike_) && !close_enough(locations_[i], std::log(strike_))) {
                                Real dy = dy_[i];
                                if (knockInLocation) {
                                    // TODO there are more sophisticated corrections (reference in the Andersen paper?)
                                    dy = (locations_[i] - std::log(strike_));
                                    if (i < pTmp.size())
                                        dy += 0.5 * (locations_[i + 1] - locations_[i]);
                                    else
                                        dy += 0.5 * (locations_[i] - locations_[i - 1]);
                                    knockInLocation = false;
                                }
                                equityOption += pTmp[i] * dy * (std::exp(locations_[i]) - strike_);
                            }
                        }
                    }

                    if (mode_ == Mode::h0) {
                        Array result(1);
                        result[0] = (defaultableBond - marketDefaultableBond_) / marketDefaultableBond_;
                        return result;
                    } else if (mode_ == Mode::sigma) {
                        Array result(1);
                        result[0] = (equityOption - marketEquityOption_) / marketEquityOption_;
                        return result;
                    } else {
                        Array result(2);
                        result[0] = (defaultableBond - marketDefaultableBond_) / marketDefaultableBond_;
                        result[1] = (equityOption - marketEquityOption_) / marketEquityOption_;
                        return result;
                    }
                }
                const Mode mode_;
                const Real strike_, marketEquityOption_, marketDefaultableBond_;
                Real &h0_, &sigma_;
                const std::vector<Real>& locations_;
                const Array &dy_, &p_;
                const QuantLib::ext::shared_ptr<FdmBackwardSolver> solver_;
                const Real t_from_, t_to_;
                const Size steps_, dampingSteps_;
            };

            /* 2.2.2.3 The first guess (i=0) are parameters computed with p = 0 as above.
                       A subsequent guess (i>0) is the last solution that resulted in an
                       error below a given threshold (or the guess for i=0 if that is the
                       only available guess). */

            if (i == 0) {
                guess[0] = -std::log(creditCurve_->survivalProbability(stepTimes_[i]) /
                                     (i == 0 ? 1.0 : creditCurve_->survivalProbability(stepTimes_[i - 1]))) /
                           (stepTimes_[i] - (i == 0 ? 0.0 : stepTimes_[i - 1]));
                guess[1] = std::sqrt(volatility->blackVariance(stepTimes_[i], forward) /
                                     (stepTimes_[i] - (i == 0 ? 0.0 : stepTimes_[i - 1])));
                lastValidGuess = guess;
            } else {
                if (lastOptimizationError < thresholdSuccessfulOptimization) {
                    guess[0] = h0_[i - 1];
                    guess[1] = sigma_[i - 1];
                    lastValidGuess = guess;
                } else {
                    guess = lastValidGuess;
                }
            }

            // 2.2.2.4 transform the guess

            guess[0] = TargetFunction::hToOpt(guess[0]);
            guess[1] = TargetFunction::sigmaToOpt(guess[1]);

            // 2.2.2.5 run the optimzation for the current time step and set the model parameters to the found solution

            Real t_from = stepTimes_.back() - (i == 0 ? 0.0 : stepTimes_[i - 1]);
            Real t_to = stepTimes_.back() - stepTimes_[i];
            Size steps = static_cast<Size>(0.5 + (t_from - t_to) * static_cast<Real>(timeStepsPerYear));

            NoConstraint noConstraint;
            LevenbergMarquardt lm;
            EndCriteria endCriteria(100, 10, 1E-8, 1E-8, 1E-8);
            if (bootstrapMode == DefaultableEquityJumpDiffusionModelBuilder::BootstrapMode::Simultaneously) {
                // simultaneous optimization of (h0,sigma)
                TargetFunction targetFunction(TargetFunction::Mode::both, forward, marketEquityOption,
                                              marketDefaultableBond, h0_[i], sigma_[i], p, mesher_->locations(), dy,
                                              solver, t_from, t_to, steps, i == 0 ? dampingSteps : 0);
                Problem problem(targetFunction, noConstraint, guess);
                lm.minimize(problem, endCriteria);
                h0_[i] = TargetFunction::hToReal(problem.currentValue()[0]);
                sigma_[i] = TargetFunction::sigmaToReal(problem.currentValue()[1]);
                lastOptimizationError = problem.functionValue();
            } else {
                // alternating optimization of h0 and sigma
                TargetFunction target_function_h0(TargetFunction::Mode::h0, forward, marketEquityOption,
                                                  marketDefaultableBond, h0_[i], sigma_[i], p, mesher_->locations(), dy,
                                                  solver, t_from, t_to, steps, i == 0 ? dampingSteps : 0);
                TargetFunction target_function_sigma(TargetFunction::Mode::sigma, forward, marketEquityOption,
                                                     marketDefaultableBond, h0_[i], sigma_[i], p, mesher_->locations(),
                                                     dy, solver, t_from, t_to, steps, i == 0 ? dampingSteps : 0);
                Real current0 = guess[0], current1 = guess[1];
                Real delta0 = 0.0, delta1 = 0.0;
                Real functionValue0 = 0.0, functionValue1 = 0.0;
                Size iteration = 0;
                constexpr Real tol_x = 1E-8;
                do {
                    // optimize h0
                    Problem p0(target_function_h0, noConstraint, Array(1, current0));
                    lm.minimize(p0, endCriteria);
                    h0_[i] = TargetFunction::hToReal(p0.currentValue()[0]);
                    delta0 = std::abs(h0_[i] - TargetFunction::hToReal(current0));
                    current0 = p0.currentValue()[0];
                    functionValue0 = p0.functionValue();
                    // optimize sigma
                    Problem p1(target_function_sigma, noConstraint, Array(1, current1));
                    lm.minimize(p1, endCriteria);
                    sigma_[i] = TargetFunction::sigmaToReal(p1.currentValue()[0]);
                    delta1 = std::abs(sigma_[i] - TargetFunction::sigmaToReal(current1));
                    current1 = p1.currentValue()[0];
                    functionValue1 = p1.functionValue();
                } while (iteration++ < 50 && (delta0 > tol_x || delta1 > tol_x));
                lastOptimizationError =
                    std::sqrt(0.5 * (functionValue0 * functionValue0 + functionValue1 * functionValue1));
            }

            // 2.2.2.6 we still have to do the actual roll back with the solution we have found

            solver->rollback(p, t_from, t_to, steps, i == 0 ? dampingSteps : 0);

        } // bootstrap over step times
    }     // case p!= 0
}

} // namespace QuantExt
