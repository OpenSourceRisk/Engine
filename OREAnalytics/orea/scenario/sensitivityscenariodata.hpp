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

#include <ored/utilities/parsers.hpp>
#include <ored/utilities/xmlutils.hpp>
#include <qle/termstructures/dynamicstype.hpp>

using QuantLib::Period;
using QuantLib::Rate;
using std::vector;
using std::string;
using std::pair;
using ore::data::XMLSerializable;
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

    struct ShiftData {
        virtual ~ShiftData() {}
        ShiftData() : shiftSize(0.0) {}
        string shiftType;
        Real shiftSize;
    };

    struct CurveShiftData : ShiftData {
        virtual ~CurveShiftData() {}
        vector<Period> shiftTenors;
    };

    struct SpotShiftData : ShiftData {};

    struct CdsVolShiftData : ShiftData {
        string ccy;
        vector<Period> shiftExpiries;
    };

    struct BaseCorrelationShiftData : ShiftData {
        vector<Period> shiftTerms;
        vector<Real> shiftLossLevels;
        string indexName;
    };

    struct VolShiftData : ShiftData {
        VolShiftData() : shiftStrikes({0.0}) {}
        vector<Period> shiftExpiries;
        vector<Real> shiftStrikes; // FIXME: absolute or relative to ATM ?
    };

    struct CapFloorVolShiftData : VolShiftData {
        string indexName;
    };

    struct SwaptionVolShiftData : VolShiftData {
        vector<Period> shiftTerms;
        string indexName;
    };

    //! Default constructor
    SensitivityScenarioData(){};

    //! \name Inspectors
    //@{
    const map<string, boost::shared_ptr<CurveShiftData>>& discountCurveShiftData() const { return discountCurveShiftData_; }
    const map<string, boost::shared_ptr<CurveShiftData>>& indexCurveShiftData() const { return indexCurveShiftData_; }
    const map<string, boost::shared_ptr<CurveShiftData>>& yieldCurveShiftData() const { return yieldCurveShiftData_; }
    const map<string, SpotShiftData>& fxShiftData() const { return fxShiftData_; }
    const map<string, CapFloorVolShiftData>& capFloorVolShiftData() const { return capFloorVolShiftData_; }
    const map<string, SwaptionVolShiftData>& swaptionVolShiftData() const { return swaptionVolShiftData_; }
    const map<string, VolShiftData>& fxVolShiftData() const { return fxVolShiftData_; }
    const map<string, CdsVolShiftData>& cdsVolShiftData() const { return cdsVolShiftData_; }
    const map<string, BaseCorrelationShiftData>& baseCorrelationShiftData() const { return baseCorrelationShiftData_; }
    const map<string, boost::shared_ptr<CurveShiftData>>& zeroInflationCurveShiftData() const { return zeroInflationCurveShiftData_; }
    const map<string, boost::shared_ptr<CurveShiftData>>& yoyInflationCurveShiftData() const { return yoyInflationCurveShiftData_; }
    const map<string, string>& creditCcys() const { return creditCcys_; }
    const map<string, boost::shared_ptr<CurveShiftData>>& creditCurveShiftData() const { return creditCurveShiftData_; }
    const map<string, SpotShiftData>& equityShiftData() const { return equityShiftData_; }
    const map<string, VolShiftData>& equityVolShiftData() const { return equityVolShiftData_; }
    const map<string, boost::shared_ptr<CurveShiftData>>& equityForecastCurveShiftData() const { return equityForecastCurveShiftData_; }
    const map<string, boost::shared_ptr<CurveShiftData>>& dividendYieldShiftData() const { return dividendYieldShiftData_; }
    const map<string, SpotShiftData>& commodityShiftData() const { return commodityShiftData_; }
    const map<string, string>& commodityCurrencies() const { return commodityCurrencies_; }
    const map<string, boost::shared_ptr<CurveShiftData>>& commodityCurveShiftData() const { return commodityCurveShiftData_; }
    const map<string, VolShiftData>& commodityVolShiftData() const { return commodityVolShiftData_; }
    const map<string, SpotShiftData>& securityShiftData() const { return securityShiftData_; }

    const vector<pair<string, string>>& crossGammaFilter() const { return crossGammaFilter_; }

    //@}

    //! \name Setters
    //@{
    map<string, boost::shared_ptr<CurveShiftData>>& discountCurveShiftData() { return discountCurveShiftData_; }
    map<string, boost::shared_ptr<CurveShiftData>>& indexCurveShiftData() { return indexCurveShiftData_; }
    map<string, boost::shared_ptr<CurveShiftData>>& yieldCurveShiftData() { return yieldCurveShiftData_; }
    map<string, SpotShiftData>& fxShiftData() { return fxShiftData_; }
    map<string, SwaptionVolShiftData>& swaptionVolShiftData() { return swaptionVolShiftData_; }
    map<string, CapFloorVolShiftData>& capFloorVolShiftData() { return capFloorVolShiftData_; }
    map<string, VolShiftData>& fxVolShiftData() { return fxVolShiftData_; }
    map<string, CdsVolShiftData>& cdsVolShiftData() { return cdsVolShiftData_; }
    map<string, BaseCorrelationShiftData>& baseCorrelationShiftData() { return baseCorrelationShiftData_; }
    map<string, boost::shared_ptr<CurveShiftData>>& zeroInflationCurveShiftData() { return zeroInflationCurveShiftData_; }
    map<string, string>& creditCcys() { return creditCcys_; }
    map<string, boost::shared_ptr<CurveShiftData>>& creditCurveShiftData() { return creditCurveShiftData_; }
    map<string, boost::shared_ptr<CurveShiftData>>& yoyInflationCurveShiftData() { return yoyInflationCurveShiftData_; }
    map<string, SpotShiftData>& equityShiftData() { return equityShiftData_; }
    map<string, boost::shared_ptr<CurveShiftData>>& equityForecastCurveShiftData() { return equityForecastCurveShiftData_; }
    map<string, boost::shared_ptr<CurveShiftData>>& dividendYieldShiftData() { return dividendYieldShiftData_; }
    map<string, VolShiftData>& equityVolShiftData() { return equityVolShiftData_; }
    map<string, SpotShiftData>& commodityShiftData() { return commodityShiftData_; }
    map<string, string>& commodityCurrencies() { return commodityCurrencies_; }
    map<string, boost::shared_ptr<CurveShiftData>>& commodityCurveShiftData() { return commodityCurveShiftData_; }
    map<string, VolShiftData>& commodityVolShiftData() { return commodityVolShiftData_; }
    map<string, SpotShiftData>& securityShiftData() { return securityShiftData_; }

    vector<pair<string, string>>& crossGammaFilter() { return crossGammaFilter_; }

    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(ore::data::XMLDocument& doc);
    //@}

    //! \name Equality Operators
    //@{
    // bool operator==(const SensitivityScenarioData& rhs);
    // bool operator!=(const SensitivityScenarioData& rhs);
    //@}

    //! Utilities
    //@{
    string getIndexCurrency(string indexName);
    //@}

