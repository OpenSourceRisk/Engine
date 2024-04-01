/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

#include <ored/model/localvolmodelbuilder.hpp>
#include <ored/utilities/log.hpp>

#include <qle/models/carrmadanarbitragecheck.hpp>

#include <ql/exercise.hpp>
#include <ql/instruments/payoffs.hpp>
#include <ql/instruments/vanillaoption.hpp>
#include <ql/termstructures/volatility/equityfx/andreasenhugelocalvoladapter.hpp>
#include <ql/termstructures/volatility/equityfx/andreasenhugevolatilityinterpl.hpp>
#include <ql/termstructures/volatility/equityfx/localconstantvol.hpp>
#include <ql/termstructures/volatility/equityfx/localvolsurface.hpp>
#include <ql/termstructures/volatility/equityfx/noexceptlocalvolsurface.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <ql/pricingengines/vanilla/analyticeuropeanengine.hpp>

namespace ore {
namespace data {

LocalVolModelBuilder::LocalVolModelBuilder(
    const std::vector<Handle<YieldTermStructure>>& curves,
    const std::vector<ext::shared_ptr<GeneralizedBlackScholesProcess>>& processes,
    const std::set<Date>& simulationDates, const std::set<Date>& addDates, const Size timeStepsPerYear,
    const Type lvType, const std::vector<Real>& calibrationMoneyness, const bool dontCalibrate)
    : BlackScholesModelBuilderBase(curves, processes, simulationDates, addDates, timeStepsPerYear), lvType_(lvType),
      calibrationMoneyness_(calibrationMoneyness), dontCalibrate_(dontCalibrate) {
    // we have to observe the whole vol surface for the Dupire implementation unfortunately; we can specify the time
    // steps that are relevant, but not a set of discrete strikes
    if (lvType == Type::Dupire) {
        for (auto const& p : processes_) {
            marketObserver_->registerWith(p->blackVolatility());
        }
    }
}

std::vector<QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>> LocalVolModelBuilder::getCalibratedProcesses() const {

    QL_REQUIRE(lvType_ != Type::AndreasenHuge || !calibrationMoneyness_.empty(), "no calibration moneyness provided");

    calculate();
    
    std::vector<QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>> processes;

    for (Size l = 0; l < processes_.size(); ++l) {

        Handle<LocalVolTermStructure> localVol;
        if (dontCalibrate_) {
            localVol = Handle<LocalVolTermStructure>(
                QuantLib::ext::make_shared<LocalConstantVol>(0, NullCalendar(), 0.10, ActualActual(ActualActual::ISDA)));
        } else if (lvType_ == Type::AndreasenHuge) {
            // for checking arbitrage free input prices, just for logging purposes at this point
            // notice that we need a uniform strike grid here, so this is not the same as the one below
            // we choose the strike grid to be the one for the last calibration point
            std::vector<Real> checkMaturities, checkMoneynesses, atmForwards;
            std::vector<std::vector<Real>> callPrices;

            // set up Andreasen Huge Volatility interpolation for each underyling
            // we choose the calibration set to be OTM options on the effective future simulation dates
            // with strikes given in terms of moneyness K / atmForward
            AndreasenHugeVolatilityInterpl::CalibrationSet calSet;
            for (auto const& d : effectiveSimulationDates_) {
                if (d <= curves_.front()->referenceDate())
                    continue;
                Real t = processes_.front()->riskFreeRate()->timeFromReference(d);
                checkMaturities.push_back(t);
                Real atmLevel =
                    atmForward(processes_[l]->x0(), processes_[l]->riskFreeRate(), processes_[l]->dividendYield(), t);
                Real atmMarketVol = std::max(1e-4, processes_[l]->blackVolatility()->blackVol(t, atmLevel));
                callPrices.push_back(std::vector<Real>());
                atmForwards.push_back(atmLevel);
                for (Size i = 0; i < calibrationMoneyness_.size(); ++i) {
                    Real strike = atmLevel * std::exp(calibrationMoneyness_[i] * atmMarketVol * std::sqrt(t));
                    Real marketVol = processes_[l]->blackVolatility()->blackVol(t, strike);
                    // skip option with effective moneyness < 0.0001 or > 0.9999 (TODO, hardcoded limits here?)
                    if (std::fabs(calibrationMoneyness_[i]) > 3.72)
                        continue;
                    auto option =
                        QuantLib::ext::make_shared<VanillaOption>(QuantLib::ext::make_shared<PlainVanillaPayoff>(Option::Call, strike),
                                                          QuantLib::ext::make_shared<EuropeanExercise>(d));
                    calSet.push_back(std::make_pair(option, QuantLib::ext::make_shared<SimpleQuote>(marketVol)));
                    option->setPricingEngine(QuantLib::ext::make_shared<AnalyticEuropeanEngine>(processes_[l]));
                    callPrices.back().push_back(option->NPV());
                    if (d == *effectiveSimulationDates_.rbegin()) {
                        checkMoneynesses.push_back(strike / atmLevel);
                    }
                }
            }

            // arbitrage check
            QuantExt::CarrMadanSurface cmCheck(checkMaturities, checkMoneynesses, processes_[l]->x0(), atmForwards,
                                               callPrices);
            if (!cmCheck.arbitrageFree()) {
                WLOG("Andreasen-Huge local vol calibration for process #" << l
                                                                          << ":, input vol is not arbitrage free:");
                DLOG("time,moneyness,callSpread,butterfly,calendar");
                for (Size i = 0; i < checkMaturities.size(); ++i)
                    for (Size j = 0; j < checkMoneynesses.size(); ++j)
                        DLOG(checkMaturities[i] << "," << checkMoneynesses[i] << "," << std::boolalpha
                                                << cmCheck.callSpreadArbitrage()[i][j] << ","
                                                << cmCheck.butterflyArbitrage()[i][j] << ","
                                                << cmCheck.calendarArbitrage()[i][j]);
            }

            // TODO using some hardcoded values here, expose to configuration?
            auto ah = QuantLib::ext::make_shared<AndreasenHugeVolatilityInterpl>(
                calSet, processes_[l]->stateVariable(), processes_[l]->riskFreeRate(), processes_[l]->dividendYield(),
                AndreasenHugeVolatilityInterpl::CubicSpline, AndreasenHugeVolatilityInterpl::Call, 500, Null<Real>(),
                Null<Real>());
            localVol = Handle<LocalVolTermStructure>(QuantLib::ext::make_shared<AndreasenHugeLocalVolAdapter>(ah));
            //localVol->enableExtrapolation();
            DLOG("Andreasen-Huge local vol calibration for process #"
                 << l
                 << ": "
                    "calibration error min="
                 << std::scientific << std::setprecision(6) << QuantLib::ext::get<0>(ah->calibrationError()) << " max="
                 << QuantLib::ext::get<1>(ah->calibrationError()) << " avg=" << QuantLib::ext::get<2>(ah->calibrationError()));
        } else if (lvType_ == Type::Dupire) {
            localVol = Handle<LocalVolTermStructure>(
                QuantLib::ext::make_shared<LocalVolSurface>(processes_[l]->blackVolatility(), processes_[l]->riskFreeRate(),
                                                    processes_[l]->dividendYield(), processes_[l]->stateVariable()));
        } else if (lvType_ == Type::DupireFloored) {
            localVol = Handle<LocalVolTermStructure>(
                QuantLib::ext::make_shared<NoExceptLocalVolSurface>(processes_[l]->blackVolatility(), processes_[l]->riskFreeRate(),
                                                            processes_[l]->dividendYield(), processes_[l]->stateVariable(),
                                                            0.0));
        } else {
            QL_FAIL("unexpected local vol type");
        }

        processes.push_back(QuantLib::ext::make_shared<GeneralizedBlackScholesProcess>(
            processes_[l]->stateVariable(), processes_[l]->dividendYield(), processes_[l]->riskFreeRate(),
            processes_[l]->blackVolatility(), localVol));
    }

    return processes;
}

std::vector<std::vector<Real>> LocalVolModelBuilder::getCurveTimes() const {
    std::vector<Real> timesExt(discretisationTimeGrid_.begin() + 1, discretisationTimeGrid_.end());
    for (auto const& d : addDates_) {
        if (d > curves_.front()->referenceDate()) {
            timesExt.push_back(curves_.front()->timeFromReference(d));
        }
    }
    std::sort(timesExt.begin(), timesExt.end());
    auto it = std::unique(timesExt.begin(), timesExt.end(),
                          [](const Real x, const Real y) { return QuantLib::close_enough(x, y); });
    timesExt.resize(std::distance(timesExt.begin(), it));
    return std::vector<std::vector<Real>>(allCurves_.size(), timesExt);
}

std::vector<std::vector<std::pair<Real, Real>>> LocalVolModelBuilder::getVolTimesStrikes() const {
    std::vector<std::vector<std::pair<Real, Real>>> volTimesStrikes;
    // for the Dupire implementation we observe the whole vol surface anyhow (see ctor above)
    if (lvType_ == Type::Dupire)
        return volTimesStrikes;
    std::vector<Real> times;
    if (lvType_ == Type::AndreasenHuge) {
        for (auto const& d : effectiveSimulationDates_) {
            if (d > curves_.front()->referenceDate())
                times.push_back(processes_.front()->riskFreeRate()->timeFromReference(d));
        }
    } else {
        times = std::vector<Real>(discretisationTimeGrid_.begin() + 1, discretisationTimeGrid_.end());
    }
    for (auto const& p : processes_) {
        volTimesStrikes.push_back(std::vector<std::pair<Real, Real>>());
        for (auto const t : times) {
            Real atmLevel = atmForward(p->x0(), p->riskFreeRate(), p->dividendYield(), t);
            Real atmMarketVol = std::max(1e-4, p->blackVolatility()->blackVol(t, atmLevel));
            for (auto const m : calibrationMoneyness_) {
                Real strike = atmLevel * std::exp(m * atmMarketVol * std::sqrt(t));
                volTimesStrikes.back().push_back(std::make_pair(t, strike));
            }
        }
    }
    return volTimesStrikes;
}

} // namespace data
} // namespace ore
