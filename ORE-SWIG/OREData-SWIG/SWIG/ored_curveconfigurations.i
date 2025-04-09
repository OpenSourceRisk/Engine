/*
 Copyright (C) 2018, 2020 Quaternion Risk Management Ltd
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

#ifndef ored_curveconfigurations_i
#define ored_curveconfigurations_i

%include std_set.i
%include ored_curvespec.i
%include ored_xmlutils.i

%{
using ore::data::CurveConfigurations;
using ore::data::CurveConfigurationsManager;
using ore::data::VolatilityConfig;
using ore::data::OneDimSolverConfig;
using ore::data::ReportConfig;
using ore::data::YieldCurveConfig;
using ore::data::FXVolatilityCurveConfig;
using ore::data::GenericYieldVolatilityCurveConfig;
using ore::data::SwaptionVolatilityCurveConfig;
using ore::data::YieldVolatilityCurveConfig;
using ore::data::CapFloorVolatilityCurveConfig;
using ore::data::DefaultCurveConfig;
using ore::data::CDSVolatilityCurveConfig;
using ore::data::BaseCorrelationCurveConfig;
using ore::data::InflationCurveConfig;
using ore::data::InflationCapFloorVolatilityCurveConfig;
using ore::data::EquityCurveConfig;
using ore::data::EquityVolatilityCurveConfig;
using ore::data::SecurityConfig;
using ore::data::FXSpotConfig;
using ore::data::CommodityCurveConfig;
using ore::data::CommodityVolatilityConfig;
using ore::data::CorrelationCurveConfig;
using ore::data::TodaysMarketParameters;
using ore::data::CurveSpec;
using ore::data::CurveConfig;
using ore::data::XMLSerializable;
using ore::data::BootstrapConfig;
using ore::data::PriceSegment;
using ore::data::YieldCurveSegment;
using ore::data::ParametricSmileConfiguration;

%}

%template(CurveTypeStringSet) std::map<CurveSpec::CurveType, std::set<std::string>>;

%shared_ptr(CurveConfigurations)
class CurveConfigurations  : public XMLSerializable  {
  public:
    CurveConfigurations();

    const ReportConfig& reportConfigEqVols() const;
    const ReportConfig& reportConfigFxVols() const;
    const ReportConfig& reportConfigCommVols() const;
    const ReportConfig& reportConfigIrCapFloorVols();
    const ReportConfig& reportConfigIrSwaptionVols();

    bool hasYieldCurveConfig(const std::string& curveID) const;
    ext::shared_ptr<YieldCurveConfig> yieldCurveConfig(const std::string& curveID) const;

    bool hasFxVolCurveConfig(const std::string& curveID) const;
    ext::shared_ptr<FXVolatilityCurveConfig> fxVolCurveConfig(const std::string& curveID) const;

    bool hasSwaptionVolCurveConfig(const std::string& curveID) const;
    ext::shared_ptr<SwaptionVolatilityCurveConfig> swaptionVolCurveConfig(const std::string& curveID) const;

    bool hasYieldVolCurveConfig(const std::string& curveID) const;
    ext::shared_ptr<YieldVolatilityCurveConfig> yieldVolCurveConfig(const std::string& curveID) const;

    bool hasCapFloorVolCurveConfig(const std::string& curveID) const;
    ext::shared_ptr<CapFloorVolatilityCurveConfig> capFloorVolCurveConfig(const std::string& curveID) const;

    bool hasDefaultCurveConfig(const std::string& curveID) const;
    ext::shared_ptr<DefaultCurveConfig> defaultCurveConfig(const std::string& curveID) const;

    bool hasCdsVolCurveConfig(const std::string& curveID) const;
    ext::shared_ptr<CDSVolatilityCurveConfig> cdsVolCurveConfig(const std::string& curveID) const;

    bool hasBaseCorrelationCurveConfig(const std::string& curveID) const;
    ext::shared_ptr<BaseCorrelationCurveConfig> baseCorrelationCurveConfig(const std::string& curveID) const;

    bool hasInflationCurveConfig(const std::string& curveID) const;
    ext::shared_ptr<InflationCurveConfig> inflationCurveConfig(const std::string& curveID) const;

    bool hasInflationCapFloorVolCurveConfig(const std::string& curveID) const;
    ext::shared_ptr<InflationCapFloorVolatilityCurveConfig> inflationCapFloorVolCurveConfig(const std::string& curveID) const;

    bool hasEquityCurveConfig(const std::string& curveID) const;
    ext::shared_ptr<EquityCurveConfig> equityCurveConfig(const std::string& curveID) const;

    bool hasEquityVolCurveConfig(const std::string& curveID) const;
    ext::shared_ptr<EquityVolatilityCurveConfig> equityVolCurveConfig(const std::string& curveID) const;

    bool hasSecurityConfig(const std::string& curveID) const;
    ext::shared_ptr<SecurityConfig> securityConfig(const std::string& curveID) const;

    bool hasFxSpotConfig(const std::string& curveID) const;
    ext::shared_ptr<FXSpotConfig> fxSpotConfig(const std::string& curveID) const;

    bool hasCommodityCurveConfig(const std::string& curveID) const;
    ext::shared_ptr<CommodityCurveConfig> commodityCurveConfig(const std::string& curveID) const;

    bool hasCommodityVolatilityConfig(const std::string& curveID) const;
    ext::shared_ptr<CommodityVolatilityConfig> commodityVolatilityConfig(const std::string& curveID) const;

    bool hasCorrelationCurveConfig(const std::string& curveID) const;
    ext::shared_ptr<CorrelationCurveConfig> correlationCurveConfig(const std::string& curveID) const;

    ext::shared_ptr<CurveConfigurations> minimalCurveConfig(const ext::shared_ptr<TodaysMarketParameters> todaysMarketParams,
                       const std::set<std::string>& configurations = {""}) const;

    std::set<std::string> quotes(const ext::shared_ptr<TodaysMarketParameters> todaysMarketParams,
                            const std::set<std::string>& configurations = {""}) const;
    std::set<std::string> quotes() const;

    std::set<std::string> conventions(const ext::shared_ptr<TodaysMarketParameters> todaysMarketParams,
                                 const std::set<std::string>& configurations = {""}) const;
    std::set<std::string> conventions() const;

    std::set<std::string> yieldCurveConfigIds();

    std::map<CurveSpec::CurveType, std::set<std::string>> requiredCurveIds(const CurveSpec::CurveType& type,
                                                                      const std::string& curveId) const;

    void add(const CurveSpec::CurveType& type, const std::string& curveId, const ext::shared_ptr<CurveConfig>& config);    
    bool has(const CurveSpec::CurveType& type, const std::string& curveId) const;
    const ext::shared_ptr<CurveConfig>& get(const CurveSpec::CurveType& type, const std::string& curveId) const;
    void parseAll();

    void addAdditionalCurveConfigs(const CurveConfigurations& c);
    
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;

};

%template(StringCurveConfigMap) std::map<std::string, ext::shared_ptr<CurveConfigurations>>;
%template(VolatilityConfigVector) std::vector<ext::shared_ptr<VolatilityConfig>>;

%shared_ptr(CurveConfigurationsManager)
class CurveConfigurationsManager {
  public:
    CurveConfigurationsManager();

    void add(const ext::shared_ptr<CurveConfigurations>& config, std::string id = std::string());
    const ext::shared_ptr<CurveConfigurations>& get(std::string id = std::string()) const;
    const bool has(std::string id = std::string()) const;
    const std::map<std::string, ext::shared_ptr<CurveConfigurations>>& curveConfigurations() const;
    const bool empty() const;

};

%shared_ptr(VolatilityConfig)
class VolatilityConfig : public XMLSerializable {
public:
    VolatilityConfig(std::string calendarStr = std::string(), QuantLib::Natural priority = 0);

    void fromXMLNode(ore::data::XMLNode* node);
    void toXMLNode(XMLDocument& doc, XMLNode* node) const;
    QuantLib::Natural priority() const;
    QuantLib::Calendar calendar() const;
};

%shared_ptr(OneDimSolverConfig)
class OneDimSolverConfig : public XMLSerializable {
public:
    OneDimSolverConfig();
    OneDimSolverConfig(QuantLib::Size maxEvaluations,
        QuantLib::Real initialGuess,
        QuantLib::Real accuracy,
        const std::pair<QuantLib::Real, QuantLib::Real>& minMax,
        QuantLib::Real lowerBound = QuantLib::Null<QuantLib::Real>(),
        QuantLib::Real upperBound = QuantLib::Null<QuantLib::Real>());
    OneDimSolverConfig(QuantLib::Size maxEvaluations,
        QuantLib::Real initialGuess,
        QuantLib::Real accuracy,
        QuantLib::Real step,
        QuantLib::Real lowerBound = QuantLib::Null<QuantLib::Real>(),
        QuantLib::Real upperBound = QuantLib::Null<QuantLib::Real>());

    void fromXML(ore::data::XMLNode* node);
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const;

    QuantLib::Size maxEvaluations() const;
    QuantLib::Real initialGuess() const;
    QuantLib::Real accuracy() const;
    const std::pair<QuantLib::Real, QuantLib::Real>& minMax() const;
    QuantLib::Real step() const;
    QuantLib::Real lowerBound() const;
    QuantLib::Real upperBound() const;

    bool empty() const;

};

%shared_ptr(CurveConfig)
class CurveConfig : public XMLSerializable {
public:
    CurveConfig(const std::string& curveID, const std::string& curveDescription, const std::vector<std::string>& quotes = std::vector<std::string>());
    CurveConfig();

    const std::string& curveID() const;
    const std::string& curveDescription() const;
    const std::set<std::string>& requiredCurveIds(const CurveSpec::CurveType& curveType) const;
    const std::map<CurveSpec::CurveType, std::set<std::string>>& requiredCurveIds() const;
    std::string& curveID();
    std::string& curveDescription();
    std::set<std::string>& requiredCurveIds(const CurveSpec::CurveType& curveType);
    std::map<CurveSpec::CurveType, std::set<std::string>>& requiredCurveIds();
    virtual const std::vector<std::string>& quotes();
};

%shared_ptr(EquityCurveConfig)
class EquityCurveConfig : public CurveConfig {
public:
    enum class Type { DividendYield, ForwardPrice, OptionPremium, NoDividends, ForwardDividendPrice};
    EquityCurveConfig(const std::string& curveID, const std::string& curveDescription, const std::string& forecastingCurve,
                      const std::string& currency, const std::string& calendar, const Type& type, const std::string& equitySpotQuote,
                      const std::vector<std::string>& quotes, const std::string& dayCountID = "",
                      const std::string& dividendInterpVariable = "Zero", const std::string& dividendInterpMethod = "Linear",
                      const bool dividendExtrapolation = false, const bool extrapolation = false,
                      const QuantLib::Exercise::Type& exerciseStyle = QuantLib::Exercise::Type::European);
    EquityCurveConfig() ;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;

    const std::string& forecastingCurve() const;
    const std::string& currency() const;
    const std::string& calendar() const;
    const Type& type() const;
    const std::string& equitySpotQuoteID() const;
    const std::string& dayCountID() const;
    const std::string& dividendInterpolationVariable() const;
    const std::string& dividendInterpolationMethod() const;
    bool dividendExtrapolation() const;
    bool extrapolation() const;
    const QuantLib::Exercise::Type exerciseStyle() const;
    const std::vector<std::string>& fwdQuotes();
};

%shared_ptr(CommodityCurveConfig)
class CommodityCurveConfig : public CurveConfig {
public:
    enum class Type { Direct, CrossCurrency, Basis, Piecewise };

    CommodityCurveConfig();

    CommodityCurveConfig(const std::string& curveId, const std::string& curveDescription, const std::string& currency,
                         const std::vector<std::string>& quotes, const std::string& commoditySpotQuote = "",
                         const std::string& dayCountId = "A365", const std::string& interpolationMethod = "Linear",
                         bool extrapolation = true, const std::string& conventionsId = "");

    CommodityCurveConfig(const std::string& curveId, const std::string& curveDescription, const std::string& currency,
                         const std::string& basePriceCurveId, const std::string& baseYieldCurveId,
                         const std::string& yieldCurveId, bool extrapolation = true);

    CommodityCurveConfig(const std::string& curveId, const std::string& curveDescription, const std::string& currency,
                         const std::string& basePriceCurveId, const std::string& baseConventionsId,
                         const std::vector<std::string>& basisQuotes, const std::string& basisConventionsId,
                         const std::string& dayCountId = "A365", const std::string& interpolationMethod = "Linear",
                         bool extrapolation = true, bool addBasis = true, QuantLib::Natural monthOffset = 0,
                         bool averageBase = true);

    CommodityCurveConfig(const std::string& curveId, const std::string& curveDescription, const std::string& currency,
                         const std::vector<PriceSegment>& priceSegments, const std::string& dayCountId = "A365",
                         const std::string& interpolationMethod = "Linear", bool extrapolation = true,
                         const boost::optional<BootstrapConfig>& bootstrapConfig = boost::none);
    
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;

    const Type& type() const;
    const std::string& currency() const;
    const std::string& commoditySpotQuoteId() const;
    const std::string& dayCountId() const;
    const std::string& interpolationMethod() const;
    const std::string& basePriceCurveId() const;
    const std::string& baseYieldCurveId() const;
    const std::string& yieldCurveId() const;
    bool extrapolation() const;
    const vector<string>& fwdQuotes() const;
    const std::string& conventionsId() const;
    const std::string& baseConventionsId() const;
    bool addBasis() const;
    QuantLib::Natural monthOffset() const;
    bool averageBase() const;
    bool priceAsHistFixing() const;
    const std::map<unsigned short, PriceSegment>& priceSegments() const;
    const boost::optional<BootstrapConfig>& bootstrapConfig() const;

    Type& type();
    std::string& currency();
    std::string& commoditySpotQuoteId();
    std::string& dayCountId();
    std::string& interpolationMethod();
    std::string& basePriceCurveId();
    std::string& baseYieldCurveId();
    std::string& yieldCurveId();
    bool& extrapolation();
    std::string& conventionsId();
    std::string& baseConventionsId();
    bool& addBasis();
    QuantLib::Natural& monthOffset();
    bool& averageBase();
    bool& priceAsHistFixing();
    void setPriceSegments(const std::map<unsigned short, PriceSegment>& priceSegments);
    void setBootstrapConfig(const BootstrapConfig& bootstrapConfig);
};

%shared_ptr(DefaultCurveConfig)
class DefaultCurveConfig : public CurveConfig {
public:
    class Config;
    DefaultCurveConfig(const std::string& curveId, const std::string& curveDescription, const std::string& currency,
                       const std::map<int, Config>& configs);
    DefaultCurveConfig(const std::string& curveID, const std::string& curveDescription, const std::string& currency,
                       const Config& config);
    DefaultCurveConfig();

    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;

    const std::string& currency() const;
    const std::map<int, Config>& configs() const;
};

%shared_ptr(YieldCurveConfig)
class YieldCurveConfig : public CurveConfig {
public:
    YieldCurveConfig();
    YieldCurveConfig(const std::string& curveID, const std::string& curveDescription, const std::string& currency,
                     const std::string& discountCurveID, const std::vector<ext::shared_ptr<YieldCurveSegment>>& curveSegments,
                     const std::string& interpolationVariable = "Discount", const std::string& interpolationMethod = "LogLinear",
                     const std::string& zeroDayCounter = "A365", bool extrapolation = true,
                     const BootstrapConfig& bootstrapConfig = BootstrapConfig(),
                     const QuantLib::Size mixedInterpolationCutoff = 1);
    virtual ~YieldCurveConfig();

    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;

    const std::string& currency() const;
    const std::string& discountCurveID() const;
    const std::vector<ext::shared_ptr<YieldCurveSegment>>& curveSegments() const;
    const std::string& interpolationVariable() const;
    const std::string& interpolationMethod() const;
    QuantLib::Size mixedInterpolationCutoff() const;
    const std::string& zeroDayCounter() const;
    bool extrapolation() const;
    const BootstrapConfig& bootstrapConfig() const;

    std::string& interpolationVariable();
    std::string& interpolationMethod();
    QuantLib::Size& mixedInterpolationCutoff();
    std::string& zeroDayCounter();
    bool& extrapolation();
    void setBootstrapConfig(const BootstrapConfig& bootstrapConfig);

    const std::vector<std::string>& quotes();
};


%shared_ptr(GenericYieldVolatilityCurveConfig)
class GenericYieldVolatilityCurveConfig : public CurveConfig {
public:
    enum class Dimension { ATM, Smile };
    enum class VolatilityType { Lognormal, Normal, ShiftedLognormal };
    enum class Interpolation {
        Hagan2002Lognormal = 0,
        Hagan2002Normal = 1,
        Hagan2002NormalZeroBeta = 2,
        Antonov2015FreeBoundaryNormal =3,
        KienitzLawsonSwaynePde=4,
        FlochKennedy=5,
        Linear = 6
    };
    enum class Extrapolation {
        None, Flat, Linear
    };
    GenericYieldVolatilityCurveConfig(const std::string& underlyingLabel, const std::string& rootNodeLabel,
                                      const std::string& marketDatumInstrumentLabel, const std::string& qualifierLabel,
                                      const bool allowSmile,
                                      const bool requireSwapIndexBases);
    GenericYieldVolatilityCurveConfig(
        const std::string& underlyingLabel, const std::string& rootNodeLabel,
        const std::string& marketDatumInstrumentLabel, const std::string& qualifierLabel, const std::string& curveID,
        const std::string& curveDescription, const std::string& qualifier, const Dimension dimension,
        const VolatilityType volatilityType, const VolatilityType outputVolatilityType,
        const Interpolation interpolation, const Extrapolation extrapolation, const std::vector<std::string>& optionTenors,
        const std::vector<std::string>& underlyingTenors, const QuantLib::DayCounter& dayCounter, const QuantLib::Calendar& calendar,
        const QuantLib::BusinessDayConvention& businessDayConvention, const std::string& shortSwapIndexBase = "",
        const std::string& swapIndexBase = "",
        // Only required for smile
        const std::vector<std::string>& smileOptionTenors = std::vector<std::string>(),
        const std::vector<std::string>& smileUnderlyingTenors = std::vector<std::string>(),
        const std::vector<std::string>& smileSpreads = std::vector<std::string>(),
        const boost::optional<ParametricSmileConfiguration>& parametricSmileConfiguration = boost::none);
    GenericYieldVolatilityCurveConfig(const std::string& underlyingLabel, const std::string& rootNodeLabel,
                                      const std::string& qualifierLabel, const std::string& curveID,
                                      const std::string& curveDescription, const std::string& qualifier,
                                      const std::string& proxySourceCurveId,
                                      const std::string& proxySourceShortSwapIndexBase,
                                      const std::string& proxySourceSwapIndexBase,
                                      const std::string& proxyTargetShortSwapIndexBase,
                                      const std::string& proxyTargetSwapIndexBase);

    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;

    const std::string& qualifier() const;
    Dimension dimension() const;
    VolatilityType volatilityType() const;
    VolatilityType outputVolatilityType() const;
    const std::vector<QuantLib::Real>& modelShift() const;
    const std::vector<QuantLib::Real>& outputShift() const;
    Interpolation interpolation() const;
    Extrapolation extrapolation() const;
    const std::vector<std::string>& optionTenors() const;
    const std::vector<std::string>& underlyingTenors() const;
    const QuantLib::DayCounter& dayCounter() const;
    const QuantLib::Calendar& calendar() const;
    const QuantLib::BusinessDayConvention& businessDayConvention() const;
    const std::string& shortSwapIndexBase() const;
    const std::string& swapIndexBase() const;
    const std::vector<std::string>& smileOptionTenors() const;
    const std::vector<std::string>& smileUnderlyingTenors() const;
    const std::vector<std::string>& smileSpreads() const;
    const std::string& quoteTag() const;
    const std::vector<std::string>& quotes();
    
    const std::string& proxySourceCurveId() const;
    const std::string& proxySourceShortSwapIndexBase() const;
    const std::string& proxySourceSwapIndexBase() const;
    const std::string& proxyTargetShortSwapIndexBase() const;
    const std::string& proxyTargetSwapIndexBase() const;
    
    const boost::optional<ParametricSmileConfiguration> parametricSmileConfiguration() const;
    const ReportConfig& reportConfig() const;

};

%shared_ptr(SwaptionVolatilityCurveConfig)
class SwaptionVolatilityCurveConfig : public GenericYieldVolatilityCurveConfig {
public:
    SwaptionVolatilityCurveConfig();
    SwaptionVolatilityCurveConfig(const std::string& curveID, const std::string& curveDescription, const GenericYieldVolatilityCurveConfig::Dimension dimension,
                                  const GenericYieldVolatilityCurveConfig::VolatilityType volatilityType, const GenericYieldVolatilityCurveConfig::VolatilityType outputVolatilityType,
                                  const GenericYieldVolatilityCurveConfig::Interpolation interpolation, const GenericYieldVolatilityCurveConfig::Extrapolation extrapolation,
                                  const std::vector<std::string>& optionTenors, const std::vector<std::string>& swapTenors,
                                  const QuantLib::DayCounter& dayCounter, const QuantLib::Calendar& calendar,
                                  const QuantLib::BusinessDayConvention& businessDayConvention, const std::string& shortSwapIndexBase,
                                  const std::string& swapIndexBase,
                                  // Only required for smile
                                  const std::vector<std::string>& smileOptionTenors = std::vector<std::string>(),
                                  const std::vector<std::string>& smileSwapTenors = std::vector<std::string>(),
                                  const std::vector<std::string>& smileSpreads = std::vector<std::string>());
    SwaptionVolatilityCurveConfig(const std::string& curveID, const std::string& curveDescription,
                                  const std::string& proxySourceCurveId, const std::string& proxySourceShortSwapIndexBase,
                                  const std::string& proxySourceSwapIndexBase, const std::string& proxyTargetShortSwapIndexBase,
                                  const std::string& proxyTargetSwapIndexBase);
};

%shared_ptr(FXVolatilityCurveConfig)
class FXVolatilityCurveConfig : public CurveConfig {
public:
    enum class Dimension { ATM, SmileVannaVolga, SmileDelta, SmileBFRR, SmileAbsolute, ATMTriangulated };
    enum class SmileInterpolation { VannaVolga1, VannaVolga2, Linear, Cubic };
    enum class TimeInterpolation { V, V2T };
    FXVolatilityCurveConfig() {}
    FXVolatilityCurveConfig(const std::string& curveID, const std::string& curveDescription, const FXVolatilityCurveConfig::Dimension& dimension,
                            const std::vector<std::string>& expiries, const std::vector<std::string>& deltas = std::vector<std::string>(),
                            const std::string& fxSpotID = "", const std::string& fxForeignCurveID = "",
                            const std::string& fxDomesticCurveID = "",
                            const QuantLib::DayCounter& dayCounter = QuantLib::Actual365Fixed(),
                            const QuantLib::Calendar& calendar = QuantLib::TARGET(),
                            const FXVolatilityCurveConfig::SmileInterpolation& interp = FXVolatilityCurveConfig::SmileInterpolation::VannaVolga2,
                            const std::string& conventionsID = "", const std::vector<QuantLib::Size>& smileDelta = {25},
                            const std::string& smileExtrapolation = "Flat");

    FXVolatilityCurveConfig(const std::string& curveID, const std::string& curveDescription, const FXVolatilityCurveConfig::Dimension& dimension,
                            const std::string& baseVolatility1, const std::string& baseVolatility2,
                            const std::string& fxIndexTag = "GENERIC");


    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;

    const Dimension& dimension() const;
    const std::vector<std::string>& expiries() const;
    const vector<string>& deltas() const;
    const QuantLib::DayCounter& dayCounter() const;
    const QuantLib::Calendar& calendar() const;
    const std::string& fxSpotID() const;
    const std::string& fxForeignYieldCurveID() const;
    const std::string& fxDomesticYieldCurveID() const;
    const FXVolatilityCurveConfig::SmileInterpolation& smileInterpolation() const;
    const std::string& smileExtrapolation() const;
    const FXVolatilityCurveConfig::TimeInterpolation& timeInterpolation() const;
    const std::string& timeWeighting() const;
    const std::string& conventionsID() const;
    const std::vector<QuantLib::Size>& smileDelta() const;
    const std::vector<std::string>& quotes();
    const std::string& baseVolatility1() const;
    const std::string& baseVolatility2() const;
    const std::string& fxIndexTag() const;
    double butterflyErrorTolerance() const;

};

%shared_ptr(CapFloorVolatilityCurveConfig)
class CapFloorVolatilityCurveConfig : public CurveConfig {
public:
    enum class VolatilityType { Lognormal, Normal, ShiftedLognormal };
    enum class Type { TermAtm, TermSurface, TermSurfaceWithAtm, OptionletAtm, OptionletSurface, OptionletSurfaceWithAtm };
    CapFloorVolatilityCurveConfig() {}
    CapFloorVolatilityCurveConfig(
        const std::string& curveID, const std::string& curveDescription, const VolatilityType& volatilityType,
        bool extrapolate, bool flatExtrapolation, bool inlcudeAtm, const std::vector<std::string>& tenors,
        const std::vector<std::string>& strikes, const QuantLib::DayCounter& dayCounter, QuantLib::Natural settleDays,
        const QuantLib::Calendar& calendar, const QuantLib::BusinessDayConvention& businessDayConvention,
        const std::string& index, const QuantLib::Period& rateComputationPeriod,
        const QuantLib::Size onCapSettlementDays, const std::string& discountCurve,
        const std::string& interpolationMethod = "BicubicSpline", const std::string& interpolateOn = "TermVolatilities",
        const std::string& timeInterpolation = "LinearFlat", const std::string& strikeInterpolation = "LinearFlat",
        const std::vector<std::string>& atmTenors = {}, const BootstrapConfig& bootstrapConfig = BootstrapConfig(),
        const string& inputType = "TermVolatilities",
        const boost::optional<ParametricSmileConfiguration>& parametricSmileConfiguration = boost::none);
    CapFloorVolatilityCurveConfig(const std::string& curveID, const std::string& curveDescription,
                                  const std::string& proxySourceCurveId, const std::string& proxySourceIndex,
                                  const std::string& proxyTargetIndex,
                                  const QuantLib::Period& proxySourceRateComputationPeriod = 0 * Days,
                                  const QuantLib::Period& proxyTargetRateComputationPeriod = 0 * Days);

    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;

    const VolatilityType& volatilityType() const;
    const VolatilityType& outputVolatilityType() const;
    QuantLib::Real modelShift() const;
    QuantLib::Real outputShift() const;
    MarketDatum::QuoteType quoteType() const;
    bool extrapolate() const;
    bool flatExtrapolation() const;
    bool includeAtm() const;
    const std::vector<std::string>& tenors() const;
    const std::vector<std::string>& strikes() const;
    bool optionalQuotes() const;
    const QuantLib::DayCounter& dayCounter() const;
    const QuantLib::Natural& settleDays() const;
    const QuantLib::Calendar& calendar() const;
    const QuantLib::BusinessDayConvention& businessDayConvention() const;
    const std::string& index() const;
    const QuantLib::Period& rateComputationPeriod() const;
    QuantLib::Size onCapSettlementDays() const;
    const std::string& discountCurve() const;
    QuantExt::CapFloorTermVolSurfaceExact::InterpolationMethod interpolationMethod() const;
    const std::string& interpolateOn() const;
    const std::string& timeInterpolation() const;
    const std::string& strikeInterpolation() const;
    bool quoteIncludesIndexName() const;
    const std::vector<std::string>& atmTenors() const;
    const BootstrapConfig& bootstrapConfig() const;
    Type type() const;
    const std::string& currency() const;
    std::string indexTenor() const;

    const std::string& proxySourceCurveId() const;
    const std::string& proxySourceIndex() const;
    const std::string& proxyTargetIndex() const;
    const QuantLib::Period& proxySourceRateComputationPeriod() const;
    const QuantLib::Period& proxyTargetRateComputationPeriod() const;

    const boost::optional<ParametricSmileConfiguration> parametricSmileConfiguration() const;

    const ReportConfig& reportConfig() const;
    std::string toString(VolatilityType type) const;

};

%shared_ptr(EquityVolatilityCurveConfig)
class EquityVolatilityCurveConfig : public CurveConfig {
public:
    EquityVolatilityCurveConfig();
    EquityVolatilityCurveConfig(const string& curveID, const string& curveDescription, const string& currency,
                                const std::vector<ext::shared_ptr<VolatilityConfig>>& volatilityConfig,
                                const string& equityId = string(),
                                const string& dayCounter = "A365", const string& calendar = "NullCalendar",
                                const OneDimSolverConfig& solverConfig = OneDimSolverConfig(),
                                const boost::optional<bool>& preferOutOfTheMoney = boost::none);
    EquityVolatilityCurveConfig(const string& curveID, const string& curveDescription, const string& currency,
                                const ext::shared_ptr<VolatilityConfig>& volatilityConfig,
                                const string& equityId = string(),
                                const string& dayCounter = "A365", const string& calendar = "NullCalendar",
                                const OneDimSolverConfig& solverConfig = OneDimSolverConfig(),
                                const boost::optional<bool>& preferOutOfTheMoney = boost::none);
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    const string& equityId() const;
    const string& ccy() const;
    const string& dayCounter() const;
    const string& calendar() const;
    const std::vector<ext::shared_ptr<VolatilityConfig>>& volatilityConfig() const;
    const string quoteStem(const std::string& volType) const;
    void populateQuotes();
    bool isProxySurface();
    OneDimSolverConfig solverConfig() const;
    const boost::optional<bool>& preferOutOfTheMoney() const;
    const ReportConfig& reportConfig() const;
    string& ccy();
    string& dayCounter();
};

#endif