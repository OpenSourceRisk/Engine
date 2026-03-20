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
using ore::data::BaselTrafficLightData;
using ore::data::AdjustmentFactors;
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
        const std::string& conventions = "");

    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(BaselTrafficLightData)
%feature("flatnested") BaselTrafficLightData;
%rename(BaselTrafficLightObservationData) BaselTrafficLightData::ObservationData;
class BaselTrafficLightData : public XMLSerializable {
public:
    struct ObservationData {
        ObservationData();
        std::vector<int> observationCount;
        std::vector<int> amberLimit;
        std::vector<int> redLimit;
    };

    BaselTrafficLightData();
    BaselTrafficLightData(const std::string& filename);
    BaselTrafficLightData(const std::map<int, ObservationData>& baselTrafficLight);

    void clear();

    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;

    std::map<int, ObservationData>& baselTrafficLightData();
    void setbaselTrafficLightData(std::map<int, ObservationData> baselTrafficLight);

    %extend {
        void setObservationData(int key, const std::vector<int>& observationCount,
                                const std::vector<int>& amberLimit, const std::vector<int>& redLimit) {
            ore::data::BaselTrafficLightData::ObservationData data;
            data.observationCount = observationCount;
            data.amberLimit = amberLimit;
            data.redLimit = redLimit;
            self->baselTrafficLightData()[key] = data;
        }
    }
};

%template(BaselTrafficLightObservationDataMap) std::map<int, BaselTrafficLightData::ObservationData>;

%shared_ptr(AdjustmentFactors)
class AdjustmentFactors : public XMLSerializable {
public:
    AdjustmentFactors(QuantLib::Date asof);

    bool hasFactor(const std::string& name) const;
    QuantLib::Real getFactor(const std::string& name, const QuantLib::Date& d) const;
    void addFactor(std::string name, QuantLib::Date d, QuantLib::Real factor);

    void fromXML(ore::data::XMLNode* node) override;
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;

    std::set<std::string> names() const;
    std::set<QuantLib::Date> dates(const std::string& name) const;
    QuantLib::Real getFactorContribution(const std::string& name, const QuantLib::Date& d) const;

    %extend {
        static ext::shared_ptr<AdjustmentFactors> create(const QuantLib::Date& asof) {
            return QuantLib::ext::make_shared<ore::data::AdjustmentFactors>(asof);
        }
    }
};

#if defined(SWIGPYTHON)
%pythoncode %{
if 'BaselTrafficLightObservationData' in globals():
    BaselTrafficLightData.ObservationData = BaselTrafficLightObservationData
%}
#endif

#endif
