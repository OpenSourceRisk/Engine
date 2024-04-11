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

#include <orea/scenario/scenario.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/xmlutils.hpp>
#include <qle/termstructures/dynamicstype.hpp>

namespace ore {
namespace analytics {
using ore::data::XMLNode;
using ore::data::XMLSerializable;
using ore::data::XMLUtils;
using QuantLib::Period;
using QuantLib::Rate;
using std::map;
using std::pair;
using std::string;
using std::vector;

//! Description of sensitivity shift scenarios
/*! \ingroup scenario
 */
class SensitivityScenarioData : public XMLSerializable {
public:
    struct ShiftData {
        virtual ~ShiftData() {}

        // default shift size, type (Absolute, Relative) and scheme (Forward, Backward, Central)
        ShiftType shiftType = ShiftType::Absolute;
        Real shiftSize = 0.0;
        ShiftScheme shiftScheme = ShiftScheme::Forward;

        // product specific shift size, type, scheme
        map<string, ShiftType> keyedShiftType;
        map<string, Real> keyedShiftSize;
        map<string, ShiftScheme> keyedShiftScheme;
    };

    struct CurveShiftData : ShiftData {
        CurveShiftData() : ShiftData() {}
        CurveShiftData(const ShiftData& d) : ShiftData(d) {}
        virtual ~CurveShiftData() {}
        vector<Period> shiftTenors;
    };

    using SpotShiftData = ShiftData;

    struct CdsVolShiftData : ShiftData {
        CdsVolShiftData() : ShiftData() {}
        CdsVolShiftData(const ShiftData& d) : ShiftData(d) {}
        string ccy;
        vector<Period> shiftExpiries;
    };

    struct BaseCorrelationShiftData : ShiftData {
        BaseCorrelationShiftData() : ShiftData() {}
        BaseCorrelationShiftData(const ShiftData& d) : ShiftData(d) {}
        vector<Period> shiftTerms;
        vector<Real> shiftLossLevels;
        string indexName;
    };

    struct VolShiftData : ShiftData {
        VolShiftData() : shiftStrikes({0.0}), isRelative(false) {}
        VolShiftData(const ShiftData& d) : ShiftData(d), shiftStrikes({0.0}), isRelative(false) {}
        vector<Period> shiftExpiries;
        vector<Real> shiftStrikes;
        bool isRelative;
    };

    struct CapFloorVolShiftData : VolShiftData {
        CapFloorVolShiftData() : VolShiftData() {}
        CapFloorVolShiftData(const VolShiftData& d) : VolShiftData(d) {}
        string indexName;
    };

    struct GenericYieldVolShiftData : VolShiftData {
        GenericYieldVolShiftData() : VolShiftData() {}
        GenericYieldVolShiftData(const VolShiftData& d) : VolShiftData(d) {}
        vector<Period> shiftTerms;
    };

    struct CurveShiftParData : CurveShiftData {
        CurveShiftParData() : CurveShiftData() {}
        CurveShiftParData(const CurveShiftData& c) : CurveShiftData(c) {}
        vector<string> parInstruments;
        bool parInstrumentSingleCurve = true;

        // Allows direct specification of discount curve if parInstrumentSingleCurve is false
        // If not given, we default to old behaviour of using the market's discount curve for
        // that currency. This string will be an index name that is searched for in the market.
        std::string discountCurve;

        // Allow specification of the other currency when creating cross currency basis swap
        // par instruments for discount curves. If left empty, we take the base currency of the
        // run as the other leg's currency.
        std::string otherCurrency;

        map<string, string> parInstrumentConventions;
    };

    struct CapFloorVolShiftParData : CapFloorVolShiftData {
        CapFloorVolShiftParData() : CapFloorVolShiftData() {}
        CapFloorVolShiftParData(const CapFloorVolShiftData& c) : CapFloorVolShiftData(c) {}
        vector<string> parInstruments;
        bool parInstrumentSingleCurve = true;

        // Allows direct specification of discount curve
        // If not given, we default to old behaviour of using the market's discount curve for
        // that currency. This string will be an index name that is searched for in the market.
        std::string discountCurve;

        map<string, string> parInstrumentConventions;
    };

    //! Default constructor
    SensitivityScenarioData(bool parConversion = true)
        : computeGamma_(true), useSpreadedTermStructures_(false), parConversion_(parConversion) {};

