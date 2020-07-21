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

#include <orea/engine/observationmode.hpp>
#include <orea/engine/valuationengine.hpp>
#include <orea/simulation/simmarket.hpp>
#include <ored/portfolio/optionwrapper.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/progressbar.hpp>

#include <boost/timer/timer.hpp>
#include <ql/errors.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;
using namespace ore::data;
using boost::timer::cpu_timer;
using boost::timer::default_places;

namespace ore {
namespace analytics {

ValuationEngine::ValuationEngine(const Date& today, const boost::shared_ptr<DateGrid>& dg,
                                 const boost::shared_ptr<SimMarket>& simMarket,
                                 const set<std::pair<string, boost::shared_ptr<ModelBuilder>>>& modelBuilders)
    : today_(today), dg_(dg), simMarket_(simMarket), modelBuilders_(modelBuilders) {

    QL_REQUIRE(dg_->size() > 0, "Error, DateGrid size must be > 0");
    QL_REQUIRE(today <= dg_->dates().front(), "ValuationEngine: Error today ("
                                                  << today << ") must not be later than first DateGrid date "
                                                  << dg_->dates().front());
    QL_REQUIRE(simMarket_, "ValuationEngine: Error, Null SimMarket");
}

void ValuationEngine::buildCube(const boost::shared_ptr<data::Portfolio>& portfolio,
                                boost::shared_ptr<analytics::NPVCube> outputCube,
                                vector<boost::shared_ptr<ValuationCalculator>> calculators, bool mporStickyDate) {

    LOG("Build cube with mporStickyDate=" << mporStickyDate);

    QL_REQUIRE(portfolio->size() > 0, "ValuationEngine: Error portfolio is empty");

    QL_REQUIRE(outputCube->numIds() == portfolio->trades().size(),
               "cube x dimension (" << outputCube->numIds() << ") "
                                    << "different from portfolio size (" << portfolio->trades().size() << ")");

    QL_REQUIRE(outputCube->numDates() == dg_->valuationDates().size(),
               "cube y dimension (" << outputCube->numDates() << ") "
                                    << "different from number of valuation dates (" << dg_->valuationDates().size() << ")");

    LOG("Starting ValuationEngine for " << portfolio->size() << " trades, " << outputCube->samples() << " samples and "
                                        << dg_->size() << " dates.");

    ObservationMode::Mode om = ObservationMode::instance().mode();
    Real updateTime = 0.0;
    Real pricingTime = 0.0;
    Real fixingTime = 0.0;

    // Loop is Samples, Dates, Trades
    const auto& dates = dg_->dates();
    const auto& trades = portfolio->trades();

    LOG("Initialise state objects...");
    Size numFRC = 0;
    // initialise state objects for each trade (required for path-dependent derivatives in particular)
    for (Size i = 0; i < trades.size(); i++) {
        QL_REQUIRE(trades[i]->npvCurrency() != "", "NPV currency not set for trade " << trades[i]->id());

        DLOG("Initialise wrapper for trade " << trades[i]->id());
        trades[i]->instrument()->initialise(dates);

        // T0 values
        for (auto calc : calculators)
            calc->calculateT0(trades[i], i, simMarket_, outputCube);

        if (om == ObservationMode::Mode::Unregister) {
            for (const Leg& leg : trades[i]->legs()) {
                for (Size n = 0; n < leg.size(); n++) {
                    boost::shared_ptr<FloatingRateCoupon> frc = boost::dynamic_pointer_cast<FloatingRateCoupon>(leg[n]);
                    if (frc) {
                        frc->unregisterWith(frc->index());
                        trades[i]->instrument()->qlInstrument()->unregisterWith(frc);
                        // Unregister with eval dates
                        frc->unregisterWith(Settings::instance().evaluationDate());
                        frc->index()->unregisterWith(Settings::instance().evaluationDate());
                        trades[i]->instrument()->qlInstrument()->unregisterWith(Settings::instance().evaluationDate());
                    }
                }
            }
        }
    }
    LOG("Total number of swaps = " << portfolio->size());
    LOG("Total number of FRC = " << numFRC);

    simMarket_->fixingManager()->initialise(portfolio);

    cpu_timer timer;
    cpu_timer loopTimer;

    // We call Cube::samples() each time her to allow for dynamic stopping times
    // e.g. MC convergence tests
    for (Size sample = 0; sample < outputCube->samples(); ++sample) {
        updateProgress(sample, outputCube->samples());

        for (auto& trade : trades)
            trade->instrument()->reset();

        // loop over Dates, increase cubeDateIndex for each valuation date we hit
        int cubeDateIndex = -1;
	for (Size i = 0; i < dates.size(); ++i) {
            Date d = dates[i];
	    
	    // Process auxiliary close-out dates first (may coincide with a valuation date, see below)
	    // Store result at same cubeDateIndex as the previous valuation date's result, but at different cube depth
	    // Differences to valuation date processing above:
	    // Update valuation date and fixings, trades exercisable depending on stickiness 
	    bool scenarioUpdated = false;
	    if (dg_->isCloseOutDate()[i]) {
		timer.start();

		// update market
		simMarket_->preUpdate();
		if (!mporStickyDate)
		    simMarket_->updateDate(d);
		simMarket_->updateScenario(d);
		scenarioUpdated = true;
		simMarket_->postUpdate(d, !mporStickyDate); // with fixings only if not sticky

		// recalibrate models
		for (auto const& b : modelBuilders_) {
		    if (om == ObservationMode::Mode::Disable)
		        b.second->forceRecalculate();
		    b.second->recalibrate();
		}

		timer.stop();
		updateTime += timer.elapsed().wall * 1e-9;

		// loop over trades
		timer.start();
		if (mporStickyDate) // switch off if sticky
                    tradeExercisable(false, trades);
		QL_REQUIRE(cubeDateIndex >= 0, "negative cube date index, ensure that the date grid starts with a valuation date");
                runCalculators(true, trades, calculators, outputCube, d, cubeDateIndex, sample);
		if (mporStickyDate) // switch on again, if sticky
                    tradeExercisable(true, trades);
		timer.stop();
		pricingTime += timer.elapsed().wall * 1e-9;
	    }

	    // process a valuation date as usual
	    if (dg_->isValuationDate()[i]) {
	        timer.start();

		cubeDateIndex++;

		// All the steps below from preUpdate() to updateAsd(d) are combined in update(d), but we decompose as follows
		// simMarket_->update(d); 		
		simMarket_->preUpdate();
		simMarket_->updateDate(d);
		// We can skip this step, if we have done that above in the close-out date section
		if (!scenarioUpdated)
		    simMarket_->updateScenario(d);
		// Always with fixing update here, in contrast to the close-out date section
		simMarket_->postUpdate(d, true); 
		// Aggregation scenario data update on valuation dates only 
		simMarket_->updateAsd(d);
	    
		// recalibrate models
		for (auto const& b : modelBuilders_) {
		    if (om == ObservationMode::Mode::Disable)
		        b.second->forceRecalculate();
		    b.second->recalibrate();
		}

		timer.stop();
		updateTime += timer.elapsed().wall * 1e-9;

		// loop over trades
		timer.start();
		runCalculators(false, trades, calculators, outputCube, d, cubeDateIndex, sample);
		timer.stop();
		pricingTime += timer.elapsed().wall * 1e-9;
	    }

	}
	
        timer.start();
        simMarket_->fixingManager()->reset();
        fixingTime += timer.elapsed().wall * 1e-9;
    }

    simMarket_->reset();
    updateProgress(outputCube->samples(), outputCube->samples());
    loopTimer.stop();
    LOG("ValuationEngine completed: loop " << setprecision(2) << loopTimer.format(2, "%w") << " sec, "
                                           << "pricing " << pricingTime << " sec, "
                                           << "update " << updateTime << " sec "
                                           << "fixing " << fixingTime);
}

void ValuationEngine::runCalculators(bool isCloseOutDate, const std::vector<boost::shared_ptr<Trade>>& trades,
                                     const std::vector<boost::shared_ptr<ValuationCalculator>>& calculators,
                                     boost::shared_ptr<analytics::NPVCube>& outputCube, const Date& d,
                                     const Size cubeDateIndex, const Size sample) {
    ObservationMode::Mode om = ObservationMode::instance().mode();
    // loop over trades
    for (Size j = 0; j < trades.size(); ++j) {
        auto trade = trades[j];
        // We can avoid checking mode here and always call updateQlInstruments()
        if (om == ObservationMode::Mode::Disable)
            trade->instrument()->updateQlInstruments();
        for (auto calc : calculators)
            calc->calculate(trade, j, simMarket_, outputCube, d, cubeDateIndex, sample, isCloseOutDate);
    }

}

void ValuationEngine::tradeExercisable(bool enable, const std::vector<boost::shared_ptr<Trade>>& trades) {
    for (Size j = 0; j < trades.size(); ++j) {
        auto t = boost::dynamic_pointer_cast<OptionWrapper>(trades[j]->instrument());
        if (t != nullptr) {
            if (enable)
                t->enableExercise();
            else
                t->disableExercise();
        }
    }
}
} // namespace analytics
} // namespace ore
