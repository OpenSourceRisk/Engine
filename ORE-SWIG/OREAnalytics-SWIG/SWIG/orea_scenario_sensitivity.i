/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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

#ifndef orea_scenario_sensitivity_i
#define orea_scenario_sensitivity_i

%shared_ptr(ore::analytics::SensitivityScenarioData);
%shared_ptr(ore::analytics::SensitivityScenarioData::ShiftData);
%shared_ptr(ore::analytics::SensitivityScenarioData::CurveShiftData);
%shared_ptr(ore::analytics::SensitivityScenarioData::VolShiftData);
%shared_ptr(ore::analytics::SensitivityScenarioData::CdsVolShiftData);
%shared_ptr(ore::analytics::SensitivityScenarioData::BaseCorrelationShiftData);
%shared_ptr(ore::analytics::SensitivityScenarioData::SpotShiftData);
%shared_ptr(ore::analytics::SensitivityScenarioData::GenericYieldVolShiftData);
%shared_ptr(ore::analytics::SensitivityScenarioData::CapFloorVolShiftData);
%shared_ptr(ore::analytics::SensitivityScenarioData::CurveShiftParData);
%shared_ptr(ore::analytics::SensitivityScenarioData::CapFloorVolShiftParData);

%template(StringShiftTypeMap) std::map<std::string, QuantExt::ShiftType>;
%template(StringShiftSchemeMap) std::map<std::string, QuantExt::ShiftScheme>;
%template(StringRealMap) std::map<std::string, Real>;
%template(StringCurveShiftDataMap) std::map<std::string, ext::shared_ptr<ore::analytics::SensitivityScenarioData::CurveShiftData>>;
%template(StringSpotShiftDataMap) std::map<std::string, ext::shared_ptr<ore::analytics::SensitivityScenarioData::SpotShiftData>>;
%template(StringCapFloorVolShiftDataMap) std::map<std::string, ext::shared_ptr<ore::analytics::SensitivityScenarioData::CapFloorVolShiftData>>;
%template(StringGenericYieldVolShiftDataMap) std::map<std::string, ext::shared_ptr<ore::analytics::SensitivityScenarioData::GenericYieldVolShiftData>>;
%template(StringVolShiftDataMap) std::map<std::string, ext::shared_ptr<ore::analytics::SensitivityScenarioData::VolShiftData>>;
%template(StringCdsVolShiftDataMap) std::map<std::string, ext::shared_ptr<ore::analytics::SensitivityScenarioData::CdsVolShiftData>>;
%template(StringBaseCorrelationShiftDataMap) std::map<std::string, ext::shared_ptr<ore::analytics::SensitivityScenarioData::BaseCorrelationShiftData>>;

%rename (SensitivityScenarioDataShiftData) ore::analytics::SensitivityScenarioData::ShiftData;
%feature ("flatnested") ShiftData;

%rename (SensitivityScenarioDataCurveShiftData) ore::analytics::SensitivityScenarioData::CurveShiftData;
%feature ("flatnested") CurveShiftData;

%rename (SensitivityScenarioDataSpotShiftData) ore::analytics::SensitivityScenarioData::SpotShiftData;
%feature ("flatnested") SpotShiftData;

%rename (SensitivityScenarioDataCdsVolShiftData) ore::analytics::SensitivityScenarioData::CdsVolShiftData;
%feature ("flatnested") CdsVolShiftData;

%rename (SensitivityScenarioDataBaseCorrelationShiftData) ore::analytics::SensitivityScenarioData::BaseCorrelationShiftData;
%feature ("flatnested") BaseCorrelationShiftData;

%rename (SensitivityScenarioDataVolShiftData) ore::analytics::SensitivityScenarioData::VolShiftData;
%feature ("flatnested") VolShiftData;

%rename (SensitivityScenarioDataCapFloorVolShiftData) ore::analytics::SensitivityScenarioData::CapFloorVolShiftData;
%feature ("flatnested") CapFloorVolShiftData;

%rename (SensitivityScenarioDataGenericYieldVolShiftData) ore::analytics::SensitivityScenarioData::GenericYieldVolShiftData;
%feature ("flatnested") GenericYieldVolShiftData;

%rename (SensitivityScenarioDataCurveShiftParData) ore::analytics::SensitivityScenarioData::CurveShiftParData;
%feature ("flatnested") CurveShiftParData;

