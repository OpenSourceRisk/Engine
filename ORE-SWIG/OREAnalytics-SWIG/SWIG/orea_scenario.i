/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

#ifndef orea_scenario_i
#define orea_scenario_i

%include ored_xmlutils.i

%{

using ore::analytics::parseShiftScheme;
using ore::analytics::parseShiftType;
using ore::analytics::ShiftScheme;
using ore::analytics::ShiftType;
using ore::analytics::parseShiftScheme;
using ore::analytics::parseShiftType;
using ore::analytics::StressTestScenarioData;
using ore::analytics::SensitivityScenarioData;
using ore::analytics::RiskFactorKey;
using QuantLib::Period;
using QuantLib::Real;
using ore::data::XMLSerializable;

%}

%template(PeriodVectorRealMap) std::map<Period, std::vector<Real>>;
%template(PeriodPairsRealMap) std::map<pair<Period, Period>, Real>;

enum class ShiftScheme { Forward, Backward, Central };
enum class ShiftType { Absolute, Relative };

ShiftScheme parseShiftScheme(const std::string& s);
ShiftType parseShiftType(const std::string& s);

%shared_ptr(StressTestScenarioData);
%shared_ptr(StressTestScenarioData::CurveShiftData);
%shared_ptr(StressTestScenarioData::SpotShiftData);
%shared_ptr(StressTestScenarioData::VolShiftData);
%shared_ptr(StressTestScenarioData::FXVolShiftData);
%shared_ptr(StressTestScenarioData::CapFloorVolShiftData);
%shared_ptr(StressTestScenarioData::SwaptionVolShiftData);
%template()std::pair<std::string, ext::shared_ptr<StressTestScenarioData::CurveShiftData>>;
%template(StressTestScenarioDataCurveShiftDataVector) std::vector<std::pair<std::string, ext::shared_ptr<StressTestScenarioData::CurveShiftData>>>;
%template()std::pair<std::string, ext::shared_ptr<StressTestScenarioData::SpotShiftData>>;
%template(StressTestScenarioDataSpotShiftDataVector) std::vector<std::pair<std::string, ext::shared_ptr<StressTestScenarioData::SpotShiftData>>>;
%template()std::pair<std::string, ext::shared_ptr<StressTestScenarioData::FXVolShiftData>>;
%template(StressTestScenarioDataFXVolShiftDataVector) std::vector<std::pair<std::string, ext::shared_ptr<StressTestScenarioData::FXVolShiftData>>>;
%template()std::pair<std::string, ext::shared_ptr<StressTestScenarioData::VolShiftData>>;
%template(StressTestScenarioDataVolShiftDataVector) std::vector<std::pair<std::string, ext::shared_ptr<StressTestScenarioData::VolShiftData>>>;
%template()std::pair<std::string, ext::shared_ptr<StressTestScenarioData::CapFloorVolShiftData>>;
%template(StressTestScenarioDataCapFloorVolShiftDataVector) std::vector<std::pair<std::string, ext::shared_ptr<StressTestScenarioData::CapFloorVolShiftData>>>;
%template()std::pair<std::string, ext::shared_ptr<StressTestScenarioData::SwaptionVolShiftData>>;
%template(StressTestScenarioDataSwaptionShiftDataVector) std::vector<std::pair<std::string, ext::shared_ptr<StressTestScenarioData::SwaptionVolShiftData>>>;
%template(StressTestDataVector) std::vector<StressTestScenarioData::StressTestData>;

%rename (StressTestScenarioDataStressTestData) StressTestScenarioData::StressTestData;
%feature ("flatnested") StressTestData;

%rename (StressTestScenarioDataCurveShiftData) StressTestScenarioData::CurveShiftData;
%feature ("flatnested") CurveShiftData;

%rename (StressTestScenarioDataSpotShiftData) StressTestScenarioData::SpotShiftData;
%feature ("flatnested") SpotShiftData;

%rename (StressTestScenarioDataFXVolShiftData) StressTestScenarioData::FXVolShiftData;
%feature ("flatnested") FXVolShiftData;

%rename (StressTestScenarioDataBaseSwaptionVolShiftData) StressTestScenarioData::SwaptionVolShiftData;
%feature ("flatnested") SwaptionVolShiftData;

%rename (StressTestScenarioDataVolShiftData) StressTestScenarioData::VolShiftData;
%feature ("flatnested") VolShiftData;

