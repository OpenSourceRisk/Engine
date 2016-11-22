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

#include <orea/scenario/sensitivityscenariogenerator.hpp>
#include <ored/utilities/log.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;

namespace ore {
namespace analytics {

SensitivityScenarioGenerator::SensitivityScenarioGenerator(
    boost::shared_ptr<ScenarioFactory> scenarioFactory,
    boost::shared_ptr<SensitivityScenarioData> sensitivityData,
    boost::shared_ptr<ScenarioSimMarketParameters> simMarketData,
    Date today,
    boost::shared_ptr<ore::data::Market> initMarket,
    const std::string& configuration)
  : scenarioFactory_(scenarioFactory),
    sensitivityData_(sensitivityData), simMarketData_(simMarketData), today_(today),
    initMarket_(initMarket), configuration_(configuration), counter_(0) {

    QL_REQUIRE(initMarket != NULL, "SensitivityScenarioGenerator: initMarket is null");

    // Cache discount curve keys and discount factors
    Size n_ccy = simMarketData_->ccys().size();
    Size n_ten = simMarketData_->yieldCurveTenors().size();
    discountCurveKeys_.reserve(n_ccy * n_ten);
    for (Size j = 0; j < n_ccy; j++) {
        std::string ccy = simMarketData_->ccys()[j];
	Handle<YieldTermStructure> ts = initMarket_->discountCurve(ccy);
        for (Size k = 0; k < n_ten; k++) {
            discountCurveKeys_.emplace_back(RiskFactorKey::KeyType::DiscountCurve, ccy, k); // j * n_ten + k
	    Period tenor = simMarketData_->yieldCurveTenors()[k];
	    Real disc = ts->discount(today_ + tenor);
	    discountCurveCache_[discountCurveKeys_[j * n_ten + k]] = disc;
	    LOG("cache discount " << disc << " for key " << discountCurveKeys_[j * n_ten + k]);
	}
    }

    // Cache index curve keys
    Size n_indices = simMarketData_->indices().size();
    indexCurveKeys_.reserve(n_indices * n_ten);
    for (Size j = 0; j < n_indices; ++j) {
        Handle<IborIndex> index = initMarket_->iborIndex(simMarketData_->indices()[j]);
	Handle<YieldTermStructure> ts = index->forwardingTermStructure();
        for (Size k = 0; k < n_ten; ++k) {
            indexCurveKeys_.emplace_back(RiskFactorKey::KeyType::IndexCurve, simMarketData_->indices()[j], k);
	    Period tenor = simMarketData_->yieldCurveTenors()[k];
	    Real disc = ts->discount(today_ + tenor);
	    indexCurveCache_[indexCurveKeys_[j * n_ten + k]] = ts->discount(today_ + tenor);
	    LOG("cache discount " << disc << " for key " << indexCurveKeys_[j * n_ten + k]);
        }
    }

    // Cache FX rate keys
    fxKeys_.reserve(n_ccy - 1);
    for (Size k = 0; k < n_ccy - 1; k++) {
        const string& foreign = simMarketData_->ccys()[k+1]; // Is this safe?
        const string& domestic = simMarketData_->ccys()[0];
        fxKeys_.emplace_back(RiskFactorKey::KeyType::FXSpot, foreign + domestic); // k
	Real fx = initMarket_->fxSpot(foreign + domestic)->value();
	fxCache_[fxKeys_[k]] = fx;
	LOG("cache FX spot " << fx << " for key " << fxKeys_[k]);
    }

    // Cache Swaption (ATM) vol keys
    Size n_swvol_ccy = simMarketData_->swapVolCcys().size();
    Size n_swvol_term = simMarketData_->swapVolTerms().size();
    Size n_swvol_exp = simMarketData_->swapVolExpiries().size();
    fxKeys_.reserve(n_swvol_ccy * n_swvol_term * n_swvol_exp);
    Size count = 0;
    for (Size i = 0; i < n_swvol_ccy; ++i) {
        std::string ccy = simMarketData_->swapVolCcys()[i];
	Handle<SwaptionVolatilityStructure> ts = initMarket_->swaptionVol(ccy);
	Real strike = 0.0; // FIXME
	for (Size j = 0; j < n_swvol_exp; ++j) {
	    Period expiry = simMarketData_->swapVolExpiries()[j];
	    for (Size k = 0; k < n_swvol_term; ++k) {
	        swaptionVolKeys_.emplace_back(RiskFactorKey::KeyType::SwaptionVolatility, ccy, j * n_swvol_term + k); 
		Period term = simMarketData_->swapVolTerms()[k];
		Real swvol = ts->volatility(expiry, term, strike);
		swaptionVolCache_[swaptionVolKeys_[count]] = swvol;
		LOG("cache swaption vol " << swvol << " for key " << swaptionVolKeys_[count]);
		count++;
	    }
	}
    }

    // Cache FX (ATM) vol keys
    Size n_fxvol_pairs = simMarketData_->ccyPairs().size();
    Size n_fxvol_exp = simMarketData_->fxVolExpiries().size();
    fxVolKeys_.reserve(n_fxvol_pairs * n_fxvol_exp);
    count = 0;
    for (Size j = 0; j < n_fxvol_pairs; ++j) {
        string ccypair = simMarketData_->ccyPairs()[j];
	Real strike = 0.0; // FIXME
        Handle<BlackVolTermStructure> ts = initMarket_->fxVol(ccypair);
        for (Size k = 0; k < n_fxvol_exp; ++k) {
	    fxVolKeys_.emplace_back(RiskFactorKey::KeyType::FXVolatility, ccypair, k);
	    Period expiry = simMarketData_->fxVolExpiries()[k];
	    Real fxvol = ts->blackVol(today_ + expiry, strike);
	    fxVolCache_[fxVolKeys_[count]] = fxvol;
	    LOG("cache FX vol " << fxvol << " for key " << fxVolKeys_[count]);
	    count++;
        }
    }

    generateDiscountCurveScenarios();
    generateIndexCurveScenarios();
    generateFxScenarios();
    if (simMarketData_->simulateSwapVols())
      generateSwaptionVolScenarios();
    if (simMarketData_->simulateFXVols())
      generateFxVolScenarios();

    //generateCapFloorVolScenarios();
    //generateCdsSpreadScenarios();

}

boost::shared_ptr<Scenario> SensitivityScenarioGenerator::next(const Date& d) {
    QL_REQUIRE(counter_ < scenarios_.size(),
	       "scenario vector size " << scenarios_.size() << " exceeded"); 
    return scenarios_[counter_++];    
}
  
bool isShiftAbsolute(const std::string& shiftType) {
    static map<string, bool> m = {
      {"Absolute", true },
      {"ABSOLUTE", true },
      {"Abs", true },
      {"ABS", true },
      {"A", true },
      {"Relative", false },
      {"RELATIVE", false },
      {"Rel", false },
      {"REL", false },
      {"R", false } };
    
    auto it = m.find(shiftType);
    if (it != m.end()) {
        return it->second;
    } else {
        QL_FAIL("Cannot convert shift type " << shiftType << " to bool");
    }
}

void SensitivityScenarioGenerator::addCacheTo(boost::shared_ptr<Scenario> scenario) {
    for (auto key: discountCurveKeys_) {
        if (!scenario->has(key))
	    scenario->add(key, discountCurveCache_[key]);
    }
    for (auto key: indexCurveKeys_) {
        if (!scenario->has(key))
	    scenario->add(key, indexCurveCache_[key]);
    }
    for (auto key: fxKeys_) {
        if (!scenario->has(key))
	    scenario->add(key, fxCache_[key]);
    }
    if (simMarketData_->simulateFXVols()) {
        for (auto key: fxVolKeys_) {
	    if (!scenario->has(key))
	        scenario->add(key, fxVolCache_[key]);
	}
    }
    if (simMarketData_->simulateSwapVols()) {
        for (auto key: swaptionVolKeys_) {
	    if (!scenario->has(key))
	        scenario->add(key, swaptionVolCache_[key]);
	}
    }
}
  
void SensitivityScenarioGenerator::generateFxScenarios() {
    Size n_ccy = simMarketData_->ccys().size();
    string domestic = simMarketData_->baseCcy();
    bool absolute = isShiftAbsolute(sensitivityData_->fxShiftType());
    QL_REQUIRE(!absolute, "FX scenario type must be relative");

    for (Size k = 0; k < n_ccy - 1; k++) {
        string foreign = simMarketData_->ccys()[k+1];
	string ccypair = foreign + domestic;
	string label = sensitivityData_->fxLabel() + "/" + ccypair; 
	boost::shared_ptr<Scenario> scenario = scenarioFactory_->buildScenario(today_, label);
	Real rate = initMarket_->fxSpot(ccypair)->value(); 
	Real newRate = rate * (1.0 + sensitivityData_->fxShiftSize());
	scenario->add(fxKeys_[k], newRate);
	// add remaining unshifted data from cache for a complete scenario
	addCacheTo(scenario);
	scenarios_.push_back(scenario);
	LOG("Sensitivity scenario # " << scenarios_.size() << ", label " << scenario->label() << " created: " << newRate);
    }
    LOG("FX scenarios done");
}
  
void SensitivityScenarioGenerator::generateDiscountCurveScenarios() {
    Size n_ccy = simMarketData_->ccys().size();
    Size n_ten = simMarketData_->yieldCurveTenors().size();
    bool absolute = isShiftAbsolute(sensitivityData_->discountShiftType());
    QL_REQUIRE(absolute, "relative shifts not supported for IR sensitivities");
  
    // original curves' buffer
    std::vector<Real> zeros(n_ten);
    std::vector<Real> times(n_ten);

    for (Size i = 0; i < n_ccy; ++i) {

        string ccy = simMarketData_->ccys()[i];
	Handle<YieldTermStructure> ts = initMarket_->discountCurve(ccy); // FIXME: configuration
	DayCounter dc = ts->dayCounter();
	for (Size j = 0; j < n_ten; ++j) {
	    Date d = today_ + simMarketData_->yieldCurveTenors()[j];
	    zeros[j] = ts->zeroRate(d, dc, Continuous);
	    times[j] = dc.yearFraction(today_, d);
	}
    
	std::vector<Period> tenors = sensitivityData_->discountShiftTenors();
	Real shiftSize = sensitivityData_->discountShiftSize();
	QL_REQUIRE(tenors.size() > 0, "Discount shift tenors not specified");
	
	for (Size j = 0; j < tenors.size(); ++j) {

	    // each shift tenor is associated with a scenario
	    ostringstream o;
	    o << sensitivityData_->discountLabel() << "/" << ccy << "/" << j << "/" << tenors[j];
	    string label = o.str(); 
	    boost::shared_ptr<Scenario> scenario = scenarioFactory_->buildScenario(today_, label);

	    // apply zero rate shift at tenor point j
	    std::vector<Real> shiftedZeros = applyShift(j, tenors, shiftSize, dc, zeros, times);

	    // store shifted discount curve in the scenario
	    for (Size k = 0; k < n_ten; ++k) {
	        Real shiftedDiscount = exp(-shiftedZeros[k] * times[k]);
	        scenario->add(discountCurveKeys_[i * n_ten + k], shiftedDiscount);
	    }
	    
	    // add remaining unshifted data from cache for a complete scenario
	    addCacheTo(scenario);

	    // add this scenario to the scenario vector
	    scenarios_.push_back(scenario);
            LOG("Sensitivity scenario # " << scenarios_.size() << ", label " << scenario->label() << " created");
	    
        } // end of shift curve tenors
    }
    LOG("Discount curve scenarios done");
}

void SensitivityScenarioGenerator::generateIndexCurveScenarios() {
    Size n_indices = simMarketData_->indices().size();
    Size n_ten = simMarketData_->yieldCurveTenors().size();
    bool absolute = isShiftAbsolute(sensitivityData_->indexShiftType());
    QL_REQUIRE(absolute, "relative shifts not supported for IR sensitivities");
  
    // original curves' buffer
    std::vector<Real> zeros(n_ten);
    std::vector<Real> times(n_ten);

    for (Size i = 0; i < n_indices; ++i) {

        string indexName = simMarketData_->indices()[i];
	Handle<IborIndex> iborIndex = initMarket_->iborIndex(indexName);// FIXME: configuration
	Handle<YieldTermStructure> ts = iborIndex->forwardingTermStructure();
	DayCounter dc = ts->dayCounter();
	for (Size j = 0; j < n_ten; ++j) {
	    Date d = today_ + simMarketData_->yieldCurveTenors()[j];
	    zeros[j] = ts->zeroRate(d, dc, Continuous);
	    times[j] = dc.yearFraction(today_, d);
	}
    
	std::vector<Period> tenors = sensitivityData_->indexShiftTenors();
	Real shiftSize = sensitivityData_->indexShiftSize();
	QL_REQUIRE(tenors.size() > 0, "Index shift tenors not specified");
	
	for (Size j = 0; j < tenors.size(); ++j) {

	    // each shift tenor is associated with a scenario
	    ostringstream o;
	    o << sensitivityData_->indexLabel() << "/" << indexName << "/" << j << "/" << tenors[j];
	    string label = o.str(); 
	    boost::shared_ptr<Scenario> scenario = scenarioFactory_->buildScenario(today_, label);

	    // apply zero rate shift at tenor point j
	    std::vector<Real> shiftedZeros = applyShift(j, tenors, shiftSize, dc, zeros, times);

	    // store shifted discount curve for this index in the scenario
	    for (Size k = 0; k < n_ten; ++k) {
	        Real shiftedDiscount = exp(-shiftedZeros[k] * times[k]);
		scenario->add(indexCurveKeys_[i * n_ten + k], shiftedDiscount);
	    }
	    
	    // add remaining unshifted data from cache for a complete scenario
	    addCacheTo(scenario);

	    // add this scenario to the scenario vector
	    scenarios_.push_back(scenario);
            LOG("Sensitivity scenario # " << scenarios_.size() << ", label " << scenario->label() << " created");
	    
        } // end of shift curve tenors
    }
    LOG("Index curve scenarios done");
}

void SensitivityScenarioGenerator::generateFxVolScenarios() {
    string domestic = simMarketData_->baseCcy();
    bool absolute = isShiftAbsolute(sensitivityData_->fxVolShiftType());
    QL_REQUIRE(!absolute, "FX vol scenario type must be relative");

    Size n_fxvol_pairs = simMarketData_->ccyPairs().size();
    Size n_fxvol_exp = simMarketData_->fxVolExpiries().size();

    std::vector<Real> values(n_fxvol_exp);
    std::vector<Real> times(n_fxvol_exp);

    std::vector<Period> tenors = sensitivityData_->fxVolShiftExpiries();
    Real shiftSize = sensitivityData_->fxVolShiftSize();
    QL_REQUIRE(tenors.size() > 0, "FX vol shift tenors not specified");

    Size count = 0;
    for (Size i = 0; i < n_fxvol_pairs; ++i) {
        string ccypair = simMarketData_->ccyPairs()[i];
	Handle<BlackVolTermStructure> ts = initMarket_->fxVol(ccypair); // FIXME: configuration
	DayCounter dc = ts->dayCounter();
	Real strike = 0.0; // FIXME
	for (Size j = 0; j < n_fxvol_exp; ++j) {
	    Date d = today_ + simMarketData_->fxVolExpiries()[j];
	    values[j] = ts->blackVol(d, strike);
	    times[j] = dc.yearFraction(today_, d);
	}

	for (Size j = 0; j < tenors.size(); ++j) {
	    // each shift tenor is associated with a scenario
	    ostringstream o;
	    o << sensitivityData_->fxVolLabel() << "/" << ccypair << "/" << j << "/" << tenors[j];
	    string label = o.str(); 
	    boost::shared_ptr<Scenario> scenario = scenarioFactory_->buildScenario(today_, label);
	    // apply shift at tenor point j
	    std::vector<Real> shiftedVols = applyShift(j, tenors, shiftSize, dc, values, times);
	    for (Size k = 0; k < n_fxvol_exp; ++k) {
	        scenario->add(fxVolKeys_[count], shiftedVols[k]);
		count++;
	    }
	    // add remaining unshifted data from cache for a complete scenario
	    addCacheTo(scenario);
	    // add this scenario to the scenario vector
	    scenarios_.push_back(scenario);
	    LOG("Sensitivity scenario # " << scenarios_.size() << ", label " << scenario->label() << " created");
        }
    }
    LOG("FX vol scenarios done");
}

void SensitivityScenarioGenerator::generateSwaptionVolScenarios() {
    bool absolute = isShiftAbsolute(sensitivityData_->swaptionVolShiftType());
    QL_REQUIRE(!absolute, "FX vol scenario type must be relative");

    Size n_swvol_ccy = simMarketData_->swapVolCcys().size();
    Size n_swvol_term = simMarketData_->swapVolTerms().size();
    Size n_swvol_exp = simMarketData_->swapVolExpiries().size();
    Size count = 0;
    // FIXME: This does a parallel shift to all ATM vols per currency for the moment
    for (Size i = 0; i < n_swvol_ccy; ++i) {
        std::string ccy = simMarketData_->swapVolCcys()[i];
	string label = sensitivityData_->swaptionVolLabel() + "/" + ccy; 
	boost::shared_ptr<Scenario> scenario = scenarioFactory_->buildScenario(today_, label);
	for (Size j = 0; j < n_swvol_exp; ++j) {
	    for (Size k = 0; k < n_swvol_term; ++k) {
	        Real vol = swaptionVolCache_[swaptionVolKeys_[count]];
		Real newVol = vol * (1.0 + sensitivityData_->swaptionVolShiftSize());
		scenario->add(swaptionVolKeys_[count], newVol);
		count++;
	    }
	}
	// add remaining unshifted data from cache for a complete scenario
	addCacheTo(scenario);
	// add this scenario to the scenario vector
	scenarios_.push_back(scenario);
	LOG("Sensitivity scenario # " << scenarios_.size() << ", label " << scenario->label() << " created");
    }
    LOG("Swaption vol scenarios done");
}

vector<Real> SensitivityScenarioGenerator::applyShift(Size j,
						      const vector<Period>& tenors,
						      Real shiftSize,
						      DayCounter dc,
						      const vector<Real>& zeros,
						      const vector<Real>& times) {
    /****************************************************************************
   * Apply triangular shaped zero rate shifts to the underlying curve
   * where the triangle reaches from the previous to the next shift tenor point
   * with peak at the current shift tenor point. At the initial and final shift
   * tenor the shape is replaced such that the full shift is applied to all curve 
   * grid points to the left of the first shift point and to the right of the last
   * shift point, respectively. The procedure guarantees that no sensitivity to 
   * original curve points is "missed" when the shift curve is less granular, e.g.
   * original curve |...|...|...|...|...|...|...|...|...|
   * shift curve    ......|...........|...........|......
   */
  // FIXME: Check case where the shift curve is more granular than the original
	
  //std::vector<Period> tenors = sensitivityData_->irShiftTenors();
	
    std::vector<Real> shiftedZeros = zeros; // FIXME: hard coded zero domain
    Date currentShiftDate = today_ + tenors[j];
    Real t1 = dc.yearFraction(today_, currentShiftDate);
    
    if (tenors.size() == 1) { // single shift tenor means parallel shift
        for (Size k = 0; k < times.size(); k++)
	    shiftedZeros[k] += shiftSize; 
    }
    else if (j == 0) { // first shift tenor, flat extrapolation to the left
        Date nextShiftDate = today_ + tenors[j+1];
	Real t2 = dc.yearFraction(today_, nextShiftDate);
	for (Size k = 0; k < times.size(); k++) {
	  if (times[k] <= t1) // full shift
	    shiftedZeros[k] += shiftSize;
	  else if (times[k] <= t2) // linear interpolation in t1 < times[k] < t2
	    shiftedZeros[k] += shiftSize * (t2 - times[k]) / (t2 - t1);
	  else break;
	}
    }
    else if (j == tenors.size() - 1) { // last shift tenor, flat extrapolation to the right
      Date previousShiftDate = today_ + tenors[j-1];
      Real t0 = dc.yearFraction(today_, previousShiftDate);
      for(Size k = 0; k < times.size(); k++) {
	if (times[k] >= t0 && times[k] <= t1) // linear interpolation in t0 < times[k] < t1
	  shiftedZeros[k] += shiftSize * (times[k] - t0) / (t1 - t0); 
	else if (times[k] > t1) // full shift
	  shiftedZeros[k] += shiftSize;
      }
    }
    else { // intermediate shift tenor
      Date previousShiftDate = today_ + tenors[j-1];
      Real t0 = dc.yearFraction(today_, previousShiftDate);
      Date nextShiftDate = today_ + tenors[j+1];
      Real t2 = dc.yearFraction(today_, nextShiftDate);
      for (Size k = 0; k < times.size(); k++) {
	if (times[k] >= t0 && times[k] <= t1) // linear interpolation in t0 < times[k] < t1
	  shiftedZeros[k] += shiftSize * (times[k] - t0) / (t1 - t0); 
	else if (times[k] > t1 && times[k] <= t2) // linear interpolation in t1 < times[k] < t2
	  shiftedZeros[k] += shiftSize * (t2 - times[k]) / (t2 - t1); 
      }
    }
    
    // convert zeros into a shifted discount curve
    // std::vector<Real> shiftedDiscounts(shiftedZeros.size());
    // for (Size k = 0; k < shiftedZeros.size(); k++) 
    //   shiftedDiscounts[k] = exp(-shiftedZeros[k] * times[k]);

    return shiftedZeros;
}
  
}
}
