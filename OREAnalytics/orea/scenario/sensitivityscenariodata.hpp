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
    enum class InterestRateDomain { ZeroRate, ParRate };
    enum class CapFloorVolDomain { OptionletVol, FlatVol };
    enum class InflationDomain { ZeroRate, ParRate };
    enum class CreditDomain { HazardRate, Spread };
    enum class ShiftType { Absolute, Relative };

    //! Default constructor
    SensitivityScenarioData(){};

    //! \name Inspectors
    //@{
    const vector<string>& discountCurrencies() const { return discountCurrencies_; }
    const string& discountLabel() const { return discountLabel_; }
    const string& discountDomain() const { return discountDomain_; }
    const vector<Period>& discountShiftTenors() const { return discountShiftTenors_; }
    const string& discountShiftType() const { return discountShiftType_; }
    const Real& discountShiftSize() const { return discountShiftSize_; }
    const vector<string>& discountParInstruments() const { return discountParInstruments_; }
    const bool& discountParInstrumentsSingleCurve() const { return discountParInstrumentsSingleCurve_; }
    const map<pair<string, string>, string>& discountParInstrumentConventions() const {
        return discountParInstrumentConventions_;
    }

    const vector<string>& indexNames() const { return indexNames_; }
    const string& indexLabel() const { return indexLabel_; }
    const string& indexDomain() const { return indexDomain_; }
    const vector<Period>& indexShiftTenors() const { return indexShiftTenors_; }
    const string& indexShiftType() const { return indexShiftType_; }
    const Real& indexShiftSize() const { return indexShiftSize_; }

    const vector<string>& infIndices() const { return infIndices_; }
    const string& infLabel() const { return infLabel_; }
    const string& infDomain() const { return infDomain_; }
    const vector<Period>& infShiftTenors() const { return infShiftTenors_; }
    const string& infShiftType() const { return infShiftType_; }
    const Real& infShiftSize() const { return infShiftSize_; }

    const vector<string>& fxCcyPairs() const { return fxCcyPairs_; }
    const string& fxLabel() const { return fxLabel_; }
    const string& fxShiftType() const { return fxShiftType_; }
    const Real& fxShiftSize() const { return fxShiftSize_; }

    const vector<string>& swaptionVolCurrencies() const { return swaptionVolCurrencies_; }
    const string& swaptionVolLabel() const { return swaptionVolLabel_; }
    const vector<Period>& swaptionVolShiftExpiries() const { return swaptionVolShiftExpiries_; }
    const vector<Period>& swaptionVolShiftTerms() const { return swaptionVolShiftTerms_; }
    const vector<Real>& swaptionVolShiftStrikes() const { return swaptionVolShiftStrikes_; }
    const string& swaptionVolShiftType() const { return swaptionVolShiftType_; }
    const Real& swaptionVolShiftSize() const { return swaptionVolShiftSize_; }

    const vector<string>& capFloorVolCurrencies() const { return capFloorVolCurrencies_; }
    const string& capFloorVolLabel() const { return capFloorVolLabel_; }
    const vector<Period>& capFloorVolShiftExpiries() const { return capFloorVolShiftExpiries_; }
    const vector<Real>& capFloorVolShiftStrikes() const { return capFloorVolShiftStrikes_; }
    const string& capFloorVolShiftType() const { return capFloorVolShiftType_; }
    const Real& capFloorVolShiftSize() const { return capFloorVolShiftSize_; }
    const map<string, string>& capFloorVolIndexMapping() const { return capFloorVolIndexMapping_; }

    const vector<string>& fxVolCcyPairs() const { return fxVolCcyPairs_; }
    const string& fxVolLabel() const { return fxVolLabel_; }
    const vector<Period>& fxVolShiftExpiries() const { return fxVolShiftExpiries_; }
    const vector<Real>& fxVolShiftStrikes() const { return fxVolShiftStrikes_; }
    const string& fxVolShiftType() const { return fxVolShiftType_; }
    const Real& fxVolShiftSize() const { return fxVolShiftSize_; }

    const vector<string>& crNames() const { return crNames_; }
    const string& crLabel() const { return crLabel_; }
    const string& crDomain() const { return crDomain_; }
    const vector<Period>& crShiftTenors() const { return crShiftTenors_; }
    const string& crShiftType() const { return crShiftType_; }
    const Real& crShiftSize() const { return crShiftSize_; }

    const vector<pair<string, string> >& crossGammaFilter() const { return crossGammaFilter_; }

    //@}

    //! \name Setters
    //@{
    vector<string>& discountCurrencies() { return discountCurrencies_; }
    string& discountLabel() { return discountLabel_; }
    string& discountDomain() { return discountDomain_; }
    vector<Period>& discountShiftTenors() { return discountShiftTenors_; }
    string& discountShiftType() { return discountShiftType_; }
    Real& discountShiftSize() { return discountShiftSize_; }
    vector<string>& discountParInstruments() { return discountParInstruments_; }
    bool& discountParInstrumentsSingleCurve() { return discountParInstrumentsSingleCurve_; }
    map<pair<string, string>, string>& discountParInstrumentConventions() { return discountParInstrumentConventions_; }

    vector<string>& indexNames() { return indexNames_; }
    string& indexLabel() { return indexLabel_; }
    string& indexDomain() { return indexDomain_; }
    vector<Period>& indexShiftTenors() { return indexShiftTenors_; }
    string& indexShiftType() { return indexShiftType_; }
    Real& indexShiftSize() { return indexShiftSize_; }
    vector<string>& indexParInstruments() { return indexParInstruments_; }
    bool& indexParInstrumentsSingleCurve() { return indexParInstrumentsSingleCurve_; }
    map<pair<string, string>, string>& indexParInstrumentConventions() { return indexParInstrumentConventions_; }

    vector<string>& infIndices() { return infIndices_; }
    string& infLabel() { return infLabel_; }
    string& infDomain() { return infDomain_; }
    vector<Period>& infShiftTenors() { return infShiftTenors_; }
    string& infShiftType() { return infShiftType_; }
    Real& infShiftSize() { return infShiftSize_; }

    vector<string>& fxCcyPairs() { return fxCcyPairs_; }
    string& fxLabel() { return fxLabel_; }
    string& fxShiftType() { return fxShiftType_; }
    Real& fxShiftSize() { return fxShiftSize_; }

    vector<string>& swaptionVolCurrencies() { return swaptionVolCurrencies_; }
    string& swaptionVolLabel() { return swaptionVolLabel_; }
    vector<Period>& swaptionVolShiftExpiries() { return swaptionVolShiftExpiries_; }
    vector<Period>& swaptionVolShiftTerms() { return swaptionVolShiftTerms_; }
    vector<Real>& swaptionVolShiftStrikes() { return swaptionVolShiftStrikes_; }
    string& swaptionVolShiftType() { return swaptionVolShiftType_; }
    Real& swaptionVolShiftSize() { return swaptionVolShiftSize_; }

    vector<string>& capFloorVolCurrencies() { return capFloorVolCurrencies_; }
    string& capFloorVolLabel() { return capFloorVolLabel_; }
    vector<Period>& capFloorVolShiftExpiries() { return capFloorVolShiftExpiries_; }
    vector<Real>& capFloorVolShiftStrikes() { return capFloorVolShiftStrikes_; }
    string& capFloorVolShiftType() { return capFloorVolShiftType_; }
    Real& capFloorVolShiftSize() { return capFloorVolShiftSize_; }
    map<string, string>& capFloorVolIndexMapping() { return capFloorVolIndexMapping_; }

    vector<string>& fxVolCcyPairs() { return fxVolCcyPairs_; }
    string& fxVolLabel() { return fxVolLabel_; }
    vector<Period>& fxVolShiftExpiries() { return fxVolShiftExpiries_; }
    vector<Real>& fxVolShiftStrikes() { return fxVolShiftStrikes_; }
    string& fxVolShiftType() { return fxVolShiftType_; }
    Real& fxVolShiftSize() { return fxVolShiftSize_; }

    vector<string>& crNames() { return crNames_; }
    string& crLabel() { return crLabel_; }
    string& crDomain() { return crDomain_; }
    vector<Period>& crShiftTenors() { return crShiftTenors_; }
    string& crShiftType() { return crShiftType_; }
    Real& crShiftSize() { return crShiftSize_; }

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

    vector<string> discountCurrencies_;
    string discountLabel_;
    string discountDomain_;
    vector<Period> discountShiftTenors_;
    string discountShiftType_;
    Real discountShiftSize_;
    vector<string> discountParInstruments_;
    bool discountParInstrumentsSingleCurve_;
    map<pair<string, string>, string> discountParInstrumentConventions_;

    vector<string> indexNames_;
    string indexLabel_;
    string indexDomain_;
    vector<Period> indexShiftTenors_;
    string indexShiftType_;
    Real indexShiftSize_;
    vector<string> indexParInstruments_;
    bool indexParInstrumentsSingleCurve_;
    map<pair<string, string>, string> indexParInstrumentConventions_;

    vector<string> fxCcyPairs_;
    string fxLabel_;
    string fxShiftType_;
    Real fxShiftSize_;

    vector<string> infIndices_;
    string infLabel_;
    string infDomain_;
    vector<Period> infShiftTenors_;
    string infShiftType_;
    Real infShiftSize_;

    vector<string> crNames_;
    string crLabel_;
    string crDomain_;
    vector<Period> crShiftTenors_;
    string crShiftType_;
    Real crShiftSize_;

    vector<string> swaptionVolCurrencies_;
    string swaptionVolLabel_;
    string swaptionVolShiftType_;
    Real swaptionVolShiftSize_;
    vector<Period> swaptionVolShiftExpiries_;
    vector<Period> swaptionVolShiftTerms_;
    vector<Real> swaptionVolShiftStrikes_; // FIXME: absolute or relative to ATM ?

    vector<string> capFloorVolCurrencies_;
    string capFloorVolLabel_;
    string capFloorVolShiftType_;
    Real capFloorVolShiftSize_;
    vector<Period> capFloorVolShiftExpiries_;
    vector<Real> capFloorVolShiftStrikes_; // absolute
    map<string, string> capFloorVolIndexMapping_;

    vector<string> fxVolCcyPairs_;
    string fxVolLabel_;
    string fxVolShiftType_;
    Real fxVolShiftSize_;
    vector<Period> fxVolShiftExpiries_;
    vector<Real> fxVolShiftStrikes_; // FIXME: absolute or relative to ATM ?

    vector<pair<string, string> > crossGammaFilter_;
};
}
}
