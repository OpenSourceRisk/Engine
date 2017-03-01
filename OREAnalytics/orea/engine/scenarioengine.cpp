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

#include <orea/engine/scenarioengine.hpp>
#include <orea/engine/observationmode.hpp>
#include <orea/simulation/simmarket.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/portfolio/optionwrapper.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/progressbar.hpp>

#include <ql/errors.hpp>
#include <ql/cashflows/iborcoupon.hpp>
#include <boost/timer.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;
using namespace ore::data;

namespace ore {
namespace analytics {

ScenarioEngine::ScenarioEngine(const Date& today, const boost::shared_ptr<SimMarket>& simMarket,
			       string baseCurrency)
    : today_(today), simMarket_(simMarket), baseCurrency_(baseCurrency) {
    QL_REQUIRE(simMarket_, "ValuationEngine: Error, Null SimMarket");
}

void ScenarioEngine::buildCube(const boost::shared_ptr<data::Portfolio>& portfolio,
			       boost::shared_ptr<analytics::NPVCube>& outputCube) {

    QL_REQUIRE(portfolio->size() > 0, "ValuationEngine: Error portfolio is empty");

    QL_REQUIRE(outputCube->numIds() == portfolio->trades().size(),
               "cube x dimension (" << outputCube->numIds() << ") "
                                    << "different from portfolio size (" << portfolio->trades().size() << ")");

    QL_REQUIRE(outputCube->numDates() == 1,
               "cube y dimension (" << outputCube->numDates() << ") "
                                    << "expected to be 1");

    LOG("Starting ScenarioEngine for " << portfolio->size() << " trades and " << outputCube->samples() << " samples");
    
    // Check observation mode
    QL_REQUIRE(ObservationMode::instance().mode() == ObservationMode::Mode::None,
	       "Observation mode None required");
    
    Real updateTime = 0.0;
    Real pricingTime = 0.0;

    const auto& trades = portfolio->trades();

    // Store t0 NPVs
    for (Size i = 0; i < trades.size(); i++) {
        QL_REQUIRE(trades[i]->npvCurrency() != "", "NPV currency not set for trade " << trades[i]->id());
	Real fx = simMarket_->fxSpot(trades[i]->npvCurrency() + baseCurrency_)->value();
        Real npv = trades[i]->instrument()->NPV() * fx;
        outputCube->setT0(npv, i, 0); 
    }

    // First call to update() resets the sim market to the base scenario (number 0)
    simMarket_->update(today_); 
    
    // Check t0 NPVs after market update to base scenario, should match
    for (Size i = 0; i < trades.size(); i++) {
        QL_REQUIRE(trades[i]->npvCurrency() != "", "NPV currency not set for trade " << trades[i]->id());
	Real fx = simMarket_->fxSpot(trades[i]->npvCurrency() + baseCurrency_)->value();
	Real npv = trades[i]->instrument()->NPV() * fx;
	Real tiny = 1.0e-10;
	QL_REQUIRE(fabs(npv - outputCube->getT0(i, 0)) < tiny, "unexpected NPV difference base scenario vs simulation market");
	outputCube->setT0(npv, i, 0); 
	outputCube->set(npv, i, 0, 0, 0);	
    }
    LOG("Portfolio size = " << portfolio->size());

    boost::timer timer;
    boost::timer loopTimer;

    for (Size sample = 1; sample < outputCube->samples(); ++sample) { // skip sample 0, this is the base scenario
      //DLOG("processing scenario " << sample);
        updateProgress(sample, outputCube->samples());

	timer.restart();
	simMarket_->update(today_);
	updateTime += timer.elapsed();

	timer.restart();
	for (Size i = 0; i < trades.size(); ++i) {
	  //DLOG("processing trade #" << i << " " << trades[i]->id());
	  Real fx = simMarket_->fxSpot(trades[i]->npvCurrency() + baseCurrency_)->value();
	  Real npv = trades[i]->instrument()->NPV() * fx;
	  outputCube->set(npv, i, 0, sample, 0); 
	}
	pricingTime += timer.elapsed();
    }

    updateProgress(outputCube->samples(), outputCube->samples());
    LOG("ScenarioEngine completed: loop " << setprecision(2) << loopTimer.elapsed() << " sec, "
	<< "pricing " << pricingTime << " sec, "
	<< "update " << updateTime << " sec ");
}
}
}
