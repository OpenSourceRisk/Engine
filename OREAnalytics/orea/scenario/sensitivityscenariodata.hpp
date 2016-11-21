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
    enum class InterestRateDomain {
      ZeroRate,
      ParRate
    };
    enum class InflationDomain {
      ZeroRate,
      ParRate
    };
    enum class CreditDomain {
      HazardRate,
      Spread
    };
    enum class ShiftType {
      Absolute,
      Relative
    };

    //! Default constructor
    SensitivityScenarioData(){};

    //! \name Inspectors
    //@{
    // const vector<string>& irCurrencies() const { return irCurrencies_; }
    // const vector<string>& irIndices() const { return irIndices_; }
    const bool& irByCurrency() const { return irByCurrency_; }
    const string& irDomain() const { return irDomain_; }
    const vector<Period>& irShiftTenors() const { return irShiftTenors_; }
    const string& irShiftType() const { return irShiftType_; }
    const Real& irShiftSize() const { return irShiftSize_; }

    // const vector<string>& infIndices() const { return infIndices_; }
    // const string& infDomain() const { return infDomain_; }
    const vector<Period>& infShiftTenors() const { return infShiftTenors_; }
    const string& infShiftType() const { return infShiftType_; }
    const Real& infShiftSize() const { return infShiftSize_; }

    // const vector<string>& fxCurrencyPairs() const { return fxCurrencyPairs_; }
    const string& fxShiftType() const { return fxShiftType_; }
    const Real& fxShiftSize() const { return fxShiftSize_; }

    // const vector<string>& swaptionVolCurrencies() const { return swaptionVolCurrencies_; }
    const vector<Period>& swaptionVolShiftExpiries() const { return swaptionVolShiftExpiries_; }
    const vector<Period>& swaptionVolShiftTerms() const { return swaptionVolShiftTerms_; }
    const vector<Real>& swaptionVolShiftStrikes() const { return swaptionVolShiftStrikes_; }
    const string& swaptionVolShiftType() const { return swaptionVolShiftType_; }
    const Real& swaptionVolShiftSize() const { return swaptionVolShiftSize_; }

    // const vector<string>& fxVolPairs() const { return fxVolCurrencies_; }
    const vector<Period>& fxVolShiftExpiries() const { return fxVolShiftExpiries_; }
    const vector<Real>& fxVolShiftStrikes() const { return fxVolShiftStrikes_; }
    const string& fxVolShiftType() const { return fxVolShiftType_; }
    const Real& fxVolShiftSize() const { return fxVolShiftSize_; }

    // const vector<string>& crNames() const { return crNames_; }
    const string& crDomain() const { return crDomain_; }
    const vector<Period>& crShiftTenors() const { return crShiftTenors_; }
    const string& crShiftType() const { return crShiftType_; }
    const Real& crShiftSize() const { return crShiftSize_; }
    //@}

    //! \name Setters
    //@{
    // vector<string>& irCurrencies()  { return irCurrencies_; }
    // vector<string>& irIndices()  { return irIndices_; }
    bool& irByCurrency()  { return irByCurrency_; }
    string& irDomain() { return irDomain_; }
    vector<Period>& irShiftTenors() { return irShiftTenors_; }
    string& irShiftType() { return irShiftType_; }
    Real& irShiftSize() { return irShiftSize_; }

    // vector<string>& infIndices()  { return infIndices_; }
    string& infDomain()  { return infDomain_; }
    vector<Period>& infShiftTenors()  { return infShiftTenors_; }
    string& infShiftType()  { return infShiftType_; }
    Real& infShiftSize()  { return infShiftSize_; }

    // vector<string>& fxCurrencyPairs()  { return fxCurrencyPairs_; }
    string& fxShiftType()  { return fxShiftType_; }
    Real& fxShiftSize()  { return fxShiftSize_; }

    // vector<string>& swaptionVolCurrencies()  { return swaptionVolCurrencies_; }
    vector<Period>& swaptionVolShiftExpiries()  { return swaptionVolShiftExpiries_; }
    vector<Period>& swaptionVolShiftTerms()  { return swaptionVolShiftTerms_; }
    vector<Real>& swaptionVolShiftStrikes()  { return swaptionVolShiftStrikes_; }
    string& swaptionVolShiftType()  { return swaptionVolShiftType_; }
    Real& swaptionVolShiftSize()  { return swaptionVolShiftSize_; }

    // vector<string>& fxVolPairs() { return fxVolCurrencies_; }
    vector<Period>& fxVolShiftExpiries() { return fxVolShiftExpiries_; }
    vector<Real>& fxVolShiftStrikes() { return fxVolShiftStrikes_; }
    string& fxVolShiftType() { return fxVolShiftType_; }
    Real& fxVolShiftSize() { return fxVolShiftSize_; }

    // vector<string>& crNames()  { return crNames_; }
    string& crDomain()  { return crDomain_; }
    vector<Period>& crShiftTenors()  { return crShiftTenors_; }
    string& crShiftType()  { return crShiftType_; }
    Real& crShiftSize()  { return crShiftSize_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc);
    //@}

    //! \Equality Operators
    //@{
    bool operator==(const SensitivityScenarioData& rhs);
    bool operator!=(const SensitivityScenarioData& rhs);
    //@}

private:
    //vector<string> irCurrencies_;
    //vector<string> irIndices_;
    bool irByCurrency_;
    string irDomain_; 
    vector<Period> irShiftTenors_;
    string irShiftType_;
    Real irShiftSize_;

    //vector<string> fxCurrencyPairs_;
    string fxShiftType_;
    Real fxShiftSize_;

    //vector<string> infIndices_;
    string infDomain_;
    vector<Period> infShiftTenors_;
    string infShiftType_;
    Real infShiftSize_;

    //vector<string> crNames_;
    string crDomain_;
    vector<Period> crShiftTenors_;
    string crShiftType_;
    Real crShiftSize_;

    //vector<string> swaptionVolCurrencies_;
    string swaptionVolShiftType_;
    Real swaptionVolShiftSize_;
    vector<Period> swaptionVolShiftExpiries_;
    vector<Period> swaptionVolShiftTerms_;
    vector<Real> swaptionVolShiftStrikes_; // FIXME: absolute or relative to ATM ?

    //vector<string> fxVolPairs_;
    string fxVolShiftType_;
    Real fxVolShiftSize_;
    vector<Period> fxVolShiftExpiries_;
    vector<Real> fxVolShiftStrikes_; // FIXME: absolute or relative to ATM ?
};
}
}
