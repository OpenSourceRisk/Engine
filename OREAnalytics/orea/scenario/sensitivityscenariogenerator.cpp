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
        const string& foreign = simMarketData_->ccys()[k+1];
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
    if (simMarketData_->simulateFXVols())
      generateFxVolScenarios();
    if (simMarketData_->simulateSwapVols())
      generateSwaptionVolScenarios();

    //generateCapFloorVolScenarios();
    //generateCdsSpreadScenarios();

}

boost::shared_ptr<Scenario> SensitivityScenarioGenerator::next(const Date& d) {
    QL_REQUIRE(counter_ < scenarios_.size(),
	       "scenario vector size " << scenarios_.size() << " exceeded"); 
    return scenarios_[counter_++];    
}
  
SensitivityScenarioGenerator::ShiftType parseShiftType(const std::string& s) {
  static map<string, SensitivityScenarioGenerator::ShiftType> m = {
      {"Absolute", SensitivityScenarioGenerator::ShiftType::Absolute },
      {"ABSOLUTE", SensitivityScenarioGenerator::ShiftType::Absolute },
      {"Abs", SensitivityScenarioGenerator::ShiftType::Absolute },
      {"ABS", SensitivityScenarioGenerator::ShiftType::Absolute },
      {"A", SensitivityScenarioGenerator::ShiftType::Absolute },
      {"Relative", SensitivityScenarioGenerator::ShiftType::Relative },
      {"RELATIVE", SensitivityScenarioGenerator::ShiftType::Relative },
      {"Rel", SensitivityScenarioGenerator::ShiftType::Relative },
      {"REL", SensitivityScenarioGenerator::ShiftType::Relative },
      {"R", SensitivityScenarioGenerator::ShiftType::Relative } };
    
    auto it = m.find(s);
    if (it != m.end()) {
        return it->second;
    } else {
        QL_FAIL("Cannot convert shift type " << s << " to SensitivityScenarioGenerator::ShiftType");
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
    ShiftType type = parseShiftType(sensitivityData_->fxShiftType());
    QL_REQUIRE(type == SensitivityScenarioGenerator::ShiftType::Relative, "FX scenario type must be relative");

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
    ShiftType shiftType = parseShiftType(sensitivityData_->discountShiftType());
  
    // original curves' buffer
    std::vector<Real> zeros(n_ten);
    std::vector<Real> times(n_ten);

    // buffer for shifted zero curves
    std::vector<Real> shiftedZeros(n_ten);
    
    for (Size i = 0; i < n_ccy; ++i) {

        string ccy = simMarketData_->ccys()[i];
	Handle<YieldTermStructure> ts = initMarket_->discountCurve(ccy); // FIXME: configuration
	DayCounter dc = ts->dayCounter();
	for (Size j = 0; j < n_ten; ++j) {
	    Date d = today_ + simMarketData_->yieldCurveTenors()[j];
	    zeros[j] = ts->zeroRate(d, dc, Continuous);
	    times[j] = dc.yearFraction(today_, d);
	}
    
	std::vector<Period> shiftTenors = sensitivityData_->discountShiftTenors();
	std::vector<Time> shiftTimes(shiftTenors.size());
	for (Size j = 0; j < shiftTenors.size(); ++j)
	    shiftTimes[j] = dc.yearFraction(today_, today_ + shiftTenors[j]);	
	Real shiftSize = sensitivityData_->discountShiftSize();
	QL_REQUIRE(shiftTenors.size() > 0, "Discount shift tenors not specified");
	
	for (Size j = 0; j < shiftTenors.size(); ++j) {

	    // each shift tenor is associated with a scenario
	    ostringstream o;
	    //o << sensitivityData_->discountLabel() << "/" << ccy << "/" << j << "/" << shiftTenors[j];
	    o << sensitivityData_->discountLabel() << "/" << ccy << "/" << shiftTenors[j];
	    string label = o.str(); 
	    boost::shared_ptr<Scenario> scenario = scenarioFactory_->buildScenario(today_, label);

	    // apply zero rate shift at tenor point j
	    applyShift(j, shiftSize, shiftType, shiftTimes, zeros, times, shiftedZeros);

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
    ShiftType shiftType = parseShiftType(sensitivityData_->indexShiftType());
  
    // original curves' buffer
    std::vector<Real> zeros(n_ten);
    std::vector<Real> times(n_ten);

    // buffer for shifted zero curves
    std::vector<Real> shiftedZeros(n_ten);

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
    
	std::vector<Period> shiftTenors = sensitivityData_->indexShiftTenors();
	std::vector<Time> shiftTimes(shiftTenors.size());
	for (Size j = 0; j < shiftTenors.size(); ++j)
	    shiftTimes[j] = dc.yearFraction(today_, today_ + shiftTenors[j]);
	Real shiftSize = sensitivityData_->indexShiftSize();
	QL_REQUIRE(shiftTenors.size() > 0, "Index shift tenors not specified");
	
	for (Size j = 0; j < shiftTenors.size(); ++j) {

	    // each shift tenor is associated with a scenario
	    ostringstream o;
	    //o << sensitivityData_->indexLabel() << "/" << indexName << "/" << j << "/" << shiftTenors[j];
	    o << sensitivityData_->indexLabel() << "/" << indexName << "/" << shiftTenors[j];
	    string label = o.str(); 
	    boost::shared_ptr<Scenario> scenario = scenarioFactory_->buildScenario(today_, label);

	    // apply zero rate shift at tenor point j
	    applyShift(j, shiftSize, shiftType, shiftTimes, zeros, times, shiftedZeros);

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
    ShiftType shiftType = parseShiftType(sensitivityData_->fxVolShiftType());

    Size n_fxvol_pairs = simMarketData_->ccyPairs().size();
    Size n_fxvol_exp = simMarketData_->fxVolExpiries().size();

    std::vector<Real> values(n_fxvol_exp);
    std::vector<Real> times(n_fxvol_exp);

    // buffer for shifted zero curves
    std::vector<Real> shiftedValues(n_fxvol_exp);

    std::vector<Period> shiftTenors = sensitivityData_->fxVolShiftExpiries();
    for (Size i = 0; i < shiftTenors.size(); ++i)
      LOG("FX Vol Shift Tenor " << i << ": " << shiftTenors[i]);
    LOG("FX Vol Keys size = " << fxVolKeys_.size());
    
    std::vector<Time> shiftTimes(shiftTenors.size());
    Real shiftSize = sensitivityData_->fxVolShiftSize();
    QL_REQUIRE(shiftTenors.size() > 0, "FX vol shift tenors not specified");

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

	for (Size j = 0; j < shiftTenors.size(); ++j)
	    shiftTimes[j] = dc.yearFraction(today_, today_ + shiftTenors[j]);

	for (Size j = 0; j < shiftTenors.size(); ++j) {
	    // each shift tenor is associated with a scenario
	    ostringstream o;
	    //o << sensitivityData_->fxVolLabel() << "/" << ccypair << "/" << j << "/" << shiftTenors[j];
	    o << sensitivityData_->fxVolLabel() << "/" << ccypair << "/" << shiftTenors[j];
	    string label = o.str(); 
	    //LOG("FX Vol shift, " << ccypair << ", tenor " << j << " " <<  shiftTenors[j] << " label " << label);
	    boost::shared_ptr<Scenario> scenario = scenarioFactory_->buildScenario(today_, label);
	    // apply shift at tenor point j
	    applyShift(j, shiftSize, shiftType, shiftTimes, values, times, shiftedValues);
	    for (Size k = 0; k < n_fxvol_exp; ++k)
	        scenario->add(fxVolKeys_[i * n_fxvol_exp + k], shiftedValues[k]);
	    
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
    ShiftType shiftType = parseShiftType(sensitivityData_->swaptionVolShiftType());
    Real shiftSize = sensitivityData_->swaptionVolShiftSize();
    Size n_swvol_ccy = simMarketData_->swapVolCcys().size();
    Size n_swvol_term = simMarketData_->swapVolTerms().size();
    Size n_swvol_exp = simMarketData_->swapVolExpiries().size();
    Size count = 0;

    vector<vector<Real>> volData(n_swvol_exp, vector<Real>(n_swvol_term, 0.0));
    vector<Real> volExpiryTimes(n_swvol_exp, 0.0);
    vector<Real> volTermTimes(n_swvol_term, 0.0);
    vector<vector<Real>> shiftedVolData(n_swvol_exp, vector<Real>(n_swvol_term, 0.0));

    vector<Real> shiftExpiryTimes(sensitivityData_->swaptionVolShiftExpiries().size(), 0.0);
    vector<Real> shiftTermTimes(sensitivityData_->swaptionVolShiftTerms().size(), 0.0);

    for (Size i = 0; i < n_swvol_ccy; ++i) {
        std::string ccy = simMarketData_->swapVolCcys()[i];
	Handle<SwaptionVolatilityStructure> ts = initMarket_->swaptionVol(ccy);
	DayCounter dc = ts->dayCounter();
	Real strike = 0.0; // FIXME

	// cache original vol data
	for (Size j = 0; j < n_swvol_exp; ++j) {
	    Date expiry = today_ + simMarketData_->swapVolExpiries()[j];
	    volExpiryTimes[j] = dc.yearFraction(today_, expiry);
	}
	for (Size j = 0; j < n_swvol_term; ++j) {
	    Date term = today_ + simMarketData_->swapVolTerms()[j];
	    volTermTimes[j] = dc.yearFraction(today_, term);
	}
	for (Size j = 0; j < n_swvol_exp; ++j) {
	    Period expiry = simMarketData_->swapVolExpiries()[j];
	    for (Size k = 0; k < n_swvol_term; ++k) {
	        Period term = simMarketData_->swapVolTerms()[k];
		Real swvol = ts->volatility(expiry, term, strike);
		volData[j][k] = swvol;
	    }
	}

	// cache tenor times
	for (Size j = 0; j < shiftExpiryTimes.size(); ++j)
	    shiftExpiryTimes[j] = dc.yearFraction(today_, today_ + sensitivityData_->swaptionVolShiftExpiries()[j]);
	for (Size j = 0; j < shiftTermTimes.size(); ++j)
	    shiftTermTimes[j] = dc.yearFraction(today_, today_ + sensitivityData_->swaptionVolShiftTerms()[j]);

	// loop over shift expiries and terms
	for (Size j = 0; j < shiftExpiryTimes.size(); ++j) {
	    for (Size k = 0; k < shiftTermTimes.size(); ++k) {
	        // each shift expiry and term is associated with a scenario
	        ostringstream o;
		o << sensitivityData_->swaptionVolLabel() << "/" << ccy //<< "/" << j
		  << "/" << sensitivityData_->swaptionVolShiftExpiries()[j] //<< "/" << k
		  << "/" << sensitivityData_->swaptionVolShiftTerms()[k];
		string label = o.str(); 
		boost::shared_ptr<Scenario> scenario = scenarioFactory_->buildScenario(today_, label);
		applyShift(j, k, shiftSize, shiftType, shiftExpiryTimes, shiftTermTimes,
			   volExpiryTimes, volTermTimes, volData, shiftedVolData);
		// add shifted vol data to the scenario
		for (Size jj = 0; jj < n_swvol_exp; ++jj) {
		    for (Size kk = 0; kk < n_swvol_term; ++kk) {
		        Size idx = i * n_swvol_exp * n_swvol_term + jj * n_swvol_term + kk;
			scenario->add(swaptionVolKeys_[idx], shiftedVolData[jj][kk]);
		    }
		}
		// add remaining unshifted data from cache for a complete scenario
		addCacheTo(scenario);
		// add this scenario to the scenario vector
		scenarios_.push_back(scenario);
		LOG("Sensitivity scenario # " << scenarios_.size() << ", label " << scenario->label() << " created");
	    }
	}
    }
    LOG("Swaption vol scenarios done");
}

void SensitivityScenarioGenerator::applyShift(Size j,
					      Real shiftSize,
					      ShiftType shiftType,
					      const vector<Time>& tenors,
					      const vector<Real>& values,
					      const vector<Real>& times,
					      vector<Real>& shiftedValues) {
    /****************************************************************************
     * Apply triangular shaped shifts to the underlying curve
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
	
    QL_REQUIRE(j < tenors.size(), "index j out of range");
    QL_REQUIRE(times.size() == values.size(), "vector size mismatch");
    QL_REQUIRE(shiftedValues.size() == values.size(), "shifted values vector size does not match input");

    for (Size i = 0; i < values.size(); ++i)
        shiftedValues[i] = values[i];
    Time t1 = tenors[j];
    
    if (tenors.size() == 1) { // single shift tenor means parallel shift
        for (Size k = 0; k < times.size(); k++)
	    if (shiftType == ShiftType::Absolute)
	        shiftedValues[k] += shiftSize;
	    else
	        shiftedValues[k] *= (1.0 + shiftSize);
    }
    else if (j == 0) { // first shift tenor, flat extrapolation to the left
        Time t2 = tenors[j+1]; 
	for (Size k = 0; k < times.size(); k++) {
	    Real w = 0.0;
	    if (times[k] <= t1) // full shift
	        w = 1.0;
	    else if (times[k] <= t2) // linear interpolation in t1 < times[k] < t2
	        w = (t2 - times[k]) / (t2 - t1);
	    if (shiftType == ShiftType::Absolute)
	        shiftedValues[k] += w * shiftSize;
	    else
	        shiftedValues[k] *= (1.0 + w * shiftSize);
	}
    }
    else if (j == tenors.size() - 1) { // last shift tenor, flat extrapolation to the right
        Time t0 = tenors[j-1];
	for (Size k = 0; k < times.size(); k++) {
	    Real w = 0.0;
	    if (times[k] >= t0 && times[k] <= t1) // linear interpolation in t0 < times[k] < t1
	        w = (times[k] - t0) / (t1 - t0); 
	    else if (times[k] > t1) // full shift
	        w = 1.0;
	    if (shiftType == ShiftType::Absolute)
	        shiftedValues[k] += w * shiftSize;
	    else
	        shiftedValues[k] *= (1.0 + w * shiftSize);
	}
    }
    else { // intermediate shift tenor
        Time t0 = tenors[j-1];
	Time t2 = tenors[j+1];
	for (Size k = 0; k < times.size(); k++) {
	  Real w = 0.0;
	  if (times[k] >= t0 && times[k] <= t1) // linear interpolation in t0 < times[k] < t1
	      w = (times[k] - t0) / (t1 - t0); 
	  else if (times[k] > t1 && times[k] <= t2) // linear interpolation in t1 < times[k] < t2
	      w = (t2 - times[k]) / (t2 - t1); 
	    if (shiftType == ShiftType::Absolute)
	        shiftedValues[k] += w * shiftSize;
	    else
	        shiftedValues[k] *= (1.0 + w * shiftSize);
	}
    }
}

void SensitivityScenarioGenerator::applyShift(Size i,
					      Size j,
					      Real shiftSize,
					      ShiftType shiftType,
					      const vector<Time>& shiftX,
					      const vector<Time>& shiftY,
					      const vector<Time>& dataX,
					      const vector<Time>& dataY,
					      const vector<vector<Real>>& data,
					      vector<vector<Real>>& shiftedData) {
    QL_REQUIRE(shiftX.size() >= 1 && shiftY.size() >= 1,
	       "shift vector size >= 1 reqired");
    QL_REQUIRE(i < shiftX.size(), "index i out of range");
    QL_REQUIRE(j < shiftY.size(), "index j out of range");
    
    // initalise the shifted data
    for (Size k = 0; k < dataX.size(); ++k) {
        for (Size l = 0; l < dataY.size(); ++l)
	    shiftedData[k][l] = data[k][l];
    }

    // single or no shift point means parallel shift
    if (shiftX.size() == 1 && shiftY.size() == 1) { 
        for (Size k = 0; k < dataX.size(); ++k) {
	    for (Size l = 0; l < dataY.size(); ++l)
	        if (shiftType == ShiftType::Absolute)
		    shiftedData[k][l] += shiftSize;
		else
		    shiftedData[k][l] *= (1.0 + shiftSize);
	}
	return;
    }

    Size iMax = shiftX.size() - 1;
    Size jMax = shiftY.size() - 1;
    Real tx = shiftX[i];
    Real ty = shiftY[j];
    Real tx1 = i > 0 ? shiftX[i-1] : QL_MAX_REAL;
    Real ty1 = j > 0 ? shiftY[j-1] : QL_MAX_REAL;
    Real tx2 = i < iMax ? shiftX[i+1] : -QL_MAX_REAL;
    Real ty2 = j < jMax ? shiftY[j+1] : -QL_MAX_REAL;

    for (Size ix = 0; ix < dataX.size(); ++ix) {
        Real x = dataX[ix];
     	for (Size iy = 0; iy < dataY.size(); ++iy) {
    	    Real y = dataY[iy];
    	    Real wx = 0.0;
    	    Real wy = 0.0;
    	    if (x >= tx && x <= tx2 && y >= ty && y <= ty2) {
    	        wx = (tx2 - x) / (tx2 - tx);
    		wy = (ty2 - y) / (ty2 - ty);
    	    }
    	    else if (x >= tx && x <= tx2 && y >= ty1 && y <= ty) {
    	        wx = (tx2 - x) / (tx2 - tx);
    		wy = (y - ty1) / (ty - ty1);
    	    }
    	    else if (x >= tx1 && x <= tx && y >= ty1 && y <= ty) {
    	        wx = (x - tx1) / (tx - tx1);
    		wy = (y - ty1) / (ty - ty1);
    	    }
    	    else if (x >= tx1 && x <= tx && y >= ty && y <= ty2) {
    	        wx = (x - tx1) / (tx - tx1);
    	    	wy = (ty2 - y) / (ty2 - ty);
    	    }
	    else if ((x <= tx && i == 0 && y < ty && j == 0) ||
		     (x <= tx && i == 0 && y >= ty && j == jMax) ||
		     (x >= tx && i == iMax && y >= ty && j == jMax) ||
		     (x >= tx && i == iMax && y < ty && j == 0)) {
	        wx = 1.0;
		wy = 1.0;
	    }
	    else if (((x <= tx && i == 0) || (x >= tx && i == iMax)) && y >= ty1 && y <= ty) {
	        wx = 1.0;
		wy = (y - ty1) / (ty - ty1);
	    }
	    else if (((x <= tx && i == 0) || (x >= tx && i == iMax)) && y >= ty && y <= ty2) {
	        wx = 1.0;
		wy = (ty2 - y) / (ty2 - ty);
	    }
	    else if (x >= tx1 && x <= tx && ((y < ty && j == 0) || (y >= ty && j == jMax))) {
	        wx = (x - tx1) / (tx - tx1);
		wy = 1.0;
	    }
	    else if (x >= tx && x <= tx2 && ((y < ty && j == 0) || (y >= ty && j == jMax))) {
	        wx = (tx2 - x) / (tx2 - tx);
		wy = 1.0;
	    }
	    QL_REQUIRE(wx >= 0.0 && wx <= 1.0, "wx out of range");
	    QL_REQUIRE(wy >= 0.0 && wy <= 1.0, "wy out of range");
	    
	    if (shiftType == ShiftType::Absolute)
	        shiftedData[ix][iy] += wx * wy * shiftSize; 
	    else
	        shiftedData[ix][iy] *= (1.0 + wx * wy * shiftSize);
    	}
    }
}

}
}
