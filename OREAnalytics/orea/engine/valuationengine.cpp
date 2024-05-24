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

#include <ored/portfolio/optionwrapper.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/portfolio/structuredtradeerror.hpp>
#include <ored/utilities/dategrid.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/progressbar.hpp>
#include <ored/utilities/to_string.hpp>

#include <ql/errors.hpp>

#include <boost/timer/timer.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;
using namespace ore::data;
using boost::timer::cpu_timer;
using boost::timer::default_places;

namespace ore {
namespace analytics {

ValuationEngine::ValuationEngine(const Date& today, const QuantLib::ext::shared_ptr<DateGrid>& dg,
                                 const QuantLib::ext::shared_ptr<SimMarket>& simMarket,
                                 const set<std::pair<string, QuantLib::ext::shared_ptr<ModelBuilder>>>& modelBuilders)
    : today_(today), dg_(dg), simMarket_(simMarket), modelBuilders_(modelBuilders) {

    QL_REQUIRE(dg_->size() > 0, "Error, DateGrid size must be > 0");
    QL_REQUIRE(today <= dg_->dates().front(), "ValuationEngine: Error today ("
                                                  << today << ") must not be later than first DateGrid date "
                                                  << dg_->dates().front());
    QL_REQUIRE(simMarket_, "ValuationEngine: Error, Null SimMarket");
}

void ValuationEngine::recalibrateModels() {
    ObservationMode::Mode om = ObservationMode::instance().mode();
    for (auto const& b : modelBuilders_) {
        if (om == ObservationMode::Mode::Disable)
            b.second->forceRecalculate();
        b.second->recalibrate();
    }
}

void ValuationEngine::buildCube(const QuantLib::ext::shared_ptr<data::Portfolio>& portfolio,
                                QuantLib::ext::shared_ptr<analytics::NPVCube> outputCube,
                                vector<QuantLib::ext::shared_ptr<ValuationCalculator>> calculators, bool mporStickyDate,
                                QuantLib::ext::shared_ptr<analytics::NPVCube> outputCubeNettingSet,
                                QuantLib::ext::shared_ptr<analytics::NPVCube> outputCptyCube,
                                vector<QuantLib::ext::shared_ptr<CounterpartyCalculator>> cptyCalculators, bool dryRun) {

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
    Real updateTime = 0.0;
    Real pricingTime = 0.0;
    Real fixingTime = 0.0;

    LOG("Initialise " << calculators.size() << " valuation calculators");
    for (auto const& c : calculators) {
        c->init(portfolio, simMarket_);
        c->initScenario();
    }

    // Loop is Samples, Dates, Trades
    const auto& dates = dg_->dates();
    const auto& trades = portfolio->trades();
    auto& counterparties = outputCptyCube ? outputCptyCube->idsAndIndexes() : std::map<string, Size>();
    std::vector<bool> tradeHasError(portfolio->size(), false);
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
            tradeHasError[i] = true;
        }

