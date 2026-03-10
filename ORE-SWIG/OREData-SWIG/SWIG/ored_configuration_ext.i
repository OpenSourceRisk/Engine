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

#ifndef ored_configuration_ext_i
#define ored_configuration_ext_i

%include ored_curveconfigurations.i

%{
using ore::data::BootstrapConfig;
using ore::data::SecurityConfig;
using ore::data::CommodityVolatilityConfig;
using ore::data::InflationCapFloorVolatilityCurveConfig;
%}

%shared_ptr(BootstrapConfig)
class BootstrapConfig : public XMLSerializable {
public:
    BootstrapConfig(QuantLib::Real accuracy = 1.0e-12,
                    QuantLib::Real globalAccuracy = QuantLib::Null<QuantLib::Real>(),
                    bool dontThrow = false, QuantLib::Size maxAttempts = 5, QuantLib::Real maxFactor = 2.0,
                    QuantLib::Real minFactor = 2.0, QuantLib::Size dontThrowSteps = 10, bool global = false,
                    Real smoothnessLambda = 0.0);
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(SecurityConfig)
class SecurityConfig : public CurveConfig {
public:
    SecurityConfig(const std::string& curveID, const std::string& curveDescription, const std::string& spreadQuote = "",
                   const std::string& recoveryQuote = "", const std::string& cprQuote = "", const std::string& priceQuote = "",
                   const std::string& conversionFactor = "");
    SecurityConfig();
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(CommodityVolatilityConfig)
class CommodityVolatilityConfig : public CurveConfig {
public:
    CommodityVolatilityConfig();
    CommodityVolatilityConfig(const std::string& curveId, const std::string& curveDescription,
                              const std::string& currency,
                              const std::vector<QuantLib::ext::shared_ptr<VolatilityConfig>>& volatilityConfig,
                              const std::string& dayCounter = "A365", const std::string& calendar = "NullCalendar",
                              const std::string& futureConventionsId = "", QuantLib::Natural optionExpiryRollDays = 0,
                              const std::string& priceCurveId = "", const std::string& yieldCurveId = "",
                              const std::string& quoteSuffix = "",
                              const OneDimSolverConfig& solverConfig = OneDimSolverConfig(),
                              const QuantLib::ext::optional<bool>& preferOutOfTheMoney = QuantLib::ext::nullopt);
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(InflationCapFloorVolatilityCurveConfig)
class InflationCapFloorVolatilityCurveConfig : public CurveConfig {
public:
    enum class Type { ZC, YY };
    enum class VolatilityType { Lognormal, Normal, ShiftedLognormal };
    enum class QuoteType { Price, Volatility };

    InflationCapFloorVolatilityCurveConfig();
    InflationCapFloorVolatilityCurveConfig(
        const std::string& curveID, const std::string& curveDescription, const Type type, const QuoteType& quoteType,
        const VolatilityType& volatilityType, const bool extrapolate, const std::vector<std::string>& tenors,
        const std::vector<std::string>& capStrikes, const std::vector<std::string>& floorStrikes, const std::vector<std::string>& strikes,
        const QuantLib::DayCounter& dayCounter, QuantLib::Natural settleDays, const QuantLib::Calendar& calendar,
        const QuantLib::BusinessDayConvention& businessDayConvention, const std::string& index, const std::string& indexCurve,
        const std::string& yieldTermStructure, const QuantLib::Period& observationLag, const std::string& quoteIndex = "",
        const std::string& conventions = "", const bool useLastAvailableFixingDate = false);

    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

#endif
