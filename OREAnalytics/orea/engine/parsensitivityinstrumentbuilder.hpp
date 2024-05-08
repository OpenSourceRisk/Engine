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

/*! \file orea/engine/parsensitivityanalysis.hpp
    \brief Perfrom sensitivity analysis for a given portfolio
    \ingroup simulation
*/

#pragma once

#include <map>
#include <orea/scenario/scenariosimmarket.hpp>
#include <orea/scenario/scenariosimmarketparameters.hpp>
#include <orea/scenario/sensitivityscenariodata.hpp>
#include <ored/marketdata/market.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <set>

namespace ore {
namespace analytics {

class ParSensitivityInstrumentBuilder {
public:
    struct Instruments {
        //! par helpers (all except cap/floors)
        std::map<ore::analytics::RiskFactorKey, QuantLib::ext::shared_ptr<QuantLib::Instrument>> parHelpers_;
        //! par helpers: IR cap / floors
        std::map<ore::analytics::RiskFactorKey, QuantLib::ext::shared_ptr<QuantLib::CapFloor>> parCaps_;
        std::map<ore::analytics::RiskFactorKey, QuantLib::Handle<QuantLib::YieldTermStructure>> parCapsYts_;
        std::map<ore::analytics::RiskFactorKey, QuantLib::Handle<QuantLib::OptionletVolatilityStructure>> parCapsVts_;
        //! par helpers: YoY cap / floors
        std::map<ore::analytics::RiskFactorKey, QuantLib::Handle<QuantLib::YieldTermStructure>> parYoYCapsYts_;
        std::map<ore::analytics::RiskFactorKey, QuantLib::Handle<QuantLib::YoYInflationIndex>> parYoYCapsIndex_;
        std::map<ore::analytics::RiskFactorKey, QuantLib::ext::shared_ptr<QuantLib::YoYInflationCapFloor>> parYoYCaps_;
        std::map<ore::analytics::RiskFactorKey, QuantLib::Handle<QuantExt::YoYOptionletVolatilitySurface>> parYoYCapsVts_;
        //! par QuantLib::Instrument pillars
        std::map<std::string, std::vector<QuantLib::Period>> yieldCurvePillars_, capFloorPillars_, cdsPillars_,
            equityForecastCurvePillars_, zeroInflationPillars_, yoyInflationPillars_, yoyCapFloorPillars_;
        //! list of (raw) risk factors on which a par helper depends
        std::map<ore::analytics::RiskFactorKey, std::set<ore::analytics::RiskFactorKey>> parHelperDependencies_;
        // ql index names for which we want to remove today's fixing for the purpose of the par sensi calculation
        std::set<std::string> removeTodaysFixingIndices_;
    };

    ParSensitivityInstrumentBuilder() = default;

    //! Create par QuantLib::Instruments
    void
    createParInstruments(ParSensitivityInstrumentBuilder::Instruments& instruments, const QuantLib::Date& asof,
                         const QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters>& simMarketParams,
                         const ore::analytics::SensitivityScenarioData& sensitivityData,
                         const std::set<ore::analytics::RiskFactorKey::KeyType>& typesDisabled = {},
                         const std::set<ore::analytics::RiskFactorKey::KeyType>& parTypes = {},
                         const std::set<ore::analytics::RiskFactorKey>& relevantRiskFactors = {},
                         const bool continueOnError = false,
                         const std::string& marketConfiguration = ore::data::Market::defaultConfiguration,
                         const QuantLib::ext::shared_ptr<ore::analytics::Market>& simMarket = nullptr) const;

private:
    //! Create Deposit for implying par rate sensitivity from zero rate sensitivity
    std::pair<QuantLib::ext::shared_ptr<QuantLib::Instrument>, Date>
    makeDeposit(const QuantLib::Date& asof, const QuantLib::ext::shared_ptr<ore::data::Market>& market, std::string ccy,
                std::string indexName, std::string yieldCurveName, std::string equityForecastCurveName, QuantLib::Period term,
                const QuantLib::ext::shared_ptr<ore::data::Convention>& conventions,
                const std::string& marketConfiguration = ore::data::Market::defaultConfiguration) const;

    //! Create FRA for implying par rate sensitivity from zero rate sensitivity
    std::pair<QuantLib::ext::shared_ptr<QuantLib::Instrument>, Date>
    makeFRA(const QuantLib::Date& asof, const QuantLib::ext::shared_ptr<ore::data::Market>& market, std::string ccy,
            std::string indexName, std::string yieldCurveName, std::string equityForecastCurveName,
            QuantLib::Period term, const QuantLib::ext::shared_ptr<ore::data::Convention>& conventions,
            const std::string& marketConfiguration = ore::data::Market::defaultConfiguration) const;

    //! Create Swap for implying par rate sensitivity from zero rate sensitivity
    std::pair<QuantLib::ext::shared_ptr<QuantLib::Instrument>, Date>
    makeSwap(const QuantLib::ext::shared_ptr<ore::data::Market>& market, std::string ccy, std::string indexName,
             std::string yieldCurveName, std::string equityForecastCurveName, QuantLib::Period term,
             const QuantLib::ext::shared_ptr<ore::data::Convention>& conventions, bool singleCurve,
             std::set<ore::analytics::RiskFactorKey>& parHelperDependencies,
             std::set<std::string>& removeTodaysFixingIndices, const std::string& expDiscountCurve = "",
             const std::string& marketConfiguration = ore::data::Market::defaultConfiguration) const;