%rename (StressTestScenarioDataCapFloorVolShiftData) StressTestScenarioData::CapFloorVolShiftData;
%feature ("flatnested") CapFloorVolShiftData;

class StressTestScenarioData : public XMLSerializable {
  public:
    struct CurveShiftData {
        ~CurveShiftData() {}
    };
    struct SpotShiftData {
        ~SpotShiftData() {}
    };
    struct VolShiftData {
        ~VolShiftData() {}
    };
    struct FXVolShiftData {
        ~FXVolShiftData() {}
    };
    struct CapFloorVolShiftData {
        ~CapFloorVolShiftData() {}
    };
    struct SwaptionVolShiftData {
        ~SwaptionVolShiftData() {}
    };
    struct StressTestData {
        ~StressTestData() {}
        string label;
        bool irCurveParShifts;
        bool irCapFloorParShifts;
        bool creditCurveParShifts;

        bool containsParShifts() const { return irCurveParShifts || irCapFloorParShifts || creditCurveParShifts; };

        void setDiscountCurveShift(std::string s, const ext::shared_ptr<CurveShiftData>& csd);
        void setIndexCurveShift(std::string s, const ext::shared_ptr<CurveShiftData>& csd);
        void setYieldCurveShifts(std::string s, const ext::shared_ptr<CurveShiftData>& csd);
        void setFxShift(std::string s, const ext::shared_ptr<StressTestScenarioData::SpotShiftData>& csd);
        void setFxVolShift(std::string s, const ext::shared_ptr<StressTestScenarioData::FXVolShiftData>& csd);
        void setEquityShift(std::string s, const ext::shared_ptr<StressTestScenarioData::SpotShiftData>& csd);
        void setEquityVolShift(std::string s, const ext::shared_ptr<StressTestScenarioData::VolShiftData>& csd);
        void setCapVolShift(std::string s, const ext::shared_ptr<StressTestScenarioData::CapFloorVolShiftData>& csd);
        void setSwaptionVolShift(std::string s, const ext::shared_ptr<StressTestScenarioData::SwaptionVolShiftData>& csd);
        void setSecuritySpreadShift(std::string s, const ext::shared_ptr<StressTestScenarioData::SpotShiftData>& csd);
        void setRecoveryRateShift(std::string s, const ext::shared_ptr<StressTestScenarioData::SpotShiftData>& csd);
        void setSurvivalProbabilityShift(std::string s, const ext::shared_ptr<CurveShiftData>& csd);
        void setLabel(const std::string& l);

        std::vector<std::pair<std::string, ext::shared_ptr<CurveShiftData>>> getDiscountCurveShifts() const;
        std::vector<std::pair<std::string, ext::shared_ptr<CurveShiftData>>> getIndexCurveShifts() const;
        std::vector<std::pair<std::string, ext::shared_ptr<CurveShiftData>>> getYieldCurveShifts() const;
        std::vector<std::pair<std::string, ext::shared_ptr<SpotShiftData>>> getFxShifts() const;
        std::vector<std::pair<std::string, ext::shared_ptr<FXVolShiftData>>> getFxVolShifts() const;
        std::vector<std::pair<std::string, ext::shared_ptr<SpotShiftData>>> getEquityShifts() const;
        std::vector<std::pair<std::string, ext::shared_ptr<VolShiftData>>> getEquityVolShifts() const;
        std::vector<std::pair<std::string, ext::shared_ptr<CapFloorVolShiftData>>> getCapVolShifts() const;
        std::vector<std::pair<std::string, ext::shared_ptr<SwaptionVolShiftData>>> getSwaptionVolShifts() const;
        std::vector<std::pair<std::string, ext::shared_ptr<SpotShiftData>>> getSecuritySpreadShifts() const;
        std::vector<std::pair<std::string, ext::shared_ptr<SpotShiftData>>> getRecoveryRateShifts() const;
        std::vector<std::pair<std::string, ext::shared_ptr<CurveShiftData>>> getSurvivalProbabilityShifts() const;
        const std::string& getLabel() const;
    };

    StressTestScenarioData();

    const std::vector<StressTestData>& data() const;
    const bool useSpreadedTermStructures() const;    
    const bool hasScenarioWithParShifts() const;
    const bool withIrCurveParShifts() const;
    const bool withCreditCurveParShifts() const;
    const bool withIrCapFloorParShifts() const;

    void setData(const std::vector<StressTestData>& data);
    void setData(const StressTestData& data);
    void clearData();
    bool& useSpreadedTermStructures();
    const std::vector<StressTestData>& getData() const;
    
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;

};

