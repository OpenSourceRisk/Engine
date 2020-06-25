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
        string shiftType;
        vector<Real> shifts;
        vector<Period> shiftTenors;
    };

    struct SpotShiftData {
        string shiftType;
        Real shiftSize;
    };

    struct VolShiftData {
        string shiftType;
        vector<Period> shiftExpiries;
        vector<Real> shifts;
    };

    struct CapFloorVolShiftData {
        string shiftType;
        vector<Period> shiftExpiries;
        vector<Real> shifts;
    };

    struct SwaptionVolShiftData {
        string shiftType;
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
    };

    //! Default constructor
    StressTestScenarioData(){};

    //! \name Inspectors
    //@{
    const vector<StressTestData>& data() const { return data_; }

    //@}

    //! \name Setters
    //@{
    vector<StressTestData>& data() { return data_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(ore::data::XMLDocument& doc);
    //@}

    //! \name Equality Operators
    //@{
    // bool operator==(const StressTestScenarioData& rhs);
    // bool operator!=(const StressTestScenarioData& rhs);
    //@}

private:
    vector<StressTestData> data_;
};
} // namespace analytics
} // namespace ore