%rename (SensitivityScenarioDataCapFloorVolShiftParData) ore::analytics::SensitivityScenarioData::CapFloorVolShiftParData;
%feature ("flatnested") CapFloorVolShiftParData;

namespace ore {
namespace analytics {
class SensitivityScenarioData : public ore::data::XMLSerializable {
  public:

    struct ShiftData {
                QuantExt::ShiftType shiftType = QuantExt::ShiftType::Absolute;
        Real shiftSize = 0.0;
                QuantExt::ShiftScheme shiftScheme = QuantExt::ShiftScheme::Forward;

                std::map<std::string, QuantExt::ShiftType> keyedShiftType;
        std::map<std::string, Real> keyedShiftSize;
                std::map<std::string, QuantExt::ShiftScheme> keyedShiftScheme;
    };

    struct CurveShiftData : ShiftData {
        CurveShiftData() : ShiftData() {}
        CurveShiftData(const ShiftData& d) : ShiftData(d) {}
        std::vector<Period> shiftTenors;
    };

    using SpotShiftData = ShiftData;

    struct CdsVolShiftData : ShiftData {
        CdsVolShiftData() : ShiftData() {}
        CdsVolShiftData(const ShiftData& d) : ShiftData(d) {}
        std::string ccy;
        std::vector<Period> shiftExpiries;
    };

    struct BaseCorrelationShiftData : ShiftData {
        BaseCorrelationShiftData() : ShiftData() {}
        BaseCorrelationShiftData(const ShiftData& d) : ShiftData(d) {}
        std::vector<Period> shiftTerms;
        std::vector<Real> shiftLossLevels;
        std::string indexName;
    };

    struct VolShiftData : ShiftData {
        VolShiftData() : shiftStrikes({0.0}), isRelative(false) {}
        VolShiftData(const ShiftData& d) : ShiftData(d), shiftStrikes({0.0}), isRelative(false) {}
        std::vector<Period> shiftExpiries;
        std::vector<Real> shiftStrikes;
        bool isRelative;
    };

    struct CapFloorVolShiftData : VolShiftData {
        CapFloorVolShiftData() : VolShiftData() {}
        CapFloorVolShiftData(const VolShiftData& d) : VolShiftData(d) {}
        std::string indexName;
    };

    struct GenericYieldVolShiftData : VolShiftData {
        GenericYieldVolShiftData() : VolShiftData() {}
        GenericYieldVolShiftData(const VolShiftData& d) : VolShiftData(d) {}
        std::vector<Period> shiftTerms;
    };

    struct CurveShiftParData : CurveShiftData {
        CurveShiftParData() : CurveShiftData() {}
        CurveShiftParData(const CurveShiftData& c) : CurveShiftData(c) {}
        std::vector<std::string> parInstruments;
        bool parInstrumentSingleCurve = true;
        std::string discountCurve;
        std::string otherCurrency;
        std::map<std::string, std::string> parInstrumentConventions;
    };

    struct CapFloorVolShiftParData : CapFloorVolShiftData {
        CapFloorVolShiftParData() : CapFloorVolShiftData() {}
        CapFloorVolShiftParData(const CapFloorVolShiftData& c) : CapFloorVolShiftData(c) {}
        std::vector<std::string> parInstruments;
        bool parInstrumentSingleCurve = true;
        std::string discountCurve;
        std::map<std::string, std::string> parInstrumentConventions;
    };

    SensitivityScenarioData(bool parConversion = true);

