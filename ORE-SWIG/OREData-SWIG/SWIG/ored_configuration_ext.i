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

%shared_ptr(ore::data::BootstrapConfig)
%shared_ptr(ore::data::SecurityConfig)
%shared_ptr(ore::data::CommodityVolatilityConfig)
%shared_ptr(ore::data::InflationCapFloorVolatilityCurveConfig)
%shared_ptr(ore::data::BaselTrafficLightData)
%shared_ptr(ore::data::AdjustmentFactors)
%feature("flatnested") BaselTrafficLightData;
%rename(BaselTrafficLightObservationData) ore::data::BaselTrafficLightData::ObservationData;

namespace ore {
namespace data {
class BootstrapConfig : public ore::data::XMLSerializable {
public:
    BootstrapConfig(QuantLib::Real accuracy = 1.0e-12,
                    QuantLib::Real globalAccuracy = QuantLib::Null<QuantLib::Real>(),
                    bool dontThrow = false, QuantLib::Size maxAttempts = 5, QuantLib::Real maxFactor = 2.0,
                    QuantLib::Real minFactor = 2.0, QuantLib::Size dontThrowSteps = 10, bool global = false,
                    Real smoothnessLambda = 0.0);
    void fromXML(ore::data::XMLNode* node) override;
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
};

class SecurityConfig : public ore::data::CurveConfig {
public:
    SecurityConfig(const std::string& curveID, const std::string& curveDescription, const std::string& spreadQuote = "",
                   const std::string& recoveryQuote = "", const std::string& cprQuote = "", const std::string& priceQuote = "",
                   const std::string& conversionFactor = "");
    SecurityConfig();
    void fromXML(ore::data::XMLNode* node) override;
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
};

class CommodityVolatilityConfig : public ore::data::CurveConfig {
public:
    CommodityVolatilityConfig();
    CommodityVolatilityConfig(const std::string& curveId, const std::string& curveDescription,
                              const std::string& currency,
                              const std::vector<QuantLib::ext::shared_ptr<ore::data::VolatilityConfig>>& volatilityConfig,
                              const std::string& dayCounter = "A365", const std::string& calendar = "NullCalendar",
                              const std::string& futureConventionsId = "", QuantLib::Natural optionExpiryRollDays = 0,
                              const std::string& priceCurveId = "", const std::string& yieldCurveId = "",
                              const std::string& quoteSuffix = "",
                              const ore::data::OneDimSolverConfig& solverConfig = ore::data::OneDimSolverConfig(),
                              const QuantLib::ext::optional<bool>& preferOutOfTheMoney = QuantLib::ext::nullopt);
    void fromXML(ore::data::XMLNode* node) override;
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
};

class InflationCapFloorVolatilityCurveConfig : public ore::data::CurveConfig {
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

    void fromXML(ore::data::XMLNode* node) override;
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
};

class BaselTrafficLightData : public ore::data::XMLSerializable {
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

    void fromXML(ore::data::XMLNode* node) override;
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;

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

class AdjustmentFactors : public ore::data::XMLSerializable {
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
        static ext::shared_ptr<ore::data::AdjustmentFactors> create(const QuantLib::Date& asof) {
            return QuantLib::ext::make_shared<ore::data::AdjustmentFactors>(asof);
        }
    }
};

} // namespace data
} // namespace ore

%template(BaselTrafficLightObservationDataMap) std::map<int, ore::data::BaselTrafficLightData::ObservationData>;

#if defined(SWIGPYTHON)
%pythoncode %{
if 'BaselTrafficLightObservationData' in globals():
    BaselTrafficLightData.ObservationData = BaselTrafficLightObservationData
%}
#endif

#endif