protected:
    void curveShiftDataFromXML(XMLNode* child, CurveShiftData& data);
    void shiftDataFromXML(XMLNode* child, ShiftData& data);
    void volShiftDataFromXML(XMLNode* child, VolShiftData& data);

    map<string, boost::shared_ptr<CurveShiftData>> discountCurveShiftData_; // key: ccy
    map<string, boost::shared_ptr<CurveShiftData>> indexCurveShiftData_; // key: indexName
    map<string, boost::shared_ptr<CurveShiftData>> yieldCurveShiftData_; // key: yieldCurveName
    map<string, SpotShiftData> fxShiftData_; // key: ccy pair
    map<string, CapFloorVolShiftData> capFloorVolShiftData_; // key: ccy
    map<string, SwaptionVolShiftData> swaptionVolShiftData_; // key: ccy
    map<string, VolShiftData> fxVolShiftData_; // key: ccy pair
    map<string, CdsVolShiftData> cdsVolShiftData_; // key: ccy pair
    map<string, BaseCorrelationShiftData> baseCorrelationShiftData_;
    map<string, boost::shared_ptr<CurveShiftData>> zeroInflationCurveShiftData_; // key: inflation index name
    map<string, boost::shared_ptr<CurveShiftData>> yoyInflationCurveShiftData_; // key: yoy inflation index name
    map<string, string> creditCcys_;
    map<string, boost::shared_ptr<CurveShiftData>> creditCurveShiftData_; // key: credit name
    map<string, SpotShiftData> equityShiftData_; // key: equity name
    map<string, VolShiftData> equityVolShiftData_; // key: equity name
    map<string, boost::shared_ptr<CurveShiftData>> equityForecastCurveShiftData_; // key: equity name
    map<string, boost::shared_ptr<CurveShiftData>> dividendYieldShiftData_; // key: equity name
    map<string, SpotShiftData> commodityShiftData_;
    map<string, std::string> commodityCurrencies_;
    map<string, boost::shared_ptr<CurveShiftData>> commodityCurveShiftData_;
    map<string, VolShiftData> commodityVolShiftData_;
    map<string, SpotShiftData> securityShiftData_; // key: security name

    vector<pair<string, string>> crossGammaFilter_;
};
} // namespace analytics
} // namespace ore