%shared_ptr(SensitivityScenarioData);
%shared_ptr(SensitivityScenarioData::ShiftData);
%shared_ptr(SensitivityScenarioData::CurveShiftData);
%shared_ptr(SensitivityScenarioData::VolShiftData);
%shared_ptr(SensitivityScenarioData::CdsVolShiftData);
%shared_ptr(SensitivityScenarioData::BaseCorrelationShiftData);
%shared_ptr(SensitivityScenarioData::SpotShiftData);
%shared_ptr(SensitivityScenarioData::GenericYieldVolShiftData);
%shared_ptr(SensitivityScenarioData::CapFloorVolShiftData);
%shared_ptr(SensitivityScenarioData::CurveShiftParData);
%shared_ptr(SensitivityScenarioData::CapFloorVolShiftParData);

%template(StringShiftTypeMap) std::map<std::string, ShiftType>;
%template(StringShiftSchemeMap) std::map<std::string, ShiftScheme>;
%template(StringRealMap) std::map<std::string, Real>;
%template(StringCurveShiftDataMap) std::map<std::string, ext::shared_ptr<SensitivityScenarioData::CurveShiftData>>;
%template(StringSpotShiftDataMap) std::map<std::string, ext::shared_ptr<SensitivityScenarioData::SpotShiftData>>;
%template(StringCapFloorVolShiftDataMap) std::map<std::string, ext::shared_ptr<SensitivityScenarioData::CapFloorVolShiftData>>;
%template(StringGenericYieldVolShiftDataMap) std::map<std::string, ext::shared_ptr<SensitivityScenarioData::GenericYieldVolShiftData>>;
%template(StringVolShiftDataMap) std::map<std::string, ext::shared_ptr<SensitivityScenarioData::VolShiftData>>;
%template(StringCdsVolShiftDataMap) std::map<std::string, ext::shared_ptr<SensitivityScenarioData::CdsVolShiftData>>;
%template(StringBaseCorrelationShiftDataMap) std::map<std::string, ext::shared_ptr<SensitivityScenarioData::BaseCorrelationShiftData>>;
%template(VectorOfStringStringPair) std::vector<std::pair<std::string, std::string>>;

%rename (SensitivityScenarioDataShiftData) SensitivityScenarioData::ShiftData;
%feature ("flatnested") ShiftData;

%rename (SensitivityScenarioDataCurveShiftData) SensitivityScenarioData::CurveShiftData;
%feature ("flatnested") CurveShiftData;

%rename (SensitivityScenarioDataSpotShiftData) SensitivityScenarioData::SpotShiftData;
%feature ("flatnested") SpotShiftData;

%rename (SensitivityScenarioDataCdsVolShiftData) SensitivityScenarioData::CdsVolShiftData;
%feature ("flatnested") CdsVolShiftData;

%rename (SensitivityScenarioDataBaseCorrelationShiftData) SensitivityScenarioData::BaseCorrelationShiftData;
%feature ("flatnested") BaseCorrelationShiftData;

%rename (SensitivityScenarioDataVolShiftData) SensitivityScenarioData::VolShiftData;
%feature ("flatnested") VolShiftData;

%rename (SensitivityScenarioDataCapFloorVolShiftData) SensitivityScenarioData::CapFloorVolShiftData;
%feature ("flatnested") CapFloorVolShiftData;

%rename (SensitivityScenarioDataGenericYieldVolShiftData) SensitivityScenarioData::GenericYieldVolShiftData;
%feature ("flatnested") GenericYieldVolShiftData;

%rename (SensitivityScenarioDataCurveShiftParData) SensitivityScenarioData::CurveShiftParData;
%feature ("flatnested") CurveShiftParData;

%rename (SensitivityScenarioDataCapFloorVolShiftParData) SensitivityScenarioData::CapFloorVolShiftParData;
%feature ("flatnested") CapFloorVolShiftParData;

class SensitivityScenarioData : public XMLSerializable {
  public:

    struct ShiftData {
        ShiftType shiftType = ShiftType::Absolute;
        Real shiftSize = 0.0;
        ShiftScheme shiftScheme = ShiftScheme::Forward;

        std::map<std::string, ShiftType> keyedShiftType;
        std::map<std::string, Real> keyedShiftSize;
        std::map<std::string, ShiftScheme> keyedShiftScheme;
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

#endif