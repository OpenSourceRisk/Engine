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

/*! \file scenario/sensitivityscenariodata.hpp
    \brief A class to hold the parametrisation for building sensitivity scenarios
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

//! Description of sensitivity shift scenarios
/*! \ingroup scenario
*/
class SensitivityScenarioData : public XMLSerializable {
public:
    enum class ShiftType { Absolute, Relative };

    struct CurveShiftData {
        string shiftType;
        Real shiftSize;
        vector<Period> shiftTenors;
        vector<string> parInstruments;
        bool parInstrumentSingleCurve;
        map<string, string> parInstrumentConventions;
    };

    struct CapFloorVolShiftData {
        string shiftType;
        Real shiftSize;
        vector<Period> shiftExpiries;
        vector<Real> shiftStrikes; // absolute
        string indexName;
    };

    struct SwaptionVolShiftData {
        string shiftType;
        Real shiftSize;
        vector<Period> shiftExpiries;
        vector<Period> shiftTerms;
        vector<Real> shiftStrikes; // FIXME: absolute or relative to ATM ?
        string indexName;
    };

    struct FxVolShiftData {
        string shiftType;
        Real shiftSize;
        vector<Period> shiftExpiries;
        vector<Real> shiftStrikes; // FIXME: absolute or relative to ATM ?
    };

    struct FxShiftData {
        string shiftType;
        Real shiftSize;
    };

    //! Default constructor
    SensitivityScenarioData(){};

    //! \name Inspectors
    //@{
    bool parConversion() const { return parConversion_; }

    const string& discountLabel() const { return discountLabel_; }
    const vector<string>& discountCurrencies() const { return discountCurrencies_; }
    const map<string, CurveShiftData>& discountCurveShiftData() const { return discountCurveShiftData_; }

    const string& indexLabel() const { return indexLabel_; }
    const vector<string>& indexNames() const { return indexNames_; }
    const map<string, CurveShiftData>& indexCurveShiftData() const { return indexCurveShiftData_; }

    const string& yieldLabel() const { return yieldLabel_; }
    const vector<string>& yieldCurveNames() const { return yieldCurveNames_; }
    const map<string, CurveShiftData>& yieldCurveShiftData() const { return yieldCurveShiftData_; }

    const string& fxLabel() const { return fxLabel_; }
    const vector<string>& fxCcyPairs() const { return fxCcyPairs_; }
    const map<string, FxShiftData>& fxShiftData() const { return fxShiftData_; }

    const string& swaptionVolLabel() const { return swaptionVolLabel_; }
    const vector<string>& swaptionVolCurrencies() const { return swaptionVolCurrencies_; }
    const map<string, SwaptionVolShiftData>& swaptionVolShiftData() const { return swaptionVolShiftData_; }

    const string& capFloorVolLabel() const { return capFloorVolLabel_; }
    const vector<string>& capFloorVolCurrencies() const { return capFloorVolCurrencies_; }
    const map<string, CapFloorVolShiftData>& capFloorVolShiftData() const { return capFloorVolShiftData_; }

    const string& fxVolLabel() const { return fxVolLabel_; }
    const vector<string>& fxVolCcyPairs() const { return fxVolCcyPairs_; }
    const map<string, FxVolShiftData>& fxVolShiftData() const { return fxVolShiftData_; }

    // todo
    // const vector<string>& infIndices() const { return inflationIndices_; }
    // const map<string, InflationCurveShiftData> inflationCurveShiftData() const { return inflationCurveShiftData_; }

    // todo
    // const vector<string>& creditNames() const { return creditNames_; }
    // const map<string, CreditCurveShiftData> creditCurveShiftData() const { return creditCurveShiftData_; }

    const vector<pair<string, string> >& crossGammaFilter() const { return crossGammaFilter_; }

    //@}

    //! \name Setters
    //@{
    bool& parConversion() { return parConversion_; }

    string& discountLabel() { return discountLabel_; }
    vector<string>& discountCurrencies() { return discountCurrencies_; }
    map<string, CurveShiftData>& discountCurveShiftData() { return discountCurveShiftData_; }

    string& indexLabel() { return indexLabel_; }
    vector<string>& indexNames() { return indexNames_; }
    map<string, CurveShiftData>& indexCurveShiftData() { return indexCurveShiftData_; }

    string& yieldLabel() { return yieldLabel_; }
    vector<string>& yieldCurveNames() { return yieldCurveNames_; }
    map<string, CurveShiftData>& yieldCurveShiftData() { return yieldCurveShiftData_; }

