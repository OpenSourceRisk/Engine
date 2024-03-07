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

#include <orea/scenario/scenariosimmarket.hpp>
#include <orea/scenario/scenariosimmarketparameters.hpp>
#include <orea/scenario/sensitivityscenariodata.hpp>
#include <ored/marketdata/market.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ql/instruments/inflationcapfloor.hpp>
#include <map>
#include <set>
#include <tuple>

namespace ore {
namespace analytics {
using namespace std;
using namespace QuantLib;
using namespace ore::data;

class ParSensitivityInstrumentBuilder {
public:
    struct Instruments {
        //! par helpers (all except cap/floors)
        std::map<ore::analytics::RiskFactorKey, boost::shared_ptr<Instrument>> parHelpers_;
        //! par helpers: IR cap / floors
        std::map<ore::analytics::RiskFactorKey, boost::shared_ptr<QuantLib::CapFloor>> parCaps_;
        std::map<ore::analytics::RiskFactorKey, Handle<YieldTermStructure>> parCapsYts_;
        std::map<ore::analytics::RiskFactorKey, Handle<OptionletVolatilityStructure>> parCapsVts_;
        //! par helpers: YoY cap / floors
        std::map<ore::analytics::RiskFactorKey, Handle<YieldTermStructure>> parYoYCapsYts_;
        std::map<ore::analytics::RiskFactorKey, Handle<YoYInflationIndex>> parYoYCapsIndex_;
        std::map<ore::analytics::RiskFactorKey, boost::shared_ptr<QuantLib::YoYInflationCapFloor>> parYoYCaps_;
        std::map<ore::analytics::RiskFactorKey, Handle<QuantExt::YoYOptionletVolatilitySurface>> parYoYCapsVts_;
        //! par instrument pillars
        std::map<std::string, std::vector<Period>> yieldCurvePillars_, capFloorPillars_, cdsPillars_,
            equityForecastCurvePillars_, zeroInflationPillars_, yoyInflationPillars_, yoyCapFloorPillars_;
        //! list of (raw) risk factors on which a par helper depends
        std::map<ore::analytics::RiskFactorKey, std::set<ore::analytics::RiskFactorKey>> parHelperDependencies_;
        std::map<ore::analytics::RiskFactorKey, Handle<YieldTermStructure>> parCapsYts_;
        // ql index names for which we want to remove today's fixing for the purpose of the par sensi calculation
        std::set<std::string> removeTodaysFixingIndices_;
    };

    ParSensitivityInstrumentBuilder() = default;

    //! Create par instruments
    ParSensitivityInstrumentBuilder::Instruments
    createParInstruments(const QuantLib::Date& asof,
                         const boost::shared_ptr<ore::analytics::ScenarioSimMarketParameters>& simMarketParams,
                         const ore::analytics::SensitivityScenarioData& sensitivityData,
                         const std::set<ore::analytics::RiskFactorKey::KeyType>& typesDisabled = {},
                         const std::set<ore::analytics::RiskFactorKey::KeyType>& parTypes = {},
                         const std::set<ore::analytics::RiskFactorKey>& relevantRiskFactors = {},
                         const bool continueOnError = false,
                         const string& marketConfiguration = Market::defaultConfiguration,
                         const boost::shared_ptr<ore::analytics::ScenarioSimMarket>& simMarket = nullptr) const;

private:
    //! Create Deposit for implying par rate sensitivity from zero rate sensitivity
    std::pair<boost::shared_ptr<QuantLib::Instrument>, Date>
    makeDeposit(const QuantLib::Date& asof, const boost::shared_ptr<ore::data::Market>& market, string ccy, string indexName, string yieldCurveName,
                string equityForecastCurveName, Period term, const boost::shared_ptr<Convention>& conventions,
                const string& marketConfiguration = Market::defaultConfiguration) const;

    //! Create FRA for implying par rate sensitivity from zero rate sensitivity
    std::pair<boost::shared_ptr<QuantLib::Instrument>, Date>
    makeFRA(const QuantLib::Date& asof, const boost::shared_ptr<ore::data::Market>& market, string ccy,
            string indexName, string yieldCurveName, string equityForecastCurveName, Period term,
            const boost::shared_ptr<Convention>& conventions,
            const string& marketConfiguration = Market::defaultConfiguration) const;

    //! Create Swap for implying par rate sensitivity from zero rate sensitivity
    std::pair<boost::shared_ptr<QuantLib::Instrument>, Date>
    makeSwap(const boost::shared_ptr<ore::data::Market>& market, string ccy, string indexName, string yieldCurveName,
             string equityForecastCurveName, Period term, const boost::shared_ptr<Convention>& conventions,
             bool singleCurve, std::set<ore::analytics::RiskFactorKey>& parHelperDependencies,
             std::set<std::string>& removeTodaysFixingIndices, const std::string& expDiscountCurve = "",
             const string& marketConfiguration = Market::defaultConfiguration) const;

    //! Create OIS Swap for implying par rate sensitivity from zero rate sensitivity
    std::pair<boost::shared_ptr<QuantLib::Instrument>, Date>
    makeOIS(const boost::shared_ptr<ore::data::Market>& market, string ccy, string indexName, string yieldCurveName,
            string equityForecastCurveName, Period term, const boost::shared_ptr<Convention>& conventions,
            bool singleCurve, std::set<ore::analytics::RiskFactorKey>& parHelperDependencies,std::set<std::string>& removeTodaysFixingIndices,
            const std::string& expDiscountCurve = "", const string& marketConfiguration = Market::defaultConfiguration) const;

