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

#ifndef orea_scenario_ext_i
#define orea_scenario_ext_i

%include stl.i
%include types.i
%include qle_termstructures.i
%include ored_xmlutils.i
%include orea_scenariosimmarketparameters.i
%include orea_simulation.i

%shared_ptr(QuantExt::RiskFactorKey)
namespace QuantExt {
class RiskFactorKey {
public:
    enum class KeyType {
        None,
        DiscountCurve,
        YieldCurve,
        IndexCurve,
        SwaptionVolatility,
        YieldVolatility,
        OptionletVolatility,
        FXSpot,
        FXVolatility,
        EquitySpot,
        DividendYield,
        EquityVolatility,
        SurvivalProbability,
        SurvivalWeight,
        RecoveryRate,
        CreditState,
        CDSVolatility,
        BaseCorrelation,
        CPIIndex,
        ZeroInflationCurve,
        ZeroInflationCapFloorVolatility,
        YoYInflationCurve,
        YoYInflationCapFloorVolatility,
        CommodityCurve,
        CommodityVolatility,
        SecuritySpread,
        Correlation,
        CPR
    };

    RiskFactorKey();
    RiskFactorKey(const KeyType& iKeytype, const std::string& iName, const Size& iIndex = 0);

    KeyType keytype;
    std::string name;
    Size index;
};
}

%shared_ptr(QuantExt::Scenario)
%nodefaultctor QuantExt::Scenario;
namespace QuantExt {
class Scenario {
public:
    virtual ~Scenario() {}
    virtual const Date& asof() const = 0;
    virtual void setAsof(const Date& d) = 0;
    virtual const std::string& label() const = 0;
    virtual void label(const std::string&) = 0;
    virtual Real getNumeraire() const = 0;
    virtual void setNumeraire(Real n) = 0;
    virtual bool has(const QuantExt::RiskFactorKey& key) const = 0;
    virtual const std::vector<QuantExt::RiskFactorKey>& keys() const = 0;
    virtual void add(const QuantExt::RiskFactorKey& key, Real value) = 0;
    virtual Real get(const QuantExt::RiskFactorKey& key) const = 0;
    virtual const bool isAbsolute() const = 0;
    virtual void setAbsolute(const bool b) = 0;
    virtual const bool isPar() const = 0;
    virtual void setPar(const bool b) = 0;
    virtual const std::map<std::pair<QuantExt::RiskFactorKey::KeyType, std::string>, std::vector<std::vector<Real>>>&
    coordinates() const = 0;
    virtual QuantLib::ext::shared_ptr<QuantExt::Scenario> clone() const = 0;
};
}

%shared_ptr(ore::analytics::SimpleScenario)
%shared_ptr(ore::analytics::ScenarioFactory)
%nodefaultctor ore::analytics::ScenarioFactory;
%shared_ptr(ore::analytics::CloneScenarioFactory)
%shared_ptr(ore::analytics::ScenarioGenerator)
%nodefaultctor ore::analytics::ScenarioGenerator;
%shared_ptr(ore::analytics::StaticScenarioGenerator)
%shared_ptr(ore::analytics::ScenarioGeneratorData)
%shared_ptr(ore::analytics::ScenarioFilter)
%shared_ptr(ore::analytics::SimMarket)
%nodefaultctor ore::analytics::SimMarket;
%shared_ptr(ore::analytics::ScenarioSimMarket)

namespace ore {
namespace analytics {
class SimpleScenario : public QuantExt::Scenario {
public:
    struct SharedData {
        std::vector<QuantExt::RiskFactorKey> keys;
    };

    SimpleScenario();
    SimpleScenario(QuantLib::Date asof, const std::string& label = std::string(), QuantLib::Real numeraire = 0,
                   const QuantLib::ext::shared_ptr<SharedData>& sharedData = nullptr);