        if (om == ObservationMode::Mode::Unregister) {
            for (const Leg& leg : trade->legs()) {
                for (Size n = 0; n < leg.size(); n++) {
                    QuantLib::ext::shared_ptr<FloatingRateCoupon> frc = QuantLib::ext::dynamic_pointer_cast<FloatingRateCoupon>(leg[n]);
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

    cpu_timer timer;
    cpu_timer loopTimer;
    Size nTrades = trades.size();

    // We call Cube::samples() each time here to allow for dynamic stopping times
    // e.g. MC convergence tests
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
                double priceTime = 0;
                double upTime = 0;
                ++cubeDateIndex;
                Date valueDate = dg_->valuationDates()[i];
                Date closeOutDate = dg_->closeOutDateFromValuationDate(valueDate);
                std::tie(priceTime, upTime) = populateCube(
                    valueDate, cubeDateIndex, sample, true, false, scenarioUpdated, trades, tradeHasError, calculators,
                    outputCube, outputCubeNettingSet, counterparties, cptyCalculators, outputCptyCube);
                pricingTime += priceTime;
                updateTime += upTime;
                if(closeOutDate != Date()){
                    std::tie(priceTime, upTime) = populateCube(
                        closeOutDate, cubeDateIndex, sample, false, mporStickyDate, scenarioUpdated, trades, tradeHasError,
                        calculators, outputCube, outputCubeNettingSet, counterparties, cptyCalculators, outputCptyCube);
                    pricingTime += priceTime;
                    updateTime += upTime;
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
                    double priceTime = 0;
                    double upTime = 0;
                    QL_REQUIRE(closeOutDateToValueDateIndex.count(d) == 1 && !closeOutDateToValueDateIndex[d].empty(),
                               "Need to calculate valuation date before close out date");
                    for (size_t& valueDateIndex : closeOutDateToValueDateIndex[d]) {
                        std::tie(priceTime, upTime) =
                            populateCube(d, valueDateIndex, sample, false, mporStickyDate, scenarioUpdated, trades,
                                         tradeHasError, calculators, outputCube, outputCubeNettingSet, counterparties,
                                         cptyCalculators, outputCptyCube);
                        pricingTime += priceTime;
                        updateTime += upTime;
                        scenarioUpdated = true;
                    }
                }
                if (dg_->isValuationDate()[i]) {
                    double priceTime = 0;
                    double upTime = 0;
                    ++cubeDateIndex;
                    Date closeOutDate = dg_->closeOutDateFromValuationDate(d);
                    if(closeOutDate != Date())
                        closeOutDateToValueDateIndex[closeOutDate].push_back(cubeDateIndex);
                    std::tie(priceTime, upTime) = populateCube(
                        d, cubeDateIndex, sample, true, false, scenarioUpdated, trades, tradeHasError, calculators,
                        outputCube, outputCubeNettingSet, counterparties, cptyCalculators, outputCptyCube);
                    pricingTime += priceTime;
                    updateTime += upTime;
                    scenarioUpdated = true;
                }
            }
        }

        std::ostringstream detail;
        detail << nTrades << " trade" << (nTrades == 1 ? "" : "s") << ", " << outputCube->samples() << " sample"
               << (outputCube->samples() == 1 ? "" : "s");
        updateProgress(sample * nTrades, outputCube->samples() * nTrades, detail.str());

        timer.start();
        simMarket_->fixingManager()->reset();
        fixingTime += timer.elapsed().wall * 1e-9;
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

    std::ostringstream detail;
    detail << nTrades << " trade" << (nTrades == 1 ? "" : "s") << ", " << outputCube->samples() << " sample"
           << (outputCube->samples() == 1 ? "" : "s");
    updateProgress(outputCube->samples() * nTrades, outputCube->samples() * nTrades, detail.str());
    loopTimer.stop();
    LOG("ValuationEngine completed: loop " << setprecision(2) << loopTimer.format(2, "%w") << " sec, "
                                           << "pricing " << pricingTime << " sec, "
                                           << "update " << updateTime << " sec "
                                           << "fixing " << fixingTime);

    // for trades with errors set all output cube values to zero
    i = 0;
    for (auto& [tradeId, trade] : trades) {
        if (tradeHasError[i]) {
            ALOG("setting all results in output cube to zero for trade '"
                 << tradeId << "' since there was at least one error during simulation");
            outputCube->remove(i);
        }
        i++;
    }
}

void ValuationEngine::runCalculators(bool isCloseOutDate, const std::map<std::string, QuantLib::ext::shared_ptr<Trade>>& trades,
                                     std::vector<bool>& tradeHasError,
                                     const std::vector<QuantLib::ext::shared_ptr<ValuationCalculator>>& calculators,
                                     QuantLib::ext::shared_ptr<analytics::NPVCube>& outputCube,
                                     QuantLib::ext::shared_ptr<analytics::NPVCube>& outputCubeNettingSet, const Date& d,
                                     const Size cubeDateIndex, const Size sample, const string& label) {
    ObservationMode::Mode om = ObservationMode::instance().mode();
    for (auto& calc : calculators)
        calc->initScenario();
    // loop over trades
    size_t j = 0;
    for (auto tradeIt = trades.begin(); tradeIt != trades.end(); ++tradeIt, ++j) {
        auto trade = tradeIt->second;
        if (tradeHasError[j]) {
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
            tradeHasError[j] = true;
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

void ValuationEngine::tradeExercisable(bool enable, const std::map<std::string, QuantLib::ext::shared_ptr<Trade>>& trades) {
    for (const auto& [tradeId, trade] : trades) {
        auto t = QuantLib::ext::dynamic_pointer_cast<OptionWrapper>(trade->instrument());
        if (t != nullptr) {
            if (enable)
                t->enableExercise();
            else
                t->disableExercise();
        }
    }
}

std::pair<double, double> ValuationEngine::populateCube(
    const QuantLib::Date& d, size_t cubeDateIndex, size_t sample, bool isValueDate, bool isStickyDate,
    bool scenarioUpdated, const std::map<std::string, QuantLib::ext::shared_ptr<Trade>>& trades,
    std::vector<bool>& tradeHasError, const std::vector<QuantLib::ext::shared_ptr<ValuationCalculator>>& calculators,
    QuantLib::ext::shared_ptr<analytics::NPVCube>& outputCube, QuantLib::ext::shared_ptr<analytics::NPVCube>& outputCubeNettingSet,
    const std::map<string, Size>& counterparties,
    const vector<QuantLib::ext::shared_ptr<CounterpartyCalculator>>& cptyCalculators,
    QuantLib::ext::shared_ptr<analytics::NPVCube>& outputCptyCube) {
    double pricingTime = 0;
    double updateTime = 0;
    QL_REQUIRE(cubeDateIndex >= 0, "first date should be a valuation date");
    cpu_timer timer;
    timer.start();
    simMarket_->preUpdate();
    if (isValueDate || !isStickyDate) {
        simMarket_->updateDate(d);
    }
    // We can skip this step, if we have done that above in the close-out date section
    if (!scenarioUpdated) {
        simMarket_->updateScenario(d);
    }
    // Always with fixing update here, in contrast to the close-out date section
    simMarket_->postUpdate(d, !isStickyDate || isValueDate);
    // Aggregation scenario data update on valuation dates only
    if (isValueDate) {
        simMarket_->updateAsd(d);
    }
    recalibrateModels();

    timer.stop();
    updateTime += timer.elapsed().wall * 1e-9;

    timer.start();
    if (isStickyDate && !isValueDate) // switch on again, if sticky
        tradeExercisable(false, trades);
    // loop over trades
    runCalculators(!isValueDate, trades, tradeHasError, calculators, outputCube, outputCubeNettingSet, d, cubeDateIndex,
                   sample, simMarket_->label());
    if (isStickyDate && !isValueDate) // switch on again, if sticky
        tradeExercisable(true, trades);
    // loop over counterparty names
    if (isValueDate) {
        runCalculators(false, counterparties, cptyCalculators, outputCptyCube, d, cubeDateIndex, sample);
    }
    timer.stop();
    pricingTime += timer.elapsed().wall * 1e-9;
    return std::make_pair(pricingTime, updateTime);
}

} // namespace analytics
} // namespace ore