    //! Create Basis Swap for implying par rate sensitivity from zero rate sensitivity
    std::pair<boost::shared_ptr<QuantLib::Instrument>, Date>
    makeTenorBasisSwap(const QuantLib::Date& asof, const boost::shared_ptr<ore::data::Market>& market, string ccy, string receiveIndexName,
                       string payIndexName, string yieldCurveName, string equityForecastCurveName, Period term,
                       const boost::shared_ptr<Convention>& conventions, const bool singleCurve,
                       std::set<ore::analytics::RiskFactorKey>& parHelperDependencies,
                       std::set<std::string>& removeTodaysFixingIndices,
                       const std::string& expDiscountCurve = "",
                       const string& marketConfiguration = Market::defaultConfiguration) const;

    //! Create Cap/Floor instrument for implying flat vol sensitivity from optionlet vol sensitivity
    boost::shared_ptr<QuantLib::CapFloor> makeCapFloor(
        const boost::shared_ptr<ore::data::Market>& market, string ccy, string indexName, Period term, Real strike,
        bool generatePillar, // isAtm ?
        std::set<ore::analytics::RiskFactorKey>& parHelperDependencies, const std::string& expDiscountCurve = "",
        const string& marketConfiguration = Market::defaultConfiguration) const;

    //! Create Cross Ccy Basis Swap for implying par rate sensitivity from zero rate sensitivity
    std::pair<boost::shared_ptr<QuantLib::Instrument>, Date>
    makeCrossCcyBasisSwap(const boost::shared_ptr<ore::data::Market>& market, string baseCcy, string ccy, Period term,
                          const boost::shared_ptr<Convention>& conventions,
                          std::set<ore::analytics::RiskFactorKey>& parHelperDependencies,
                          std::set<std::string>& removeTodaysFixingIndices,
                          const string& marketConfiguration = Market::defaultConfiguration) const;

    //! Create FX Forwrad for implying par rate sensitivity from zero rate sensitivity
    std::pair<boost::shared_ptr<QuantLib::Instrument>, Date>
    makeFxForward(const boost::shared_ptr<ore::data::Market>& market, string baseCcy, string ccy, Period term,
                  const boost::shared_ptr<Convention>& conventions,
                  std::set<ore::analytics::RiskFactorKey>& parHelperDependencies,
                  const string& marketConfiguration = Market::defaultConfiguration) const;

    //! Create CDS for implying par rate sensitivity from Hazard Rate sensitivity
    std::pair<boost::shared_ptr<Instrument>, Date>
    makeCDS(const boost::shared_ptr<ore::data::Market>& market, string name, string ccy, Period term,
            const boost::shared_ptr<Convention>& conventions,
            std::set<ore::analytics::RiskFactorKey>& parHelperDependencies, const std::string& expDiscountCurve = "",
            const string& marketConfiguration = Market::defaultConfiguration) const;

    //! Create Zero Swap for implying par rate sensitivity from zero rate sensitivity
    boost::shared_ptr<QuantLib::Instrument>
    makeZeroInflationSwap(const boost::shared_ptr<ore::data::Market>& market, string indexName, Period term,
                          const boost::shared_ptr<Convention>& conventions, bool singleCurve,
                          std::set<ore::analytics::RiskFactorKey>& parHelperDependencies,
                          const std::string& expDiscountCurve = "",
                          const string& marketConfiguration = Market::defaultConfiguration) const;

    //! Create YoY Swap for implying par rate sensitivity from yoy rate sensitivity
    boost::shared_ptr<QuantLib::Instrument>
    makeYoyInflationSwap(const boost::shared_ptr<ore::data::Market>& market, string indexName, Period term,
                         const boost::shared_ptr<Convention>& conventions, bool singleCurve, bool fromZero,
                         std::set<ore::analytics::RiskFactorKey>& parHelperDependencies,
                         const std::string& expDiscountCurve = "",
                         const string& marketConfiguration = Market::defaultConfiguration) const;

    //! Create YoY Cap/Floor for implying rate rate sensitivity from yoy optionlet vol sensitivity
    void
    makeYoYCapFloor(ParSensitivityInstrumentBuilder::Instruments& instruments,
                    const boost::shared_ptr<Market>& market, string indexName, Period term, Real strike,
                    const boost::shared_ptr<Convention>& convention, bool singleCurve, bool fromZero,
                    const std::string& expDiscountCurve, const ore::analytics::RiskFactorKey& key,
                    const string& marketConfiguration = Market::defaultConfiguration) const;
};

class ParSensitivityHelperFunctions {
public:
    //! Computes the implied quote
    Real impliedQuote(const boost::shared_ptr<Instrument>& i) const;

    //! true if key type and name are equal, do not care about the index though
    bool riskFactorKeysAreSimilar(const ore::analytics::RiskFactorKey& x, const ore::analytics::RiskFactorKey& y) {
        return x.keytype == y.keytype && x.name == y.name;
    }

    /*! Utility for implying a flat volatility which reproduces the provided Cap/Floor price
      IR: CapFloorType = CapFloor, IndexType not used
        YY: CapFloorType = YoYInflationCapFloor, IndexType = YoYInflationIndex */
    template <typename CapFloorType, typename IndexType = QuantLib::Index>
    Volatility impliedVolatility(const CapFloorType& cap, Real targetValue, const Handle<YieldTermStructure>& d,
                                 Volatility guess, VolatilityType type, Real displacement,
                                 const Handle<IndexType>& index = Handle<IndexType>());
};

} // namespace analytics
} // namespace ore