    const std::map<std::string, ext::shared_ptr<SensitivityScenarioData::CurveShiftData>>& discountCurveShiftData() const;
    const std::map<std::string, ext::shared_ptr<SensitivityScenarioData::CurveShiftData>>& indexCurveShiftData() const;
    const std::map<std::string, ext::shared_ptr<SensitivityScenarioData::CurveShiftData>>& yieldCurveShiftData() const;
    const std::map<std::string, ext::shared_ptr<SensitivityScenarioData::SpotShiftData>>& fxShiftData() const;
    const std::map<std::string, ext::shared_ptr<SensitivityScenarioData::CapFloorVolShiftData>>& capFloorVolShiftData() const;
    const std::map<std::string, ext::shared_ptr<SensitivityScenarioData::GenericYieldVolShiftData>>& swaptionVolShiftData() const;
    const std::map<std::string, ext::shared_ptr<SensitivityScenarioData::GenericYieldVolShiftData>>& yieldVolShiftData() const;
    const std::map<std::string, ext::shared_ptr<SensitivityScenarioData::VolShiftData>>& fxVolShiftData() const;
    const std::map<std::string, ext::shared_ptr<SensitivityScenarioData::CdsVolShiftData>>& cdsVolShiftData() const;
    const std::map<std::string, ext::shared_ptr<SensitivityScenarioData::BaseCorrelationShiftData>>& baseCorrelationShiftData() const;
    const std::map<std::string, ext::shared_ptr<SensitivityScenarioData::CurveShiftData>>& zeroInflationCurveShiftData() const;
    const std::map<std::string, ext::shared_ptr<SensitivityScenarioData::CurveShiftData>>& yoyInflationCurveShiftData() const;
    const std::map<std::string, ext::shared_ptr<SensitivityScenarioData::CapFloorVolShiftData>>& yoyInflationCapFloorVolShiftData() const;
    const std::map<std::string, ext::shared_ptr<SensitivityScenarioData::CapFloorVolShiftData>>& zeroInflationCapFloorVolShiftData() const;
    const std::map<std::string, std::string>& creditCcys() const;
    const std::map<std::string, ext::shared_ptr<SensitivityScenarioData::CurveShiftData>>& creditCurveShiftData() const;
    const std::map<std::string, ext::shared_ptr<SensitivityScenarioData::SpotShiftData>>& equityShiftData() const;
    const std::map<std::string, ext::shared_ptr<SensitivityScenarioData::VolShiftData>>& equityVolShiftData() const;
    const std::map<std::string, ext::shared_ptr<SensitivityScenarioData::CurveShiftData>>& dividendYieldShiftData() const;
    const std::map<std::string, std::string>& commodityCurrencies() const;
    const std::map<std::string, ext::shared_ptr<SensitivityScenarioData::CurveShiftData>>& commodityCurveShiftData() const;
    const std::map<std::string, ext::shared_ptr<SensitivityScenarioData::VolShiftData>>& commodityVolShiftData() const;
    const std::map<std::string, ext::shared_ptr<SensitivityScenarioData::VolShiftData>>& correlationShiftData() const;
    const std::map<std::string, ext::shared_ptr<SensitivityScenarioData::SpotShiftData>>& securityShiftData() const;

    const std::vector<std::pair<std::string, std::string>>& crossGammaFilter() const;
    const bool computeGamma() const;
    const bool useSpreadedTermStructures() const;
    const ShiftData& shiftData(const ore::analytics::RiskFactorKey::KeyType& keyType, const std::string& name) const;
    bool& parConversion();

    std::map<std::string, ext::shared_ptr<SensitivityScenarioData::CurveShiftData>>& discountCurveShiftData();
    std::map<std::string, ext::shared_ptr<SensitivityScenarioData::CurveShiftData>>& indexCurveShiftData();
    std::map<std::string, ext::shared_ptr<SensitivityScenarioData::CurveShiftData>>& yieldCurveShiftData();
    std::map<std::string, ext::shared_ptr<SensitivityScenarioData::SpotShiftData>>& fxShiftData();
    std::map<std::string, ext::shared_ptr<SensitivityScenarioData::GenericYieldVolShiftData>>& swaptionVolShiftData();
    std::map<std::string, ext::shared_ptr<SensitivityScenarioData::GenericYieldVolShiftData>>& yieldVolShiftData();
    std::map<std::string, ext::shared_ptr<SensitivityScenarioData::CapFloorVolShiftData>>& capFloorVolShiftData();
    std::map<std::string, ext::shared_ptr<SensitivityScenarioData::VolShiftData>>& fxVolShiftData();
    std::map<std::string, ext::shared_ptr<SensitivityScenarioData::CdsVolShiftData>>& cdsVolShiftData();
    std::map<std::string, ext::shared_ptr<SensitivityScenarioData::BaseCorrelationShiftData>>& baseCorrelationShiftData();
    std::map<std::string, ext::shared_ptr<SensitivityScenarioData::CurveShiftData>>& zeroInflationCurveShiftData();
    std::map<std::string, std::string>& creditCcys();
    std::map<std::string, ext::shared_ptr<SensitivityScenarioData::CurveShiftData>>& creditCurveShiftData();
    std::map<std::string, ext::shared_ptr<SensitivityScenarioData::CurveShiftData>>& yoyInflationCurveShiftData();
    std::map<std::string, ext::shared_ptr<SensitivityScenarioData::CapFloorVolShiftData>>& yoyInflationCapFloorVolShiftData();
    std::map<std::string, ext::shared_ptr<SensitivityScenarioData::CapFloorVolShiftData>>& zeroInflationCapFloorVolShiftData();
    std::map<std::string, ext::shared_ptr<SensitivityScenarioData::SpotShiftData>>& equityShiftData();
    std::map<std::string, ext::shared_ptr<SensitivityScenarioData::CurveShiftData>>& dividendYieldShiftData();
    std::map<std::string, ext::shared_ptr<SensitivityScenarioData::VolShiftData>>& equityVolShiftData();
    std::map<std::string, std::string>& commodityCurrencies();
    std::map<std::string, ext::shared_ptr<SensitivityScenarioData::CurveShiftData>>& commodityCurveShiftData();
    std::map<std::string, ext::shared_ptr<SensitivityScenarioData::VolShiftData>>& commodityVolShiftData();
    std::map<std::string, ext::shared_ptr<SensitivityScenarioData::VolShiftData>>& correlationShiftData();
    std::map<std::string, ext::shared_ptr<SensitivityScenarioData::SpotShiftData>>& securityShiftData();