    //! \name Inspectors
    //@{
    const map<string, QuantLib::ext::shared_ptr<CurveShiftData>>& discountCurveShiftData() const {
        return discountCurveShiftData_;
    }
    const map<string, QuantLib::ext::shared_ptr<CurveShiftData>>& indexCurveShiftData() const { return indexCurveShiftData_; }
    const map<string, QuantLib::ext::shared_ptr<CurveShiftData>>& yieldCurveShiftData() const { return yieldCurveShiftData_; }
    const map<string, SpotShiftData>& fxShiftData() const { return fxShiftData_; }
    const map<string, QuantLib::ext::shared_ptr<CapFloorVolShiftData>>& capFloorVolShiftData() const {
        return capFloorVolShiftData_;
    }
    const map<string, GenericYieldVolShiftData>& swaptionVolShiftData() const { return swaptionVolShiftData_; }
    const map<string, GenericYieldVolShiftData>& yieldVolShiftData() const { return yieldVolShiftData_; }
    const map<string, VolShiftData>& fxVolShiftData() const { return fxVolShiftData_; }
    const map<string, CdsVolShiftData>& cdsVolShiftData() const { return cdsVolShiftData_; }
    const map<string, BaseCorrelationShiftData>& baseCorrelationShiftData() const { return baseCorrelationShiftData_; }
    const map<string, QuantLib::ext::shared_ptr<CurveShiftData>>& zeroInflationCurveShiftData() const {
        return zeroInflationCurveShiftData_;
    }
    const map<string, QuantLib::ext::shared_ptr<CurveShiftData>>& yoyInflationCurveShiftData() const {
        return yoyInflationCurveShiftData_;
    }
    const map<string, QuantLib::ext::shared_ptr<CapFloorVolShiftData>>& yoyInflationCapFloorVolShiftData() const {
        return yoyInflationCapFloorVolShiftData_;
    }
    const map<string, QuantLib::ext::shared_ptr<CapFloorVolShiftData>>& zeroInflationCapFloorVolShiftData() const {
        return zeroInflationCapFloorVolShiftData_;
    }
    const map<string, string>& creditCcys() const { return creditCcys_; }
    const map<string, QuantLib::ext::shared_ptr<CurveShiftData>>& creditCurveShiftData() const { return creditCurveShiftData_; }
    const map<string, SpotShiftData>& equityShiftData() const { return equityShiftData_; }
    const map<string, VolShiftData>& equityVolShiftData() const { return equityVolShiftData_; }
    const map<string, QuantLib::ext::shared_ptr<CurveShiftData>>& dividendYieldShiftData() const {
        return dividendYieldShiftData_;
    }
    const map<string, string>& commodityCurrencies() const { return commodityCurrencies_; }
    const map<string, QuantLib::ext::shared_ptr<CurveShiftData>>& commodityCurveShiftData() const {
        return commodityCurveShiftData_;
    }
    const map<string, VolShiftData>& commodityVolShiftData() const { return commodityVolShiftData_; }
    const map<string, VolShiftData>& correlationShiftData() const { return correlationShiftData_; }
    const map<string, SpotShiftData>& securityShiftData() const { return securityShiftData_; }

    const vector<pair<string, string>>& crossGammaFilter() const { return crossGammaFilter_; }
    const bool computeGamma() const { return computeGamma_; }
    const bool useSpreadedTermStructures() const { return useSpreadedTermStructures_; }

    //! Give back the shift data for the given risk factor type, \p keyType, with the given \p name
    const ShiftData& shiftData(const ore::analytics::RiskFactorKey::KeyType& keyType, const std::string& name) const;
    //@}

    //! \name Setters
    //@{
    bool& parConversion() { return parConversion_; }

    map<string, QuantLib::ext::shared_ptr<CurveShiftData>>& discountCurveShiftData() { return discountCurveShiftData_; }
    map<string, QuantLib::ext::shared_ptr<CurveShiftData>>& indexCurveShiftData() { return indexCurveShiftData_; }
    map<string, QuantLib::ext::shared_ptr<CurveShiftData>>& yieldCurveShiftData() { return yieldCurveShiftData_; }
    map<string, SpotShiftData>& fxShiftData() { return fxShiftData_; }
    map<string, GenericYieldVolShiftData>& swaptionVolShiftData() { return swaptionVolShiftData_; }
    map<string, GenericYieldVolShiftData>& yieldVolShiftData() { return yieldVolShiftData_; }
    map<string, QuantLib::ext::shared_ptr<CapFloorVolShiftData>>& capFloorVolShiftData() { return capFloorVolShiftData_; }
    map<string, VolShiftData>& fxVolShiftData() { return fxVolShiftData_; }
    map<string, CdsVolShiftData>& cdsVolShiftData() { return cdsVolShiftData_; }
    map<string, BaseCorrelationShiftData>& baseCorrelationShiftData() { return baseCorrelationShiftData_; }
    map<string, QuantLib::ext::shared_ptr<CurveShiftData>>& zeroInflationCurveShiftData() {
        return zeroInflationCurveShiftData_;
    }
    map<string, string>& creditCcys() { return creditCcys_; }
    map<string, QuantLib::ext::shared_ptr<CurveShiftData>>& creditCurveShiftData() { return creditCurveShiftData_; }
    map<string, QuantLib::ext::shared_ptr<CurveShiftData>>& yoyInflationCurveShiftData() { return yoyInflationCurveShiftData_; }
    map<string, QuantLib::ext::shared_ptr<CapFloorVolShiftData>>& yoyInflationCapFloorVolShiftData() {
        return yoyInflationCapFloorVolShiftData_;
    }
    map<string, QuantLib::ext::shared_ptr<CapFloorVolShiftData>>& zeroInflationCapFloorVolShiftData() {
        return zeroInflationCapFloorVolShiftData_;
    }
    map<string, SpotShiftData>& equityShiftData() { return equityShiftData_; }
    map<string, QuantLib::ext::shared_ptr<CurveShiftData>>& dividendYieldShiftData() { return dividendYieldShiftData_; }
    map<string, VolShiftData>& equityVolShiftData() { return equityVolShiftData_; }
    map<string, string>& commodityCurrencies() { return commodityCurrencies_; }
    map<string, QuantLib::ext::shared_ptr<CurveShiftData>>& commodityCurveShiftData() { return commodityCurveShiftData_; }
    map<string, VolShiftData>& commodityVolShiftData() { return commodityVolShiftData_; }
    map<string, VolShiftData>& correlationShiftData() { return correlationShiftData_; }
    map<string, SpotShiftData>& securityShiftData() { return securityShiftData_; }