    const QuantLib::Date& asof() const override;
    void setAsof(const QuantLib::Date& d) override;
    const std::string& label() const override;
    void label(const std::string& s) override;
    QuantLib::Real getNumeraire() const override;
    void setNumeraire(QuantLib::Real n) override;
    bool has(const QuantExt::RiskFactorKey& key) const override;
    const std::vector<QuantExt::RiskFactorKey>& keys() const override;
    void add(const QuantExt::RiskFactorKey& key, QuantLib::Real value) override;
    QuantLib::Real get(const QuantExt::RiskFactorKey& key) const override;
    const bool isAbsolute() const override;
    const bool isPar() const override;
    const std::map<std::pair<QuantExt::RiskFactorKey::KeyType, std::string>, std::vector<std::vector<QuantLib::Real>>>&
    coordinates() const override;
    QuantLib::ext::shared_ptr<QuantExt::Scenario> clone() const override;
};

class ScenarioFactory {
public:
    virtual ~ScenarioFactory() {}
    virtual const QuantLib::ext::shared_ptr<QuantExt::Scenario> buildScenario(QuantLib::Date asof, bool isAbsolute,
                                                                     bool isPar = false,
                                                                     const std::string& label = "",
                                                                     QuantLib::Real numeraire = 0.0) const = 0;
};

class CloneScenarioFactory : public ScenarioFactory {
public:
    CloneScenarioFactory(const QuantLib::ext::shared_ptr<QuantExt::Scenario>& baseScenario);
    const QuantLib::ext::shared_ptr<QuantExt::Scenario> buildScenario(QuantLib::Date asof, bool isAbsolute,
                                                            bool isPar = false, const std::string& label = "",
                                                            QuantLib::Real numeraire = 0.0) const override;
};

class ScenarioGenerator {
public:
    virtual ~ScenarioGenerator() {}
    virtual QuantLib::ext::shared_ptr<QuantExt::Scenario> next(const Date& d) = 0;
    virtual void reset() = 0;
};

class StaticScenarioGenerator : public ScenarioGenerator {
public:
    StaticScenarioGenerator();
    void reset() override;
    QuantLib::ext::shared_ptr<QuantExt::Scenario> next(const Date&) override;
    void setScenario(const QuantLib::ext::shared_ptr<QuantExt::Scenario>& s);
};

class ScenarioGeneratorData : public ore::data::XMLSerializable {
public:
    ScenarioGeneratorData();
    void clear();
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

class ScenarioFilter {
public:
    ScenarioFilter();
    virtual ~ScenarioFilter() {}
    virtual bool allow(const QuantExt::RiskFactorKey& key) const;
};

class SimMarket : public ore::data::MarketImpl {
public:
    explicit SimMarket(const bool handlePseudoCurrencies);
    virtual void preUpdate() = 0;
    virtual void updateDate(const Date&) = 0;
    virtual void updateScenario(const Date&) = 0;
    virtual void postUpdate(const Date& d) = 0;
    virtual void updateAsd(const Date&) = 0;
    virtual void reset() = 0;
    virtual const QuantLib::ext::shared_ptr<ore::analytics::FixingManager>& fixingManager() const = 0;
};

class ScenarioSimMarket : public SimMarket {
public:
    explicit ScenarioSimMarket(const bool handlePseudoCurrencies);

    virtual QuantLib::ext::shared_ptr<ore::analytics::ScenarioGenerator>& scenarioGenerator();
    virtual const QuantLib::ext::shared_ptr<ore::analytics::ScenarioGenerator>& scenarioGenerator() const;
    virtual QuantLib::ext::shared_ptr<ore::analytics::AggregationScenarioData>& aggregationScenarioData();
    virtual const QuantLib::ext::shared_ptr<ore::analytics::AggregationScenarioData>& aggregationScenarioData() const;
    virtual QuantLib::ext::shared_ptr<ore::analytics::ScenarioFilter>& filter();
    virtual const QuantLib::ext::shared_ptr<ore::analytics::ScenarioFilter>& filter() const;

    virtual void preUpdate() override;
    virtual void updateScenario(const Date&) override;
    virtual void updateDate(const Date&) override;
    virtual void postUpdate(const Date& d) override;
    virtual void updateAsd(const Date&) override;
    virtual void reset() override;

    virtual QuantLib::ext::shared_ptr<QuantExt::Scenario> baseScenario() const;
    virtual QuantLib::ext::shared_ptr<QuantExt::Scenario> baseScenarioAbsolute() const;
    bool useSpreadedTermStructures() const;
    const QuantLib::ext::shared_ptr<ore::analytics::FixingManager>& fixingManager() const override;
    virtual bool isSimulated(const QuantExt::RiskFactorKey::KeyType& factor) const;
    void applyScenario(const QuantLib::ext::shared_ptr<QuantExt::Scenario>& scenario);
};

} // namespace analytics
} // namespace ore

#endif
