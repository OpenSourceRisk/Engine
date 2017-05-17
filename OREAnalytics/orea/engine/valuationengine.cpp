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

#include <boost/timer.hpp>
#include <ql/errors.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;
using namespace ore::data;

namespace ore {
namespace analytics {

ValuationEngine::ValuationEngine(const Date& today, const boost::shared_ptr<DateGrid>& dg,
                                 const boost::shared_ptr<SimMarket>& simMarket,
                                 const set<boost::shared_ptr<ModelBuilder>>& modelBuilders)
    : today_(today), dg_(dg), simMarket_(simMarket), modelBuilders_(modelBuilders) {

    QL_REQUIRE(dg_->size() > 0, "Error, DateGrid size must be > 0");
    QL_REQUIRE(today <= dg_->dates().front(),
               "ValuationEngine: Error today (" << today << ") must not be later than first DateGrid date "
                                                << dg_->dates().front());
    QL_REQUIRE(simMarket_, "ValuationEngine: Error, Null SimMarket");
}

void ValuationEngine::buildCube(const boost::shared_ptr<data::Portfolio>& portfolio,
                                boost::shared_ptr<analytics::NPVCube>& outputCube,
                                vector<boost::shared_ptr<ValuationCalculator>> calculators) {

    QL_REQUIRE(portfolio->size() > 0, "ValuationEngine: Error portfolio is empty");

    QL_REQUIRE(outputCube->numIds() == portfolio->trades().size(),
               "cube x dimension (" << outputCube->numIds() << ") "
                                    << "different from portfolio size (" << portfolio->trades().size() << ")");

    QL_REQUIRE(outputCube->numDates() == dg_->dates().size(),
               "cube y dimension (" << outputCube->numDates() << ") "
                                    << "different from number of time steps (" << dg_->dates().size() << ")");

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

    boost::timer timer;
    boost::timer loopTimer;

    // We call Cube::samples() each time her to allow for dynamic stopping times
    // e.g. MC convergence tests
    for (Size sample = 0; sample < outputCube->samples(); ++sample) {
        updateProgress(sample, outputCube->samples());

        for (auto& trade : trades)
            trade->instrument()->reset();

        // loop over Dates
        for (Size i = 0; i < dates.size(); ++i) {
            Date d = dates[i];

            timer.restart();

            simMarket_->update(d);

            // recalibrate models
            for (auto const& b : modelBuilders_) {
                if (om == ObservationMode::Mode::Disable)
                    b->recalculate();
                b->recalibrate();
            }

            updateTime += timer.elapsed();

            // loop over trades
            timer.restart();
            for (Size j = 0; j < trades.size(); ++j) {
                auto trade = trades[j];

                // We can avoid checking mode here and always call updateQlInstruments()
                if (om == ObservationMode::Mode::Disable)
                    trade->instrument()->updateQlInstruments();

                for (auto calc : calculators)
                    calc->calculate(trade, j, simMarket_, outputCube, d, i, sample);
            }
            pricingTime += timer.elapsed();
        }

        timer.restart();
        simMarket_->fixingManager()->reset();
        fixingTime += timer.elapsed();
    }

    updateProgress(outputCube->samples(), outputCube->samples());
    LOG("ValuationEngine completed: loop " << setprecision(2) << loopTimer.elapsed() << " sec, "
                                           << "pricing " << pricingTime << " sec, "
                                           << "update " << updateTime << " sec "
                                           << "fixing " << fixingTime);
}
}
}