    string& fxLabel() { return fxLabel_; }
    vector<string>& fxCcyPairs() { return fxCcyPairs_; }
    map<string, FxShiftData>& fxShiftData() { return fxShiftData_; }

    string& swaptionVolLabel() { return swaptionVolLabel_; }
    vector<string>& swaptionVolCurrencies() { return swaptionVolCurrencies_; }
    map<string, SwaptionVolShiftData>& swaptionVolShiftData() { return swaptionVolShiftData_; }

    string& capFloorVolLabel() { return capFloorVolLabel_; }
    vector<string>& capFloorVolCurrencies() { return capFloorVolCurrencies_; }
    map<string, CapFloorVolShiftData>& capFloorVolShiftData() { return capFloorVolShiftData_; }

    string& fxVolLabel() { return fxVolLabel_; }
    vector<string>& fxVolCcyPairs() { return fxVolCcyPairs_; }
    map<string, FxVolShiftData>& fxVolShiftData() { return fxVolShiftData_; }

    // todo
    // vector<string>& infIndices() { return inflationIndices_; }
    // map<string, InflationCurveShiftData> inflationCurveShiftData() { return inflationCurveShiftData_; }

    // todo
    // vector<string>& creditNames() { return creditNames_; }
    // map<string, CreditCurveShiftData> creditCurveShiftData() { return creditCurveShiftData_; }

    vector<pair<string, string> >& crossGammaFilter() { return crossGammaFilter_; }

    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc);
    //@}

    //! \Equality Operators
    //@{
    // bool operator==(const SensitivityScenarioData& rhs);
    // bool operator!=(const SensitivityScenarioData& rhs);
    //@}

    //! Utilities for constructing and interpreting shift scenario labels
    //@{
    string discountShiftScenarioLabel(string ccy, Size bucket, bool up);
    string indexShiftScenarioLabel(string ccy, Size bucket, bool up);
    string yieldShiftScenarioLabel(string name, Size bucket, bool up);
    string fxShiftScenarioLabel(string ccypair, bool up);
    string swaptionVolShiftScenarioLabel(string ccy, Size expiryBucket, Size termBucket, Size strikeBucket, bool up);
    string capFloorVolShiftScenarioLabel(string ccy, Size expiryBucket, Size strikeBucket, bool up);
    string fxVolShiftScenarioLabel(string ccypair, Size expiryBucket, Size strikeBucket, bool up);
    string shiftDirectionLabel(bool up);
    string labelSeparator();
    string labelToFactor(string label);
    bool isBaseScenario(string label);
    bool isSingleShiftScenario(string label);
    bool isCrossShiftScenario(string label);
    bool isUpShiftScenario(string label);
    bool isDownShiftScenario(string label);
    bool isYieldShiftScenario(string label);
    bool isCapFloorVolShiftScenario(string label);
    string getCrossShiftScenarioLabel(string label, Size factor);
    string getIndexCurrency(string indexName);
    //@}

private:
    //! Return copy of input string with ending removed
    string remove(const string& input, const string& ending);

    bool parConversion_;

    string discountLabel_;
    vector<string> discountCurrencies_;
    map<string, CurveShiftData> discountCurveShiftData_; // key: ccy

    string indexLabel_;
    vector<string> indexNames_;
    map<string, CurveShiftData> indexCurveShiftData_; // key: indexName

    string yieldLabel_;
    vector<string> yieldCurveNames_;
    map<string, CurveShiftData> yieldCurveShiftData_; // key: yieldCurveName

    string capFloorVolLabel_;
    vector<string> capFloorVolCurrencies_;
    map<string, CapFloorVolShiftData> capFloorVolShiftData_; // key: ccy

    string swaptionVolLabel_;
    vector<string> swaptionVolCurrencies_;
    map<string, SwaptionVolShiftData> swaptionVolShiftData_; // key: ccy

    string fxVolLabel_;
    vector<string> fxVolCcyPairs_;
    map<string, FxVolShiftData> fxVolShiftData_; // key: ccy pair

    string fxLabel_;
    vector<string> fxCcyPairs_;
    map<string, FxShiftData> fxShiftData_; // key: ccy pair

    // todo
    string inflationLabel_;
    vector<string> inflationIndices_;
    map<string, CurveShiftData> inflationCurveShiftData_; // key: inflation index name

    // todo
    string creditLabel_;
    vector<string> creditNames_;
    map<string, CurveShiftData> creditCurveShiftData_; // key: credit name

    vector<pair<string, string> > crossGammaFilter_;
};
}
}