    //! Create OIS Swap for implying par rate sensitivity from zero rate sensitivity
    std::pair<QuantLib::ext::shared_ptr<QuantLib::Instrument>, Date>
    makeOIS(const QuantLib::ext::shared_ptr<ore::data::Market>& market, std::string ccy, std::string indexName,
            std::string yieldCurveName, std::string equityForecastCurveName, QuantLib::Period term,
            const QuantLib::ext::shared_ptr<ore::data::Convention>& conventions, bool singleCurve,
            std::set<ore::analytics::RiskFactorKey>& parHelperDependencies,
            std::set<std::string>& removeTodaysFixingIndices, const std::string& expDiscountCurve = "",
            const std::string& marketConfiguration = ore::data::Market::defaultConfiguration) const;

    //! Create Basis Swap for implying par rate sensitivity from zero rate sensitivity
    std::pair<QuantLib::ext::shared_ptr<QuantLib::Instrument>, Date>
    makeTenorBasisSwap(const QuantLib::Date& asof, const QuantLib::ext::shared_ptr<ore::data::Market>& market, std::string ccy,
                       std::string receiveIndexName, std::string payIndexName, std::string yieldCurveName,
                       std::string equityForecastCurveName, QuantLib::Period term, const QuantLib::ext::shared_ptr<ore::data::Convention>& conventions,
                       const bool singleCurve, std::set<ore::analytics::RiskFactorKey>& parHelperDependencies,
                       std::set<std::string>& removeTodaysFixingIndices, const std::string& expDiscountCurve = "",
                       const std::string& marketConfiguration = ore::data::Market::defaultConfiguration) const;

    //! Create Cap/Floor QuantLib::Instrument for implying flat vol sensitivity from optionlet vol sensitivity
    QuantLib::ext::shared_ptr<QuantLib::CapFloor> makeCapFloor(
        const QuantLib::ext::shared_ptr<ore::data::Market>& market, std::string ccy, std::string indexName, QuantLib::Period term, double strike,
        bool generatePillar, // isAtm ?
        std::set<ore::analytics::RiskFactorKey>& parHelperDependencies, const std::string& expDiscountCurve = "",
        const std::string& marketConfiguration = ore::data::Market::defaultConfiguration) const;

    //! Create Cross Ccy Basis Swap for implying par rate sensitivity from zero rate sensitivity
    std::pair<QuantLib::ext::shared_ptr<QuantLib::Instrument>, Date>
    makeCrossCcyBasisSwap(const QuantLib::ext::shared_ptr<ore::data::Market>& market, std::string baseCcy, std::string ccy, QuantLib::Period term,
                          const QuantLib::ext::shared_ptr<ore::data::Convention>& conventions,
                          std::set<ore::analytics::RiskFactorKey>& parHelperDependencies,
                          std::set<std::string>& removeTodaysFixingIndices,
                          const std::string& marketConfiguration = ore::data::Market::defaultConfiguration) const;

    //! Create FX Forwrad for implying par rate sensitivity from zero rate sensitivity
    std::pair<QuantLib::ext::shared_ptr<QuantLib::Instrument>, Date>
    makeFxForward(const QuantLib::ext::shared_ptr<ore::data::Market>& market, std::string baseCcy, std::string ccy, QuantLib::Period term,
                  const QuantLib::ext::shared_ptr<ore::data::Convention>& conventions,
                  std::set<ore::analytics::RiskFactorKey>& parHelperDependencies,
                  const std::string& marketConfiguration = ore::data::Market::defaultConfiguration) const;

    //! Create CDS for implying par rate sensitivity from Hazard Rate sensitivity
    std::pair<QuantLib::ext::shared_ptr<QuantLib::Instrument>, Date>
    makeCDS(const QuantLib::ext::shared_ptr<ore::data::Market>& market, std::string name, std::string ccy, QuantLib::Period term,
            const QuantLib::ext::shared_ptr<ore::data::Convention>& conventions,
            std::set<ore::analytics::RiskFactorKey>& parHelperDependencies, const std::string& expDiscountCurve = "",
            const std::string& marketConfiguration = ore::data::Market::defaultConfiguration) const;

    //! Create Zero Swap for implying par rate sensitivity from zero rate sensitivity
    QuantLib::ext::shared_ptr<QuantLib::Instrument>
    makeZeroInflationSwap(const QuantLib::ext::shared_ptr<ore::data::Market>& market, std::string indexName, QuantLib::Period term,
                          const QuantLib::ext::shared_ptr<ore::data::Convention>& conventions, bool singleCurve,
                          std::set<ore::analytics::RiskFactorKey>& parHelperDependencies,
                          const std::string& expDiscountCurve = "",
                          const std::string& marketConfiguration = ore::data::Market::defaultConfiguration) const;

    //! Create YoY Swap for implying par rate sensitivity from yoy rate sensitivity
    QuantLib::ext::shared_ptr<QuantLib::Instrument>
    makeYoyInflationSwap(const QuantLib::ext::shared_ptr<ore::data::Market>& market, std::string indexName, QuantLib::Period term,
                         const QuantLib::ext::shared_ptr<ore::data::Convention>& conventions, bool singleCurve, bool fromZero,
                         std::set<ore::analytics::RiskFactorKey>& parHelperDependencies,
                         const std::string& expDiscountCurve = "",
                         const std::string& marketConfiguration = ore::data::Market::defaultConfiguration) const;

    //! Create YoY Cap/Floor for implying rate rate sensitivity from yoy optionlet vol sensitivity
    void makeYoYCapFloor(ParSensitivityInstrumentBuilder::Instruments& instruments,
                         const QuantLib::ext::shared_ptr<Market>& market, std::string indexName, QuantLib::Period term,
                         double strike, const QuantLib::ext::shared_ptr<ore::data::Convention>& convention, bool singleCurve,
                         bool fromZero, const std::string& expDiscountCurve, const ore::analytics::RiskFactorKey& key,
                         const std::string& marketConfiguration = ore::data::Market::defaultConfiguration) const;
};

} // namespace analytics
} // namespace ore
