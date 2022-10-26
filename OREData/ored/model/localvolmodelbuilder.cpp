/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
 All rights reserved.
*/

#include <orepscriptedtrade/ored/models/localvolmodelbuilder.hpp>
#include <orepscriptedtrade/ored/scripting/utilities.hpp>
#include <orepscriptedtrade/qle/models/carrmadanarbitragecheck.hpp>

#include <ored/utilities/log.hpp>

#include <ql/instruments/payoffs.hpp>
#include <ql/instruments/vanillaoption.hpp>
#include <ql/termstructures/volatility/equityfx/andreasenhugelocalvoladapter.hpp>
#include <ql/termstructures/volatility/equityfx/andreasenhugevolatilityinterpl.hpp>
#include <ql/termstructures/volatility/equityfx/localconstantvol.hpp>
#include <ql/termstructures/volatility/equityfx/localvolsurface.hpp>
#include <ql/time/daycounters/actualactual.hpp>

namespace oreplus {
namespace data {

LocalVolModelBuilder::LocalVolModelBuilder(
    const std::vector<Handle<YieldTermStructure>>& curves,
    const std::vector<boost::shared_ptr<GeneralizedBlackScholesProcess>>& processes,
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

std::vector<boost::shared_ptr<GeneralizedBlackScholesProcess>> LocalVolModelBuilder::getCalibratedProcesses() const {

    QL_REQUIRE(lvType_ != Type::AndreasenHuge || !calibrationMoneyness_.empty(), "no calibration moneyness provided");

    std::vector<boost::shared_ptr<GeneralizedBlackScholesProcess>> processes;

    for (Size l = 0; l < processes_.size(); ++l) {

        Handle<LocalVolTermStructure> localVol;
        if (dontCalibrate_) {
            localVol = Handle<LocalVolTermStructure>(
                boost::make_shared<LocalConstantVol>(0, NullCalendar(), 0.10, ActualActual(ActualActual::ISDA)));
        } else if (lvType_ == Type::AndreasenHuge) {
            // for checking arbitrage free input prices, just for logging purposes at this point
            // notice that we need a uniform strike grid here, so this is not the same as the one below
            // we choose the strike grid to be the one for the last calibration point
            std::vector<Real> checkMaturities, checkStrikes;

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
                Real atmMarketVol = processes_[l]->blackVolatility()->blackVol(t, atmLevel);
                for (Size i = 0; i < calibrationMoneyness_.size(); ++i) {
                    Real strike = atmLevel * std::exp(calibrationMoneyness_[i] * atmMarketVol * std::sqrt(t));
                    Real marketVol = processes_[l]->blackVolatility()->blackVol(t, strike);
                    // skip option with effective moneyness < 0.0001 or > 0.9999 (TODO, hardcoded limits here?)
                    if (std::fabs(calibrationMoneyness_[i]) > 3.72)
                        continue;
                    auto option =
                        boost::make_shared<VanillaOption>(boost::make_shared<PlainVanillaPayoff>(Option::Call, strike),
                                                          boost::make_shared<EuropeanExercise>(d));
                    calSet.push_back(std::make_pair(option, boost::make_shared<SimpleQuote>(marketVol)));
                    if (d == *effectiveSimulationDates_.rbegin())
                        checkStrikes.push_back(strike);
                }
            }

            // arbitrage check
            CarrMadanArbitrageCheck cmCheck(processes_[l]->x0(), processes_[l]->blackVolatility(), checkMaturities,
                                            checkStrikes);
            if (!cmCheck.arbitrageFree()) {
                WLOG("Andreasen-Huge local vol calibration for process #" << l
                                                                          << ":, input vol is not arbitrage free:");
                DLOGGERSTREAM(cmCheck.violationsAsMatrix());
            }

            // TODO using some hardcoded values here, expose to configuration?
            auto ah = boost::make_shared<AndreasenHugeVolatilityInterpl>(
                calSet, processes_[l]->stateVariable(), processes_[l]->riskFreeRate(), processes_[l]->dividendYield(),
                AndreasenHugeVolatilityInterpl::CubicSpline, AndreasenHugeVolatilityInterpl::Call, 500, Null<Real>(),
                Null<Real>());
            localVol = Handle<LocalVolTermStructure>(boost::make_shared<AndreasenHugeLocalVolAdapter>(ah));
            DLOG("Andreasen-Huge local vol calibration for process #"
                 << l
                 << ": "
                    "calibration error min="
                 << std::scientific << std::setprecision(6) << boost::get<0>(ah->calibrationError()) << " max="
                 << boost::get<1>(ah->calibrationError()) << " avg=" << boost::get<2>(ah->calibrationError()));
        } else if (lvType_ == Type::Dupire) {
            localVol = Handle<LocalVolTermStructure>(
                boost::make_shared<LocalVolSurface>(processes_[l]->blackVolatility(), processes_[l]->riskFreeRate(),
                                                    processes_[l]->dividendYield(), processes_[l]->stateVariable()));
        } else {
            QL_FAIL("unexpected local vol type");
        }

        processes.push_back(boost::make_shared<GeneralizedBlackScholesProcess>(
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
            Real atmMarketVol = p->blackVolatility()->blackVol(t, atmLevel);
            for (auto const m : calibrationMoneyness_) {
                Real strike = atmLevel * std::exp(m * atmMarketVol * std::sqrt(t));
                volTimesStrikes.back().push_back(std::make_pair(t, strike));
            }
        }
    }
    return volTimesStrikes;
}

} // namespace data
} // namespace oreplus
