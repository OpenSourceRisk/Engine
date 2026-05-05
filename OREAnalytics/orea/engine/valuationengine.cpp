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

#include <orea/cube/npvcube.hpp>
#include <orea/engine/cptycalculator.hpp>
#include <orea/engine/observationmode.hpp>
#include <orea/engine/valuationcalculator.hpp>
#include <orea/engine/valuationengine.hpp>
#include <orea/simulation/simmarket.hpp>
#include <orea/scenario/scenariosimmarket.hpp>

#include <ored/portfolio/compositeinstrumentwrapper.hpp>
#include <ored/portfolio/optionwrapper.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/portfolio/structuredtradeerror.hpp>
#include <ored/utilities/dategrid.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/progressbar.hpp>
#include <ored/utilities/to_string.hpp>
#include <ored/utilities/osutils.hpp>

#include <ql/errors.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;
using namespace ore::data;

namespace ore {
namespace analytics {

ValuationEngine::ValuationEngine(const Date& today, const QuantLib::ext::shared_ptr<DateGrid>& dg,
                                 const QuantLib::ext::shared_ptr<SimMarket>& simMarket,
                                 const set<std::pair<string, QuantLib::ext::shared_ptr<ModelBuilder>>>& modelBuilders,
                                 const bool recalibrate)
    : today_(today), dg_(dg), simMarket_(simMarket), modelBuilders_(modelBuilders), recalibrate_(recalibrate) {

    QL_REQUIRE(dg_->size() > 0, "Error, DateGrid size must be > 0");
    QL_REQUIRE(today <= dg_->dates().front(), "ValuationEngine: Error today ("
                                                  << today << ") must not be later than first DateGrid date "
                                                  << dg_->dates().front());
    QL_REQUIRE(simMarket_, "ValuationEngine: Error, Null SimMarket");
}

void ValuationEngine::recalibrateModels() {
    ObservationMode::Mode om = ObservationMode::instance().mode();
    for (auto const& b : modelBuilders_) {
        if (recalibrate_) {
            if (om == ObservationMode::Mode::Disable)
                b.second->forceRecalculate();
            else
                b.second->recalibrate();
        } else
            b.second->newCalcWithoutRecalibration();
    }
}

void ValuationEngine::buildCube(const QuantLib::ext::shared_ptr<data::Portfolio>& portfolio,
                                QuantLib::ext::shared_ptr<analytics::NPVCube> outputCube,
                                vector<QuantLib::ext::shared_ptr<ValuationCalculator>> calculators,
                                const ErrorPolicy errorPolicy, bool mporStickyDate,
                                QuantLib::ext::shared_ptr<analytics::NPVCube> outputCubeNettingSet,
                                QuantLib::ext::shared_ptr<analytics::NPVCube> outputCptyCube,
                                vector<QuantLib::ext::shared_ptr<CounterpartyCalculator>> cptyCalculators, bool dryRun,
                                Errors* errors) {

    struct SimMarketResetter {
        SimMarketResetter(QuantLib::ext::shared_ptr<SimMarket> simMarket) : simMarket_(simMarket) {}
        ~SimMarketResetter() { simMarket_->reset(); }
        QuantLib::ext::shared_ptr<SimMarket> simMarket_;
    } simMarketResetter(simMarket_);

    LOG("Build cube with mporStickyDate=" << mporStickyDate << ", dryRun=" << std::boolalpha << dryRun);

    QL_REQUIRE(portfolio->size() > 0, "ValuationEngine: Error portfolio is empty");

    QL_REQUIRE(outputCube->numIds() == portfolio->trades().size(),
               "cube x dimension (" << outputCube->numIds() << ") "
                                    << "different from portfolio size (" << portfolio->trades().size() << ")");

    QL_REQUIRE(outputCube->numDates() == dg_->valuationDates().size(),
               "cube y dimension (" << outputCube->numDates() << ") "
                                    << "different from number of valuation dates (" << dg_->valuationDates().size()
                                    << ")");

    if (outputCptyCube) {
        QL_REQUIRE(outputCptyCube->numIds() == portfolio->counterparties().size() + 1,
                   "cptyCube x dimension (" << outputCptyCube->numIds() << "minus 1) "
                                            << "different from portfolio counterparty size ("
                                            << portfolio->counterparties().size() << ")");

        QL_REQUIRE(outputCptyCube->numDates() == dg_->dates().size(), "outputCptyCube y dimension ("
                                                                          << outputCptyCube->numDates() << ") "
                                                                          << "different from number of time steps ("
                                                                          << dg_->dates().size() << ")");
    }

    LOG("Starting ValuationEngine for " << portfolio->size() << " trades, " << outputCube->samples() << " samples and "
                                        << dg_->size() << " dates.");

    ObservationMode::Mode om = ObservationMode::instance().mode();

    Timings timings;

    LOG("Initialise " << calculators.size() << " valuation calculators");
    for (auto const& c : calculators) {
        c->init(portfolio, simMarket_);
        c->initScenario();
    }

    // Loop is Samples, Dates, Trades
    const auto& dates = dg_->dates();
    const auto& trades = portfolio->trades();
    auto& counterparties = outputCptyCube ? outputCptyCube->idsAndIndexes() : std::map<string, Size>();
    std::vector<bool> tradeHasT0Error(portfolio->size(), false);
    std::vector<bool> tradeHasSampleError(portfolio->size(), false);

    std::vector<QuantLib::ext::shared_ptr<OptionWrapper>> optionWrappers;
    for (auto const& [id, t] : trades) {
        std::vector<QuantLib::ext::shared_ptr<InstrumentWrapper>> wrapperStack{t->instrument()};
        while (!wrapperStack.empty()) {
            auto w = wrapperStack.back();
            wrapperStack.pop_back();
            if (auto comp = QuantLib::ext::dynamic_pointer_cast<CompositeInstrumentWrapper>(w)) {
                for (auto const& cw : comp->wrappers())
                    wrapperStack.push_back(cw);
            } else if (auto o = QuantLib::ext::dynamic_pointer_cast<OptionWrapper>(w)) {
                optionWrappers.push_back(o);
            }
        }
    }

    LOG("Initialise state objects...");
    // initialise state objects for each trade (required for path-dependent derivatives in particular)
    size_t i = 0;
    for (const auto& [tradeId, trade] : trades) {
        QL_REQUIRE(!trade->npvCurrency().empty(), "NPV currency not set for trade " << trade->id());

        DLOG("Initialise wrapper for trade " << trade->id());
        trade->instrument()->initialise(dates);

        recalibrateModels();

        // T0 values
        try {
            for (auto& calc : calculators)
                calc->calculateT0(trade, i, simMarket_, outputCube, outputCubeNettingSet);
        } catch (const std::exception& e) {
            string expMsg = string("T0 valuation error: ") + e.what();
            StructuredTradeErrorMessage(tradeId, trade->tradeType(), "ScenarioValuation", expMsg.c_str()).log();
            tradeHasT0Error[i] = true;
            if (errors)
                errors->t0.insert(i);
        }

        if (om == ObservationMode::Mode::Unregister) {
            for (const Leg& leg : trade->legs()) {
                for (Size n = 0; n < leg.size(); n++) {
                    QuantLib::ext::shared_ptr<FloatingRateCoupon> frc =
                        QuantLib::ext::dynamic_pointer_cast<FloatingRateCoupon>(leg[n]);
                    if (frc) {
                        frc->unregisterWith(frc->index());
                        trade->instrument()->qlInstrument()->unregisterWith(frc);
                        // Unregister with eval dates
                        frc->unregisterWith(Settings::instance().evaluationDate());
                        frc->index()->unregisterWith(Settings::instance().evaluationDate());
                        trade->instrument()->qlInstrument()->unregisterWith(Settings::instance().evaluationDate());
                    }
                }
            }
        }
        ++i;
    }
    LOG("Total number of trades = " << portfolio->size());

    if (!dates.empty() && dates.front() > simMarket_->asofDate()) {
        // the fixing manager is only required if sim dates contain future dates
        simMarket_->fixingManager()->initialise(portfolio, simMarket_);
    }

    Size nTrades = trades.size();

    // We call Cube::samples() each time here to allow for dynamic stopping times
    // e.g. MC convergence tests

    auto loopTimeStart = data::os::nanosecondsClock();

    for (Size sample = 0; sample < (dryRun ? std::min<Size>(1, outputCube->samples()) : outputCube->samples());
         ++sample) {
        TLOG("ValuationEngine: apply scenario sample #" << sample);

        for (auto& [tradeId, trade] : portfolio->trades())
            trade->instrument()->reset();

        // loop over Dates, increase cubeDateIndex for each valuation date we hit
        int cubeDateIndex = -1;
        if (!dg_->closeOutDates().empty() && mporStickyDate) {
            // loop over dates and always do value date and close out date in one run
            const bool scenarioUpdated = false;
            for (size_t i = 0; i < dg_->valuationDates().size(); ++i) {
                ++cubeDateIndex;
                Date valueDate = dg_->valuationDates()[i];
                Date closeOutDate = dg_->closeOutDateFromValuationDate(valueDate);
                populateCube(valueDate, cubeDateIndex, sample, true, false, scenarioUpdated, trades, optionWrappers,
                             errorPolicy, tradeHasT0Error, tradeHasSampleError, calculators, outputCube,
                             outputCubeNettingSet, counterparties, cptyCalculators, outputCptyCube, errors, timings);
                if (closeOutDate != Date()) {
                    populateCube(closeOutDate, cubeDateIndex, sample, false, mporStickyDate, scenarioUpdated, trades,
                                 optionWrappers, errorPolicy, tradeHasT0Error, tradeHasSampleError, calculators,
                                 outputCube, outputCubeNettingSet, counterparties, cptyCalculators, outputCptyCube,
                                 errors, timings);
                }
            }
        } else {
            std::map<Date, std::vector<size_t>> closeOutDateToValueDateIndex;
            for (Size i = 0; i < dates.size(); ++i) {
                Date d = dates[i];
                // Process auxiliary close-out dates first (may coincide with a valuation date, see below)
                // Store result at same cubeDateIndex as the corresponding valuation date's result, but at different
                // cube depth Differences to valuation date processing above: Update valuation date and fixings, trades
                // exercisable depending on stickiness
                bool scenarioUpdated = false;
                if (dg_->isCloseOutDate()[i]) {
                    QL_REQUIRE(closeOutDateToValueDateIndex.count(d) == 1 && !closeOutDateToValueDateIndex[d].empty(),
                               "Need to calculate valuation date before close out date");
                    for (size_t& valueDateIndex : closeOutDateToValueDateIndex[d]) {
                        populateCube(d, valueDateIndex, sample, false, mporStickyDate, scenarioUpdated, trades,
                                     optionWrappers, errorPolicy, tradeHasT0Error, tradeHasSampleError, calculators,
                                     outputCube, outputCubeNettingSet, counterparties, cptyCalculators, outputCptyCube,
                                     errors, timings);
                        scenarioUpdated = true;
                    }
                }
                if (dg_->isValuationDate()[i]) {
                    ++cubeDateIndex;
                    Date closeOutDate = dg_->closeOutDateFromValuationDate(d);
                    if (closeOutDate != Date())
                        closeOutDateToValueDateIndex[closeOutDate].push_back(cubeDateIndex);
                    populateCube(d, cubeDateIndex, sample, true, false, scenarioUpdated, trades, optionWrappers,
                                 errorPolicy, tradeHasT0Error, tradeHasSampleError, calculators, outputCube,
                                 outputCubeNettingSet, counterparties, cptyCalculators, outputCptyCube, errors,
                                 timings);
                    scenarioUpdated = true;
                }
            }
        }

        std::ostringstream detail;
        detail << nTrades << " trade" << (nTrades == 1 ? "" : "s") << ", " << outputCube->samples() << " sample"
               << (outputCube->samples() == 1 ? "" : "s");
        updateProgress(sample * nTrades, outputCube->samples() * nTrades, detail.str());

        auto fixingTimeStart = data::os::nanosecondsClock();
        simMarket_->fixingManager()->reset();
        timings.fixingTime += data::os::nanosecondsClock() - fixingTimeStart;
    }

    if (dryRun) {
        LOG("Doing a dry run - fill remaining cube with random values.");
        for (Size sample = 1; sample < outputCube->samples(); ++sample) {
            for (Size i = 0; i < dates.size(); ++i) {
                for (Size j = 0; j < trades.size(); ++j) {
                    for (Size d = 0; d < outputCube->depth(); ++d) {
                        // add some noise, but only for the first few samples, so that e.g.
                        // a sensi run is not polluted with too many sensis for each trade
                        Real noise = sample < 10 ? static_cast<Real>(i + j + d + sample) : 0.0;
                        outputCube->set(outputCube->getT0(j, d) + noise, j, i, sample, d);
                    }
                }
            }
        }
    }

    timings.loopTime += data::os::nanosecondsClock() - loopTimeStart;

    std::ostringstream detail;
    detail << nTrades << " trade" << (nTrades == 1 ? "" : "s") << ", " << outputCube->samples() << " sample"
           << (outputCube->samples() == 1 ? "" : "s");
    updateProgress(outputCube->samples() * nTrades, outputCube->samples() * nTrades, detail.str());
    if(auto ssm = ext::dynamic_pointer_cast<ScenarioSimMarket>(simMarket_)) {
        timings.scenGenTime += ssm->scenarioGenerator()->timing();
    }
    LOG("ValuationEngine completed:");
    LOG("Loop              : " << static_cast<double>(timings.loopTime) * 1E-3 << " mus");
    LOG("  UpdateDate      : " << static_cast<double>(timings.updateDateTime) * 1E-3 << " mus");
    LOG("  UpdateScenario  : " << static_cast<double>(timings.updateScenarioTime) * 1E-3 << " mus");
    LOG("    ScenGen       : " << static_cast<double>(timings.scenGenTime) * 1E-3 << " mus");
    LOG("    ApplyScen     : " << static_cast<double>(timings.updateScenarioTime - timings.scenGenTime) * 1E-3 << " mus");
    LOG("  Refresh         : " << static_cast<double>(timings.refreshTime) * 1E-3 << " mus");
    LOG("  Asd             : " << static_cast<double>(timings.asdTime) * 1E-3 << " mus");
    LOG("  Pricing         : " << static_cast<double>(timings.pricingTime) * 1E-3 << " mus");
    LOG("  Fixing          : " << static_cast<double>(timings.fixingTime) * 1E-3 << " mus");
    LOG("  Calibration     : " << static_cast<double>(timings.calibrationTime) * 1E-3 << " mus");
    LOG("  Residual        : " << static_cast<double>(timings.loopTime - timings.updateDateTime -
                                                      timings.updateScenarioTime - timings.refreshTime -
                                                      timings.asdTime - timings.pricingTime - timings.fixingTime -
                                                      timings.calibrationTime) *
                                      1E-3
                               << " mus");

    // for trades with errors set output cube values to zero depending on chosen error policy
    i = 0;
    for (auto& [tradeId, trade] : trades) {
        if (tradeHasT0Error[i] || (tradeHasSampleError[i] && errorPolicy == ErrorPolicy::RemoveAll)) {
            LOG("Setting all results in output cube to zero for trade '"
                << tradeId << "'. Trade has t0 valuation error: " << std::boolalpha << tradeHasT0Error[i]
                << ". Trade has sample valuation error: " << tradeHasSampleError[i]
                << ". Error Policy is RemoveAll: " << (errorPolicy == ErrorPolicy::RemoveAll) << ".");
            outputCube->removeT0(i);
            outputCube->remove(i, Null<Size>(), false);
        }
        i++;
    }
}

void ValuationEngine::runCalculators(bool isCloseOutDate,
                                     const std::map<std::string, QuantLib::ext::shared_ptr<Trade>>& trades,
                                     const ErrorPolicy errorPolicy, std::vector<bool>& tradeHasT0Error,
                                     std::vector<bool>& tradeHasSampleError,
                                     const std::vector<QuantLib::ext::shared_ptr<ValuationCalculator>>& calculators,
                                     QuantLib::ext::shared_ptr<analytics::NPVCube>& outputCube,
                                     QuantLib::ext::shared_ptr<analytics::NPVCube>& outputCubeNettingSet, const Date& d,
                                     const Size cubeDateIndex, const Size sample, const string& label,
                                     Errors* errors) {
    ObservationMode::Mode om = ObservationMode::instance().mode();
    for (auto& calc : calculators)
        calc->initScenario();
    // loop over trades
    size_t j = 0;
    for (auto tradeIt = trades.begin(); tradeIt != trades.end(); ++tradeIt, ++j) {
        auto trade = tradeIt->second;
        if (tradeHasT0Error[j] || (tradeHasSampleError[j] && errorPolicy == ErrorPolicy::RemoveAll)) {
            outputCube->remove(j, sample, false);
            continue;
        }

        // We can avoid checking mode here and always call updateQlInstruments()
        if (om == ObservationMode::Mode::Disable || om == ObservationMode::Mode::Unregister)
            trade->instrument()->updateQlInstruments();
        try {
            for (auto& calc : calculators)
                calc->calculate(trade, j, simMarket_, outputCube, outputCubeNettingSet, d, cubeDateIndex, sample,
                                isCloseOutDate);
        } catch (const std::exception& e) {
            string expMsg = "date = " + ore::data::to_string(io::iso_date(d)) +
                            ", sample = " + ore::data::to_string(sample) + ", label = " + label + ": " + e.what();
            StructuredTradeErrorMessage(trade->id(), trade->tradeType(), "ScenarioValuation", expMsg.c_str()).log();
            outputCube->remove(j, sample, errorPolicy == ErrorPolicy::RemoveSample);
            tradeHasSampleError[j] = true;
            if (errors)
                errors->samples.insert(std::make_pair(j, sample));
        }
    }
}

void ValuationEngine::runCalculators(bool isCloseOutDate, const std::map<std::string, Size>& counterparties,
                                     const std::vector<QuantLib::ext::shared_ptr<CounterpartyCalculator>>& calculators,
                                     QuantLib::ext::shared_ptr<analytics::NPVCube>& cptyCube, const Date& d,
                                     const Size cubeDateIndex, const Size sample) {
    // loop over counterparties
    for (const auto& [counterparty, idx] : counterparties) {
        for (auto& calc : calculators) {
            calc->calculate(counterparty, idx, simMarket_, cptyCube, d, cubeDateIndex, sample, isCloseOutDate);
        }
    }
}

namespace {
void tradeExercisable(bool enable, const std::vector<QuantLib::ext::shared_ptr<OptionWrapper>>& optionWrappers) {
    std::for_each(optionWrappers.begin(), optionWrappers.end(), [enable](const auto& t) {
        if (t != nullptr) {
            if (enable)
                t->enableExercise();
            else
                t->disableExercise();
        }
    });
}
} // namespace

void ValuationEngine::populateCube(
    const QuantLib::Date& d, size_t cubeDateIndex, size_t sample, bool isValueDate, bool isStickyDate,
    bool scenarioUpdated, const std::map<std::string, QuantLib::ext::shared_ptr<Trade>>& trades,
    const std::vector<QuantLib::ext::shared_ptr<OptionWrapper>>& optionWrappers, const ErrorPolicy errorPolicy,
    std::vector<bool>& tradeHasT0Error, std::vector<bool>& tradeHasSampleError,
    const std::vector<QuantLib::ext::shared_ptr<ValuationCalculator>>& calculators,
    QuantLib::ext::shared_ptr<analytics::NPVCube>& outputCube,
    QuantLib::ext::shared_ptr<analytics::NPVCube>& outputCubeNettingSet, const std::map<string, Size>& counterparties,
    const vector<QuantLib::ext::shared_ptr<CounterpartyCalculator>>& cptyCalculators,
    QuantLib::ext::shared_ptr<analytics::NPVCube>& outputCptyCube, Errors* errors, Timings& timings) {

    QL_REQUIRE(cubeDateIndex >= 0, "first date should be a valuation date");

    auto t0 = data::os::nanosecondsClock();
    simMarket_->preUpdate();
    if (isValueDate || !isStickyDate) {
        simMarket_->updateDate(d);
    }
    // We can skip this step, if we have done that above in the close-out date section
    auto t1 = data::os::nanosecondsClock();
    timings.updateDateTime += t1 - t0;
    if (!scenarioUpdated) {
        simMarket_->updateScenario(d);
    }

    auto t2 = data::os::nanosecondsClock();
    timings.updateScenarioTime += t2 - t1;

    simMarket_->postUpdate(d);
    auto t3 = data::os::nanosecondsClock();
    timings.refreshTime += t3 - t2;

    if (!isStickyDate || isValueDate)
        simMarket_->fixingManager()->update(d);
    auto t4 = data::os::nanosecondsClock();
    timings.fixingTime += t4 - t3;

    // Aggregation scenario data update on valuation dates only
    if (isValueDate) {
        simMarket_->updateAsd(d);
    }
    auto t5 = data::os::nanosecondsClock();
    timings.asdTime += t5 - t4;

    recalibrateModels();
    auto t6 = data::os::nanosecondsClock();
    timings.calibrationTime += t6 - t5;

    if (isStickyDate && !isValueDate) // switch on again, if sticky
        tradeExercisable(false, optionWrappers);
    // loop over trades
    runCalculators(!isValueDate, trades, errorPolicy, tradeHasT0Error, tradeHasSampleError, calculators, outputCube,
                   outputCubeNettingSet, d, cubeDateIndex, sample, simMarket_->label(), errors);
    if (isStickyDate && !isValueDate) // switch on again, if sticky
        tradeExercisable(true, optionWrappers);
    // loop over counterparty names
    if (isValueDate) {
        runCalculators(false, counterparties, cptyCalculators, outputCptyCube, d, cubeDateIndex, sample);
    }

    timings.pricingTime += data::os::nanosecondsClock() - t6;
}

} // namespace analytics
} // namespace ore
