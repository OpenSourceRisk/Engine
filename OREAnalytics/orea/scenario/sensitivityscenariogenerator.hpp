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

/*! \file scenario/sensitivityscenariogenerator.hpp
    \brief Sensitivity scenario generation 
    \ingroup scenario
*/

#pragma once

#include <ored/marketdata/market.hpp>
#include <orea/scenario/scenariogenerator.hpp>
#include <orea/scenario/scenariofactory.hpp>
#include <orea/scenario/scenariosimmarket.hpp>
#include <orea/scenario/sensitivityscenariodata.hpp>

namespace ore {
using namespace data;
namespace analytics {

//! Sensitivity Scenario Generator
/*!

  \ingroup scenario
 */
class SensitivityScenarioGenerator : public ScenarioGenerator {
public:
    enum class ShiftType {
      Absolute,
      Relative
    };

    //! Constructor
    SensitivityScenarioGenerator(boost::shared_ptr<ScenarioFactory> scenarioFactory,
				 boost::shared_ptr<SensitivityScenarioData> sensitivityData,
				 boost::shared_ptr<ScenarioSimMarketParameters> simMarketData,
				 QuantLib::Date today, 
				 boost::shared_ptr<ore::data::Market> initMarket,
				 const std::string& configuration = Market::defaultConfiguration);
    //! Default destructor
    ~SensitivityScenarioGenerator(){};
    boost::shared_ptr<Scenario> next(const Date& d);
    void reset() { counter_ = 0; }
    Size samples() { return scenarios_.size(); }
    const std::vector<boost::shared_ptr<Scenario>>& scenarios() {
        return scenarios_;
    }

    //! Apply 1d triangular shift to 1d data, public to allow test suite access
    void applyShift(Size j,
		    Real shiftSize,
		    ShiftType type,
		    const vector<Time>& shiftTimes,
		    const vector<Real>& values,
		    const vector<Time>& times,
		    vector<Real>& shiftedValues);
    // vector<Real> applyShift(Size j, Real shiftSize, DayCounter dc,
    // 			    const vector<Period>& tenors,
    // 			    const vector<Real>& values,
    // 			    const vector<Real>& times);
    //! Apply 2d shift to 2d matrix such as swaption volatiities, public to allow test suite access
    void applyShift(Size j,
		    Size k,
		    Real shiftSize,
		    ShiftType type,
		    const vector<Time>& shiftX,
		    const vector<Time>& shiftY,
		    const vector<Time>& dataX,
		    const vector<Time>& dataY,
		    const vector<vector<Real>>& data,
		    vector<vector<Real>>& shiftedData);
private:
    void addCacheTo(boost::shared_ptr<Scenario> scenario);
    void generateYieldCurveScenarios();
    void generateDiscountCurveScenarios();
    void generateIndexCurveScenarios();
    void generateFxScenarios();
    void generateSwaptionVolScenarios();
    void generateFxVolScenarios();
    void generateCapFloorVolScenarios(); // todo
    void generateCdsSpreadScenarios(); // todo
    //! Apply a zero rate shift at tenor point j and return the sim market discount curve scenario
    boost::shared_ptr<ScenarioFactory> scenarioFactory_;
    boost::shared_ptr<SensitivityScenarioData> sensitivityData_;
    boost::shared_ptr<ScenarioSimMarketParameters> simMarketData_;
    Date today_;
    boost::shared_ptr<ore::data::Market> initMarket_;
    const std::string configuration_;
    std::vector<RiskFactorKey> discountCurveKeys_, indexCurveKeys_, fxKeys_, swaptionVolKeys_, fxVolKeys_;
    std::map<RiskFactorKey,Real> discountCurveCache_, indexCurveCache_, fxCache_, swaptionVolCache_, fxVolCache_;
    std::vector<boost::shared_ptr<Scenario>> scenarios_;
    Size counter_;
};
}
}