    vector<pair<string, string>>& crossGammaFilter() { return crossGammaFilter_; }
    bool& computeGamma() { return computeGamma_; }
    bool& useSpreadedTermStructures() { return useSpreadedTermStructures_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(ore::data::XMLDocument& doc) const override;
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
    void shiftDataFromXML(XMLNode* child, ShiftData& data);
    void curveShiftDataFromXML(XMLNode* child, CurveShiftData& data);
    void volShiftDataFromXML(XMLNode* child, VolShiftData& data, const bool requireShiftStrikes = true);

    //! toXML helper methods
    //@{
    void shiftDataToXML(ore::data::XMLDocument& doc, XMLNode* node, const ShiftData& data) const;
    void curveShiftDataToXML(ore::data::XMLDocument& doc, XMLNode* node, const CurveShiftData& data) const;
    void volShiftDataToXML(ore::data::XMLDocument& doc, XMLNode* node, const VolShiftData& data) const;
    //@}

    map<string, QuantLib::ext::shared_ptr<CurveShiftData>> discountCurveShiftData_;     // key: ccy
    map<string, QuantLib::ext::shared_ptr<CurveShiftData>> indexCurveShiftData_;        // key: indexName
    map<string, QuantLib::ext::shared_ptr<CurveShiftData>> yieldCurveShiftData_;        // key: yieldCurveName
    map<string, SpotShiftData> fxShiftData_;                                    // key: ccy pair
    map<string, QuantLib::ext::shared_ptr<CapFloorVolShiftData>> capFloorVolShiftData_; // key: ccy
    map<string, GenericYieldVolShiftData> swaptionVolShiftData_;                // key: ccy
    map<string, GenericYieldVolShiftData> yieldVolShiftData_;                   // key: securityId
    map<string, VolShiftData> fxVolShiftData_;                                  // key: ccy pair
    map<string, CdsVolShiftData> cdsVolShiftData_;                              // key: ccy pair
    map<string, BaseCorrelationShiftData> baseCorrelationShiftData_;
    map<string, QuantLib::ext::shared_ptr<CurveShiftData>> zeroInflationCurveShiftData_; // key: inflation index name
    map<string, QuantLib::ext::shared_ptr<CurveShiftData>> yoyInflationCurveShiftData_;  // key: yoy inflation index name
    map<string, QuantLib::ext::shared_ptr<CapFloorVolShiftData>> yoyInflationCapFloorVolShiftData_; // key: inflation index name
    map<string, QuantLib::ext::shared_ptr<CapFloorVolShiftData>>
        zeroInflationCapFloorVolShiftData_; // key: inflation index name
    map<string, string> creditCcys_;
    map<string, QuantLib::ext::shared_ptr<CurveShiftData>> creditCurveShiftData_;   // key: credit name
    map<string, SpotShiftData> equityShiftData_;                            // key: equity name
    map<string, VolShiftData> equityVolShiftData_;                          // key: equity name
    map<string, QuantLib::ext::shared_ptr<CurveShiftData>> dividendYieldShiftData_; // key: equity name
    map<string, std::string> commodityCurrencies_;
    map<string, QuantLib::ext::shared_ptr<CurveShiftData>> commodityCurveShiftData_;
    map<string, VolShiftData> correlationShiftData_;
    map<string, VolShiftData> commodityVolShiftData_;
    map<string, SpotShiftData> securityShiftData_; // key: security name

    vector<pair<string, string>> crossGammaFilter_;
    bool computeGamma_;
    bool useSpreadedTermStructures_;
    bool parConversion_;

private:
    void parDataFromXML(XMLNode* child, CurveShiftParData& data);

    //! toXML helper method
    XMLNode* parDataToXML(ore::data::XMLDocument& doc, const QuantLib::ext::shared_ptr<CurveShiftData>& csd) const;
};

// Utility to extract the set of keys that are used in a sensi config to qualiffy shift specs
std::set<std::string> getShiftSpecKeys(const SensitivityScenarioData& d);

} // namespace analytics
} // namespace ore
