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

/*! \file scenario/sensitivityscenario.hpp
    \brief A class to hold Scenario parameters for scenarioSimMarket
    \ingroup scenario
*/

#pragma once

#include <qlw/analytics/scenario.hpp>
#include <qlw/analytics/scenarioconfig.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/daycounters/actualactual.hpp>

using namespace QuantLib;

namespace ore {
namespace analytics {

//-----------------------------------------------------------------------------------------------
//
// Generate sensitivity scenarios
//
class SensitivityScenario  {
public:
    enum Type {
        DISCOUNT = 0,
	INDEX = 1,
	FX = 2,
	SWAPTIONVOL = 3,
	CAPVOL = 4,
	FXVOL = 5
    };
    SensitivityScenario(Real absoluteYieldShift, 
			bool shiftIndexCurves,
			Real relativeFxRateShift, 
			Real absoluteVolShift);
  
    Size size() { return scenarioData_.size(); }
    PTR<Scenario> getScenario(Size i);
  Type getScenarioType(Size i);
  std::string getScenarioKey(Size i);
  std::string getScenarioCurrency(Size i);
  std::string getScenarioLabel(Size i);
private:	
  void generateYieldCurveScenarios(bool discountCurves,
				   Handle<YieldTermStructure> ts, 
					 std::string key, std::string ccy, std::string baseLabel);
	Real absoluteYieldShift_;
	bool shiftIndexCurves_;
	Real relativeFxRateShift_;
	Real absoluteVolShift_;
	std::vector<PTR<Scenario> > scenarioData_;
	std::vector<std::string> scenarioLabel_;
	std::vector<std::string> scenarioCcy_;
	std::vector<std::string> scenarioKey_;
	std::vector<Type> scenarioType_;
    };
}
}

