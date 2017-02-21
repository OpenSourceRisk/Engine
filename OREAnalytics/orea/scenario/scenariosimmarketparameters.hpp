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

/*! \file scenario/scenariosimmarketparameters.hpp
    \brief A class to hold Scenario parameters for scenarioSimMarket
    \ingroup scenario
*/

#pragma once

#include <ored/utilities/xmlutils.hpp>
#include <ored/utilities/parsers.hpp>
#include <qle/termstructures/dynamicstype.hpp>

using QuantLib::Period;
using QuantLib::Rate;
using std::vector;
using std::string;
using std::pair;
using ore::data::XMLSerializable;
using ore::data::XMLDocument;
using ore::data::XMLNode;
using ore::data::XMLUtils;

namespace ore {
namespace analytics {

//! ScenarioSimMarket description
/*! \ingroup scenario
*/
class ScenarioSimMarketParameters : public XMLSerializable {
public:
    //! Default constructor
    ScenarioSimMarketParameters(){};

    //! \name Inspectors
    //@{
    const string& baseCcy() const { return baseCcy_; }
    const vector<string>& ccys() const { return ccys_; }
    const vector<string>& yieldCurveNames() const { return yieldCurveNames_; }
    const vector<string>& yieldCurveCurrencies() const { return yieldCurveCurrencies_; }
    const vector<Period>& yieldCurveTenors() const { return yieldCurveTenors_; }
    const vector<string>& indices() const { return indices_; }
    const map<string, string>& swapIndices() const { return swapIndices_; }
    const string& interpolation() const { return interpolation_; }
    const bool& extrapolate() const { return extrapolate_; }

    const vector<string>& fxCcyPairs() const { return fxCcyPairs_; }

    bool simulateSwapVols() const { return swapVolSimulate_; }
    const vector<Period>& swapVolTerms() const { return swapVolTerms_; }
    const vector<Period>& swapVolExpiries() const { return swapVolExpiries_; }
    const vector<string>& swapVolCcys() const { return swapVolCcys_; }
    const string& swapVolDecayMode() const { return swapVolDecayMode_; }

    const bool& simulateCapFloorVols() const { return capFloorVolSimulate_; }
    const vector<string>& capFloorVolCcys() const { return capFloorVolCcys_; }
    const vector<Period>& capFloorVolExpiries() const { return capFloorVolExpiries_; }
    const vector<Real>& capFloorVolStrikes() const { return capFloorVolStrikes_; }
    const string& capFloorVolDecayMode() const { return capFloorVolDecayMode_; }

    const vector<string>& defaultNames() const { return defaultNames_; }
    const vector<Period>& defaultTenors() const { return defaultTenors_; }

    bool simulateFXVols() const { return fxVolSimulate_; }
    const vector<Period>& fxVolExpiries() const { return fxVolExpiries_; }
    const string& fxVolDecayMode() const { return fxVolDecayMode_; }
    const vector<string>& fxVolCcyPairs() const { return fxVolCcyPairs_; }

    const vector<string>& additionalScenarioDataIndices() const { return additionalScenarioDataIndices_; }
    const vector<string>& additionalScenarioDataCcys() const { return additionalScenarioDataCcys_; }

    const vector<string>& securities() const { return securities_; }
    //@}

    //! \name Setters
    //@{
    string& baseCcy() { return baseCcy_; }
    vector<string>& ccys() { return ccys_; }
    vector<string>& yieldCurveNames() { return yieldCurveNames_; }
    vector<string>& yieldCurveCurrencies() { return yieldCurveCurrencies_; }
    vector<Period>& yieldCurveTenors() { return yieldCurveTenors_; }
    vector<string>& indices() { return indices_; }
    map<string, string>& swapIndices() { return swapIndices_; }
    string& interpolation() { return interpolation_; }
    bool& extrapolate() { return extrapolate_; }

    vector<string>& fxCcyPairs() { return fxCcyPairs_; }

    bool& simulateSwapVols() { return swapVolSimulate_; }
    vector<Period>& swapVolTerms() { return swapVolTerms_; }
    vector<string>& swapVolCcys() { return swapVolCcys_; }
    vector<Period>& swapVolExpiries() { return swapVolExpiries_; }
    string& swapVolDecayMode() { return swapVolDecayMode_; }

    bool& simulateCapFloorVols() { return capFloorVolSimulate_; }
    vector<string>& capFloorVolCcys() { return capFloorVolCcys_; }
    vector<Period>& capFloorVolExpiries() { return capFloorVolExpiries_; }
    vector<Real>& capFloorVolStrikes() { return capFloorVolStrikes_; }
    string& capFloorVolDecayMode() { return capFloorVolDecayMode_; }

    vector<string>& defaultNames() { return defaultNames_; }
    vector<Period>& defaultTenors() { return defaultTenors_; }

    bool& simulateFXVols() { return fxVolSimulate_; }
    vector<Period>& fxVolExpiries() { return fxVolExpiries_; }
    string& fxVolDecayMode() { return fxVolDecayMode_; }
    vector<string>& fxVolCcyPairs() { return fxVolCcyPairs_; }

    vector<string>& additionalScenarioDataIndices() { return additionalScenarioDataIndices_; }
    vector<string>& additionalScenarioDataCcys() { return additionalScenarioDataCcys_; }

    vector<string>& securities() { return securities_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc);
    //@}

    //! \Equality Operators
    //@{
    bool operator==(const ScenarioSimMarketParameters& rhs);
    bool operator!=(const ScenarioSimMarketParameters& rhs);
    //@}

private:
    string baseCcy_;
    vector<string> ccys_; // may or may not include baseCcy;
    vector<string> yieldCurveNames_;
    vector<string> yieldCurveCurrencies_;
    vector<Period> yieldCurveTenors_;
    vector<string> indices_;
    map<string, string> swapIndices_;
    string interpolation_;
    bool extrapolate_;

    vector<string> fxCcyPairs_;

    bool swapVolSimulate_;
    vector<Period> swapVolTerms_;
    vector<string> swapVolCcys_;
    vector<Period> swapVolExpiries_;
    string swapVolDecayMode_;

    bool capFloorVolSimulate_;
    vector<string> capFloorVolCcys_;
    vector<Period> capFloorVolExpiries_;
    vector<Real> capFloorVolStrikes_;
    string capFloorVolDecayMode_;

    vector<string> defaultNames_;
    vector<Period> defaultTenors_;

    bool fxVolSimulate_;
    vector<Period> fxVolExpiries_;
    string fxVolDecayMode_;
    vector<string> fxVolCcyPairs_;

    vector<string> additionalScenarioDataIndices_;
    vector<string> additionalScenarioDataCcys_;

    vector<string> securities_;
};
}
}