    void setParConversion(const bool b);
    void addDiscountCurveShiftData(const std::string& s, const ext::shared_ptr<CurveShiftData>& d);
    void addIndexCurveShiftData(const std::string& s, const ext::shared_ptr<CurveShiftData>& d);
    void addYieldCurveShiftData(const std::string& s, const ext::shared_ptr<CurveShiftData>& d);
    void addFxShiftData(const std::string& s, const ext::shared_ptr<SpotShiftData>& d);
    void addSwaptionVolShiftData(const std::string& s, const ext::shared_ptr<GenericYieldVolShiftData>& d);
    void addYieldVolShiftData(const std::string& s, const ext::shared_ptr<GenericYieldVolShiftData>& d);
    void addCapFloorVolShiftData(const std::string& s, const ext::shared_ptr<CapFloorVolShiftData>& d);
    void addFxVolShiftData(const std::string& s, const ext::shared_ptr<VolShiftData>& d);
    void addCdsVolShiftData(const std::string& s, const ext::shared_ptr<CdsVolShiftData>& d);
    void addBaseCorrelationShiftData(const std::string& s, ext::shared_ptr<BaseCorrelationShiftData>& d);
    void addZeroInflationCurveShiftData(const std::string& s, const ext::shared_ptr<CurveShiftData>& d);
    void addCreditCcys(const std::string& s, const std::string& d);
    void addCreditCurveShiftData(const std::string& s, const ext::shared_ptr<CurveShiftData>& d);
    void addYoyInflationCurveShiftData(const std::string& s, const ext::shared_ptr<CurveShiftData>& d);
    void addYoyInflationCapFloorVolShiftData(const std::string& s,
                                             const ext::shared_ptr<CapFloorVolShiftData>& d);
    void addZeroInflationCapFloorVolShiftData(const std::string& s,
                                              const ext::shared_ptr<CapFloorVolShiftData>& d);
    void addEquityShiftData(const std::string& s, const ext::shared_ptr<SpotShiftData>& d);
    void addDividendYieldShiftData(const std::string& s, const ext::shared_ptr<CurveShiftData>& d);
    void addEquityVolShiftData(const std::string& s, const ext::shared_ptr<VolShiftData>& d);
    void addCommodityCurrencies(const std::string& s, const string& d);
    void addCorrelationShiftData(const std::string& s, const ext::shared_ptr<VolShiftData>& d);
    void addCommodityCurveShiftData(const std::string& s, const ext::shared_ptr<CurveShiftData>& d);
    void addCommodityVolShiftData(const std::string& s, const ext::shared_ptr<VolShiftData>& d);
    void addSecurityShiftData(const string& s, const ext::shared_ptr<SpotShiftData>& d);
    void setCrossGammaFilter(const std::vector<std::pair<std::string, std::string>>& d);
    void setComputeGamma(const bool b);
    void setUseSpreadedTermStructures(const bool b);

    std::string getIndexCurrency(std::string indexName);
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

} // namespace analytics
} // namespace ore

#endif
