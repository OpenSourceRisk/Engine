/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

#include <orea/scenario/scenariofactory.hpp>
#include <orea/scenario/scenariogenerator.hpp>
#include <orea/scenario/scenariosimmarket.hpp>
#include <orea/scenario/sensitivityscenariodata.hpp>
#include <ored/marketdata/market.hpp>

namespace ore {
using namespace data;
namespace analytics {

//! Shift Scenario Generator
/*!
   Base class for sensitivity and stress scenario generators
  \ingroup scenario
 */
class ShiftScenarioGenerator : public ScenarioGenerator {
public:
    enum class ShiftType { Absolute, Relative };

    class ScenarioDescription {
    public:
        enum class Type { Base, Up, Down, Cross };
        //! Constructor
        ScenarioDescription(Type type)
            : type_(type), key1_(RiskFactorKey()), indexDesc1_(""), key2_(RiskFactorKey()), indexDesc2_("") {}
        ScenarioDescription(Type type, RiskFactorKey key1, string indexDesc1)
            : type_(type), key1_(key1), indexDesc1_(indexDesc1), key2_(RiskFactorKey()), indexDesc2_("") {}
        ScenarioDescription(ScenarioDescription d1, ScenarioDescription d2)
            : type_(Type::Cross), key1_(d1.key1()), indexDesc1_(d1.indexDesc1()), key2_(d2.key1()),
              indexDesc2_(d2.indexDesc1()) {}
        //! Inspectors
        //@{
        Type type() const { return type_; }
        RiskFactorKey key1() const { return key1_; }
        RiskFactorKey key2() const { return key2_; }
        string indexDesc1() const { return indexDesc1_; }
        string indexDesc2() const { return indexDesc2_; }
        //@}
        //! Return type as string
        string typeString() const;
        //! Return key1 as string with text1 appended as key index description
        string factor1() const;
        //! Return key2 as string with text2 appended as key index description
        string factor2() const;
        //! Return full description
        string text() const;

    private:
        Type type_;
        RiskFactorKey key1_;
        string indexDesc1_;
        RiskFactorKey key2_;
        string indexDesc2_;
    };

    //! Constructor
    ShiftScenarioGenerator(const boost::shared_ptr<Scenario>& baseScenario,
                           const boost::shared_ptr<ScenarioSimMarketParameters> simMarketData_);
    //! Default destructor
    ~ShiftScenarioGenerator(){};

    //! Scenario Generator interface
    //@{
    boost::shared_ptr<Scenario> next(const Date& d);
    void reset() { counter_ = 0; }
    //@}

    //! Inspectors
    //@{
    //! Number of shift scenarios
    Size samples() { return scenarios_.size(); }
    //! Return the base scenario, i.e. cached initial values of all relevant market points
    const boost::shared_ptr<Scenario>& baseScenario() { return scenarios_.front(); }
    //! Return vector of sensitivity scenarios, scenario 0 is the base scenario
    const std::vector<boost::shared_ptr<Scenario>>& scenarios() { return scenarios_; }
    //! Return vector of scenario descriptions
    std::vector<ScenarioDescription> scenarioDescriptions() { return scenarioDescriptions_; }
    // ! Return map of RiskFactorKeys to factors, i.e. human readable text representations
    const std::map<RiskFactorKey, std::string>& keyToFactor() { return keyToFactor_; }
    //! Return revers map of factors to RiskFactorKeys
    const std::map<std::string, RiskFactorKey>& factorToKey() { return factorToKey_; }
    //@}

    //! Apply 1d triangular shift to 1d data such as yield curves, public to allow test suite access
    /*!
      Apply triangular shaped shifts to the underlying curve where the triangle
      reaches from the previous to the next shift tenor point with peak at the
      current shift tenor point.
      At the initial and final shift tenor the shape is replaced such that the
      full shift is applied to all curve grid points to the left of the first
      shift point and to the right of the last shift point, respectively.
      The procedure guarantees that no sensitivity to original curve points is
      "missed" when the shift curve is less granular, e.g.
      original curve |...|...|...|...|...|...|...|...|...|
      shift curve    ......|...........|...........|......
     */
    void applyShift(
        //! Number of the shift curve tenor point to be shifted here
        Size j,
        //! Shift size interpreted as either absolute or relative shift
        Real shiftSize,
        //! Upwards shift if true, otherwise downwards
        bool up,
        //! Absolute: newValue = oldValue + shiftSize. Relative: newValue = oldValue * (1 + shiftSize)
        ShiftType type,
        //! Shift tenors expressed as times
        const vector<Time>& shiftTimes,
        //! Input curve values such as zero rates
        const vector<Real>& values,
        //! Tenor points of the input curve, expressed as times
        const vector<Time>& times,
        //! Resulting shifted curve with same tenor structure as the input curve
        vector<Real>& shiftedValues,
        //! Initialise shiftedValues vector before applying this shift j (yes for sensitivity, no for stress)
        bool initialise);

    //! Apply 2d shift to 2d matrix such as swaption volatilities, public to allow test suite access
    /*! This is the 2d generalisation of the 1d version of applyShift()
     */
    void applyShift(
        //! Index of the shift tenor in "expiry" direction
        Size j,
        //! Index of the shift tenor in "term" (Swaptions) or "strike" (Caps) direction
        Size k,
        //! Shift size interpreted as either absolute or relative shift
        Real shiftSize,
        //! Upwards shift if true, otherwise downwards
        bool up,
        //! Absolute: newValue = oldValue + shiftSize. Relative: newValue = oldValue * (1 + shiftSize)
        ShiftType type,
        //! Coordinate time in "expiry" direction of the shift curve
        const vector<Time>& shiftX,
        //! Coordinate time in "term" or "strike" direction of the shift curve
        const vector<Time>& shiftY,
        //! Coordinate time in "expiry" direction of the underlying data
        const vector<Time>& dataX,
        //! Coordinate time in "term" or "strike" direction of the underlying data
        const vector<Time>& dataY,
        //! Matrix of input  data
        const vector<vector<Real>>& data,
        //! Matrix of shifted result data
        vector<vector<Real>>& shiftedData,
        //! Initialise shiftedData vector before applying this shift j/k (yes for sensitivity, no for stress)
        bool initialise);

    //! return the base scenario
    boost::shared_ptr<Scenario> baseScenario() const { return scenarios_.front(); }

protected:
    const boost::shared_ptr<Scenario> baseScenario_;
    const boost::shared_ptr<ScenarioSimMarketParameters> simMarketData_;
    std::vector<boost::shared_ptr<Scenario>> scenarios_;
    Size counter_;
    std::vector<ScenarioDescription> scenarioDescriptions_;
    // map risk factor key to "factor", i.e. human readable text representation
    std::map<RiskFactorKey, std::string> keyToFactor_;
    // reverse map of factors to risk factor keys
    std::map<std::string, RiskFactorKey> factorToKey_;
};

ShiftScenarioGenerator::ShiftType parseShiftType(const std::string& s);
} // namespace analytics
} // namespace ore
