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

/*! \file scenario/stressscenariodata.hpp
    \brief A class to hold the parametrisation for building sensitivity scenarios
    \ingroup scenario
*/

#pragma once

#include <orea/scenario/scenario.hpp>

#include <ored/utilities/parsers.hpp>
#include <ored/utilities/xmlutils.hpp>

#include <qle/termstructures/dynamicstype.hpp>

namespace ore {
namespace analytics {
using namespace QuantLib;
using ore::data::XMLNode;
using ore::data::XMLSerializable;
using ore::data::XMLUtils;
using std::map;
using std::pair;
using std::string;
using std::vector;

//! Description of sensitivity shift scenarios
/*! \ingroup scenario
 */
class StressTestScenarioData : public XMLSerializable {
public:
    struct CurveShiftData {
        ShiftType shiftType;
        vector<Real> shifts;
        vector<Period> shiftTenors;
    };

    struct SpotShiftData {
        ShiftType shiftType;
        Real shiftSize;
    };

    struct VolShiftData {
        ShiftType shiftType;
        vector<Period> shiftExpiries;
        vector<Real> shifts; 
    };

    struct CapFloorVolShiftData {
        ShiftType shiftType;
        vector<Period> shiftExpiries;
        vector<double> shiftStrikes;
        std::map<Period, vector<Real>> shifts;
    };
    struct SwaptionVolShiftData {
        ShiftType shiftType;
        Real parallelShiftSize;
        vector<Period> shiftExpiries;
        vector<Period> shiftTerms;
        map<pair<Period, Period>, Real> shifts;
    };

    struct StressTestData {
        string label;
        map<string, CurveShiftData> discountCurveShifts;       // by currency code
        map<string, CurveShiftData> indexCurveShifts;          // by index name
        map<string, CurveShiftData> yieldCurveShifts;          // by yield curve name
        map<string, SpotShiftData> fxShifts;                   // by currency pair
        map<string, VolShiftData> fxVolShifts;                 // by currency pair
        map<string, SpotShiftData> equityShifts;               // by equity
        map<string, VolShiftData> equityVolShifts;             // by equity
        map<string, CapFloorVolShiftData> capVolShifts;        // by currency
        map<string, SwaptionVolShiftData> swaptionVolShifts;   // by currency
        map<string, SpotShiftData> securitySpreadShifts;       // by bond/security
        map<string, SpotShiftData> recoveryRateShifts;         // by underlying name
        map<string, CurveShiftData> survivalProbabilityShifts; // by underlying name
        bool irCurveParShifts = false;
        bool irCapFloorParShifts = false;
        bool creditCurveParShifts = false;

        bool containsParShifts() const { return irCurveParShifts || irCapFloorParShifts || creditCurveParShifts; };        
    };

    //! Default constructor
    StressTestScenarioData(){};

    //! \name Inspectors
    //@{
    const vector<StressTestData>& data() const { return data_; }
    const bool useSpreadedTermStructures() const { return useSpreadedTermStructures_; }
    
    const bool hasScenarioWithParShifts() const {
        for (const auto& scenario : data_) {
            if (scenario.containsParShifts())
                return true;
        }
        return false;
    }

    const bool withIrCurveParShifts() const {
        for (const auto& scenario : data_) {
            if (scenario.irCurveParShifts)
                return true;
        }
        return false;
    }

    const bool withCreditCurveParShifts() const {
        for (const auto& scenario : data_) {
            if (scenario.creditCurveParShifts)
                return true;
        }
        return false;
    }

    const bool withIrCapFloorParShifts() const {
        for (const auto& scenario : data_) {
            if (scenario.irCapFloorParShifts)
                return true;
        }
        return false;
    }
    //@}

    //! \name Setters
    //@{
    vector<StressTestData>& data() { return data_; }
    bool& useSpreadedTermStructures() { return useSpreadedTermStructures_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(ore::data::XMLDocument& doc) const override;
    //@}

    //! \name Equality Operators
    //@{
    // bool operator==(const StressTestScenarioData& rhs);
    // bool operator!=(const StressTestScenarioData& rhs);
    //@}

private:
    vector<StressTestData> data_;
    bool useSpreadedTermStructures_ = false;
};
} // namespace analytics
} // namespace ore
