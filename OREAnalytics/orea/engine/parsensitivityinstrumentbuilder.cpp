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

#include <orea/app/structuredanalyticserror.hpp>
#include <orea/cube/inmemorycube.hpp>
#include <orea/engine/observationmode.hpp>
#include <orea/engine/parsensitivityinstrumentbuilder.hpp>
#include <orea/engine/valuationengine.hpp>
#include <orea/scenario/sensitivityscenariodata.hpp>
#include <orea/scenario/simplescenariofactory.hpp>

#include <ored/marketdata/inflationcurve.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/marketdata.hpp>
#include <ored/utilities/to_string.hpp>

#include <qle/indexes/inflationindexwrapper.hpp>
#include <qle/instruments/brlcdiswap.hpp>
#include <qle/instruments/crossccybasismtmresetswap.hpp>
#include <qle/instruments/crossccybasisswap.hpp>
#include <qle/instruments/deposit.hpp>
#include <qle/instruments/fxforward.hpp>
#include <qle/instruments/makecds.hpp>
#include <qle/instruments/subperiodsswap.hpp>
#include <qle/instruments/tenorbasisswap.hpp>
#include <qle/math/blockmatrixinverse.hpp>
#include <qle/pricingengines/crossccyswapengine.hpp>
#include <qle/pricingengines/depositengine.hpp>
#include <qle/pricingengines/discountingfxforwardengine.hpp>
#include <qle/pricingengines/inflationcapfloorengines.hpp>

#include <ql/cashflows/capflooredinflationcoupon.hpp>
#include <ql/cashflows/indexedcashflow.hpp>
#include <ql/cashflows/overnightindexedcoupon.hpp>
#include <ql/errors.hpp>
#include <ql/indexes/ibor/libor.hpp>
#include <ql/instruments/creditdefaultswap.hpp>
#include <ql/instruments/forwardrateagreement.hpp>
#include <ql/instruments/makecapfloor.hpp>
#include <ql/instruments/makeois.hpp>
#include <ql/instruments/makevanillaswap.hpp>
#include <ql/instruments/yearonyearinflationswap.hpp>
#include <ql/instruments/zerocouponinflationswap.hpp>
#include <ql/math/solvers1d/newtonsafe.hpp>
#include <ql/pricingengines/capfloor/bacheliercapfloorengine.hpp>
#include <ql/pricingengines/capfloor/blackcapfloorengine.hpp>
#include <ql/pricingengines/credit/midpointcdsengine.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/quotes/derivedquote.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/termstructures/yield/oisratehelper.hpp>
#include <qle/instruments/fixedbmaswap.hpp>

#include <boost/lexical_cast.hpp>
#include <boost/numeric/ublas/operation.hpp>
#include <boost/numeric/ublas/vector.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;
using namespace ore::data;
using namespace ore::analytics;

using boost::numeric::ublas::element_div;
using boost::numeric::ublas::element_prod;

namespace ore {
namespace analytics {

void ParSensitivityInstrumentBuilder::createParInstruments(
    ParSensitivityInstrumentBuilder::Instruments &instruments,
    const QuantLib::Date& asof,
    const QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters>& simMarketParams,
    const ore::analytics::SensitivityScenarioData& sensitivityData,
    const std::set<ore::analytics::RiskFactorKey::KeyType>& typesDisabled,
    const std::set<ore::analytics::RiskFactorKey::KeyType>& parTypes,
    const std::set<ore::analytics::RiskFactorKey>& relevantRiskFactors, const bool continueOnError,
    const string& marketConfiguration, const QuantLib::ext::shared_ptr<Market>& simMarket) const {

    QL_REQUIRE(typesDisabled != parTypes, "At least one par risk factor type must be enabled "
                                              << "for a valid ParSensitivityAnalysis.");

    // this is called twice, first (dry run) with simMarket = nullptr to
    // - do pillar adjustments and
    // - populate the par helper dependencies
    // and second with simMarket != null to
    // - create the final par instruments linked to the sim market

    bool dryRun = simMarket == nullptr;
    if (dryRun) {
        Settings::instance().evaluationDate() = asof;
    }

    LOG("Build par instruments...");
    auto& parHelpers_ = instruments.parHelpers_;
    auto& parCaps_ = instruments.parCaps_;
    auto& parYoYCaps_ = instruments.parYoYCaps_;
    auto& parHelperDependencies_ = instruments.parHelperDependencies_;
    auto& yieldCurvePillars_ = instruments.yieldCurvePillars_;
    auto& parCapsYts_ = instruments.parCapsYts_;
    auto& parCapsVts_ = instruments.parCapsVts_;
    auto& capFloorPillars_ = instruments.capFloorPillars_;
    auto& cdsPillars_ = instruments.cdsPillars_;
    auto& yoyCapFloorPillars_ = instruments.yoyCapFloorPillars_;
    auto& zeroInflationPillars_ = instruments.zeroInflationPillars_;

    parHelpers_.clear();
    parCaps_.clear();
    parYoYCaps_.clear();

    const QuantLib::ext::shared_ptr<Conventions>& conventions = InstrumentConventions::instance().conventions();
    QL_REQUIRE(conventions != nullptr, "conventions are empty");

    // Discount curve instruments
    if (typesDisabled.count(RiskFactorKey::KeyType::DiscountCurve) == 0) {

        LOG("ParSensitivityAnalysis: Discount curve par instruments");
        for (auto c : sensitivityData.discountCurveShiftData()) {
            string ccy = c.first;
            QL_REQUIRE(simMarket || yieldCurvePillars_.find(ccy) == yieldCurvePillars_.end(),
                       "duplicate entry in yieldCurvePillars '" << ccy << "'");
            SensitivityScenarioData::CurveShiftParData data =
                *QuantLib::ext::dynamic_pointer_cast<SensitivityScenarioData::CurveShiftParData>(c.second);
            LOG("ParSensitivityAnalysis: Discount curve ccy=" << ccy);
            Size n_ten = data.shiftTenors.size();
            QL_REQUIRE(data.parInstruments.size() == n_ten,
                       "number of tenors does not match number of discount curve par instruments, "
                           << data.parInstruments.size() << " vs. " << n_ten << " ccy=" << ccy
                           << ", check sensitivity configuration.");
            for (Size j = 0; j < n_ten; ++j) {
                RiskFactorKey key(RiskFactorKey::KeyType::DiscountCurve, ccy, j);
                if (!dryRun && !relevantRiskFactors.empty() &&
                    relevantRiskFactors.find(key) == relevantRiskFactors.end())
                    continue;
                Period term = data.shiftTenors[j];
                string instType = data.parInstruments[j];
                bool singleCurve = data.parInstrumentSingleCurve;
                string indexName = "";               // if empty, it will be picked from conventions
                string yieldCurveName = "";          // ignored, if empty
                string equityForecastCurveName = ""; // ignored, if empty
                std::pair<QuantLib::ext::shared_ptr<Instrument>, Date> ret;
                bool recognised = true, skipped = false;
                try {
                    map<string, string> conventionsMap = data.parInstrumentConventions;
                    QL_REQUIRE(conventionsMap.find(instType) != conventionsMap.end(),
                               "conventions not found for ccy " << ccy << " and instrument type " << instType);
                    QuantLib::ext::shared_ptr<Convention> convention = conventions->get(conventionsMap[instType]);
                    QL_REQUIRE(convention != nullptr, "convention is empty");
                    if (instType == "IRS")
                        ret = makeSwap(simMarket, ccy, indexName, yieldCurveName, equityForecastCurveName, term,
                                       convention, singleCurve, parHelperDependencies_[key],
                                       instruments.removeTodaysFixingIndices_, data.discountCurve, marketConfiguration);
                    else if (instType == "DEP")
                        ret = makeDeposit(asof, simMarket, ccy, indexName, yieldCurveName, equityForecastCurveName,
                                          term, convention, marketConfiguration);
                    else if (instType == "FRA")
                        ret = makeFRA(asof, simMarket, ccy, indexName, yieldCurveName, equityForecastCurveName, term,
                                      convention, marketConfiguration);
                    else if (instType == "OIS")
                        ret = makeOIS(simMarket, ccy, indexName, yieldCurveName, equityForecastCurveName, term,
                                      convention, singleCurve, parHelperDependencies_[key],
                                      instruments.removeTodaysFixingIndices_, data.discountCurve, marketConfiguration);
                    else if (instType == "XBS") {
                        string otherCurrency =
                            data.otherCurrency.empty() ? simMarketParams->baseCcy() : data.otherCurrency;
                        ret = makeCrossCcyBasisSwap(simMarket, otherCurrency, ccy, term, convention,
                                                    parHelperDependencies_[key], instruments.removeTodaysFixingIndices_,
                                                    marketConfiguration);
                    } else if (instType == "FXF")
                        ret = makeFxForward(simMarket, simMarketParams->baseCcy(), ccy, term, convention,
                                            parHelperDependencies_[key], marketConfiguration);
                    else if (instType == "TBS")
                        ret = makeTenorBasisSwap(asof, simMarket, ccy, "", "", "", "", term, convention, singleCurve,
                                                 parHelperDependencies_[key], instruments.removeTodaysFixingIndices_,
                                                 data.discountCurve, marketConfiguration);
                    else
                        recognised = false;
                } catch (const std::exception& e) {
                    skipped = true;
                    if (continueOnError) {
                        StructuredAnalyticsErrorMessage("Par sensitivity conversion",
                                                        "Skipping par instrument for discount curve " + ccy, e.what())
                            .log();
                    } else {
                        QL_FAIL(e.what());
                    }
                }
                if (!recognised)
                    QL_FAIL("Instrument type " << instType << " for par sensitivity conversion not recognised");
                if (!skipped) {
                    parHelpers_[key] = ret.first;
                    if (!simMarket) {
                        yieldCurvePillars_[ccy].push_back((ret.second - asof) * Days);
                    }
                    DLOG("Par instrument for discount curve, ccy " << ccy << " tenor " << j << ", type " << instType
                                                                   << " built.");
                }
            }
        }
    }

    if (typesDisabled.count(RiskFactorKey::KeyType::YieldCurve) == 0) {

        LOG("ParSensitivityAnalysis: Yield curve par instruments");
        // Yield curve instruments
        QL_REQUIRE(simMarketParams->yieldCurveNames().size() == simMarketParams->yieldCurveCurrencies().size(),
                   "vector size mismatch in sim market parameters yield curve names/currencies");
        for (auto y : sensitivityData.yieldCurveShiftData()) {
            string curveName = y.first;
            QL_REQUIRE(simMarket || yieldCurvePillars_.find(curveName) == yieldCurvePillars_.end(),
                       "duplicate entry in yieldCurvePillars '" << curveName << "'");
            string equityForecastCurveName = ""; // ignored, if empty
            string ccy = "";
            for (Size j = 0; j < simMarketParams->yieldCurveNames().size(); ++j) {
                if (curveName == simMarketParams->yieldCurveNames()[j])
                    ccy = simMarketParams->yieldCurveCurrencies().at(curveName);
            }
            LOG("ParSensitivityAnalysis: yield curve name " << curveName);
            QL_REQUIRE(ccy != "", "yield curve currency not found for yield curve " << curveName);
            SensitivityScenarioData::CurveShiftParData data =
                *QuantLib::ext::static_pointer_cast<SensitivityScenarioData::CurveShiftParData>(y.second);
            Size n_ten = data.shiftTenors.size();
            QL_REQUIRE(data.parInstruments.size() == n_ten,
                       "number of tenors does not match number of yield curve par instruments");
            for (Size j = 0; j < n_ten; ++j) {
                RiskFactorKey key(RiskFactorKey::KeyType::YieldCurve, curveName, j);
                if (!dryRun && !relevantRiskFactors.empty() &&
                    relevantRiskFactors.find(key) == relevantRiskFactors.end())
                    continue;
                Period term = data.shiftTenors[j];
                string instType = data.parInstruments[j];
                bool singleCurve = data.parInstrumentSingleCurve;
                std::pair<QuantLib::ext::shared_ptr<Instrument>, Date> ret;
                bool recognised = true, skipped = false;
                try {
                    map<string, string> conventionsMap = data.parInstrumentConventions;
                    QL_REQUIRE(conventionsMap.find(instType) != conventionsMap.end(),
                               "conventions not found for ccy " << ccy << " and instrument type " << instType);
                    QuantLib::ext::shared_ptr<Convention> convention = conventions->get(conventionsMap[instType]);

                    if (instType == "IRS")
                        ret = makeSwap(simMarket, ccy, "", curveName, equityForecastCurveName, term, convention,
                                       singleCurve, parHelperDependencies_[key], instruments.removeTodaysFixingIndices_,
                                       data.discountCurve, marketConfiguration);
                    else if (instType == "DEP")
                        ret = makeDeposit(asof, simMarket, ccy, "", curveName, equityForecastCurveName, term,
                                          convention, marketConfiguration);
                    else if (instType == "FRA")
                        ret = makeFRA(asof, simMarket, ccy, "", curveName, equityForecastCurveName, term, convention,
                                      marketConfiguration);
                    else if (instType == "OIS")
                        ret = makeOIS(simMarket, ccy, "", curveName, equityForecastCurveName, term, convention,
                                      singleCurve, parHelperDependencies_[key], instruments.removeTodaysFixingIndices_,
                                      data.discountCurve, marketConfiguration);
                    else if (instType == "TBS")
                        ret = makeTenorBasisSwap(asof, simMarket, ccy, "", "", curveName, "", term, convention,
                                                 singleCurve, parHelperDependencies_[key],
                                                 instruments.removeTodaysFixingIndices_, data.discountCurve,
                                                 marketConfiguration);
                    else if (instType == "XBS") {
                        string otherCurrency =
                            data.otherCurrency.empty() ? simMarketParams->baseCcy() : data.otherCurrency;
                        ret = makeCrossCcyBasisSwap(simMarket, otherCurrency, ccy, term, convention,
                                                    parHelperDependencies_[key], instruments.removeTodaysFixingIndices_,
                                                    marketConfiguration);
                    } else
                        recognised = false;
                } catch (const std::exception& e) {
                    skipped = true;
                    if (continueOnError) {
                        StructuredAnalyticsErrorMessage("Par sensitivity conversion",
                                                        "Skipping par instrument for " + curveName, e.what())
                            .log();
                    } else {
                        QL_FAIL(e.what());
                    }
                }
                if (!recognised)
                    QL_FAIL("Instrument type " << instType << " for par sensitivity conversion unexpected");
                if (!skipped) {
                    parHelpers_[key] = ret.first;
                    if (!simMarket) {
                        yieldCurvePillars_[curveName].push_back((ret.second - asof) * Days);
                    }
                    DLOG("Par instrument for yield curve, ccy " << ccy << " tenor " << j << ", type " << instType
                                                                << " built.");
                }
            }
        }
    }

    if (typesDisabled.count(RiskFactorKey::KeyType::IndexCurve) == 0) {

        LOG("ParSensitivityAnalysis: Index curve par instruments");
        // Index curve instruments
        for (auto index : sensitivityData.indexCurveShiftData()) {
            string indexName = index.first;
            QL_REQUIRE(simMarket || yieldCurvePillars_.find(indexName) == yieldCurvePillars_.end(),
                       "duplicate entry in yieldCurvePillars '" << indexName << "'");
            SensitivityScenarioData::CurveShiftParData data =
                *QuantLib::ext::static_pointer_cast<SensitivityScenarioData::CurveShiftParData>(index.second);
            Size n_ten = data.shiftTenors.size();
            QL_REQUIRE(data.parInstruments.size() == n_ten,
                       indexName  << " number of tenors " << n_ten << "does not match number of index curve par instruments"
                                                     << data.parInstruments.size());
            vector<string> tokens;
            boost::split(tokens, indexName, boost::is_any_of("-"));
            QL_REQUIRE(tokens.size() >= 2, "index name " << indexName << " unexpected");
            string ccy = tokens[0];
            QL_REQUIRE(ccy.length() == 3, "currency token not recognised");
            for (Size j = 0; j < n_ten; ++j) {
                RiskFactorKey key(RiskFactorKey::KeyType::IndexCurve, indexName, j);
                if (!dryRun && !relevantRiskFactors.empty() &&
                    relevantRiskFactors.find(key) == relevantRiskFactors.end())
                    continue;
                Period term = data.shiftTenors[j];
                string instType = data.parInstruments[j];
                bool singleCurve = data.parInstrumentSingleCurve;
                string yieldCurveName = "";
                string equityForecastCurveName = ""; // ignored, if empty
                std::pair<QuantLib::ext::shared_ptr<Instrument>, Date> ret;
                bool recognised = true, skipped = false;
                try {
                    map<string, string> conventionsMap = data.parInstrumentConventions;
                    QL_REQUIRE(conventionsMap.find(instType) != conventionsMap.end(),
                               "conventions not found for ccy " << ccy << " and instrument type " << instType);
                    QuantLib::ext::shared_ptr<Convention> convention = conventions->get(conventionsMap[instType]);

                    if (instType == "IRS")
                        ret = makeSwap(simMarket, ccy, indexName, yieldCurveName, equityForecastCurveName, term,
                                       convention, singleCurve, parHelperDependencies_[key],
                                       instruments.removeTodaysFixingIndices_, data.discountCurve, marketConfiguration);
                    else if (instType == "DEP")
                        ret = makeDeposit(asof, simMarket, ccy, indexName, yieldCurveName, equityForecastCurveName,
                                          term, convention, marketConfiguration);
                    else if (instType == "FRA")
                        ret = makeFRA(asof, simMarket, ccy, indexName, yieldCurveName, equityForecastCurveName, term,
                                      convention, marketConfiguration);
                    else if (instType == "OIS")
                        ret = makeOIS(simMarket, ccy, indexName, yieldCurveName, equityForecastCurveName, term,
                                      convention, singleCurve, parHelperDependencies_[key],
                                      instruments.removeTodaysFixingIndices_, data.discountCurve, marketConfiguration);
                    else if (instType == "TBS")
                        ret = makeTenorBasisSwap(asof, simMarket, ccy, "", "", "", "", term, convention, singleCurve,
                                                 parHelperDependencies_[key], instruments.removeTodaysFixingIndices_,
                                                 data.discountCurve, marketConfiguration);
                    else
                        recognised = false;
                } catch (const std::exception& e) {
                    skipped = true;
                    if (continueOnError) {
                        StructuredAnalyticsErrorMessage("Par sensitivity conversion",
                                                        "Skipping par instrument for index curve " + indexName,
                                                        e.what())
                            .log();
                    } else {
                        QL_FAIL(e.what());
                    }
                }
                if (!recognised)
                    QL_FAIL("Instrument type " << instType << " for par sensitivity conversion not recognised");
                if (!skipped) {
                    parHelpers_[key] = ret.first;
                    if (!simMarket) {
                        yieldCurvePillars_[indexName].push_back((ret.second - asof) * Days);
                    }
                    DLOG("Par instrument for index " << indexName << " ccy " << ccy << " tenor " << j << " built.");
                }
            }
        }
    }

    if (typesDisabled.count(RiskFactorKey::KeyType::OptionletVolatility) == 0) {

        // Caps/Floors
        LOG("ParSensitivityAnalysis: Cap/Floor par instruments");

        for (auto c : sensitivityData.capFloorVolShiftData()) {
            string key = c.first;
            auto datap = QuantLib::ext::dynamic_pointer_cast<SensitivityScenarioData::CapFloorVolShiftParData>(c.second);
            string expDiscountCurve = datap ? datap->discountCurve : "";
            SensitivityScenarioData::CapFloorVolShiftData data = *c.second;
            string indexName = data.indexName;
            string ccy = parseIborIndex(indexName)->currency().code();
            Handle<YieldTermStructure> yts;
            Handle<OptionletVolatilityStructure> ovs;
            Size n_strikes = data.shiftStrikes.size();
            Size n_expiries = data.shiftExpiries.size();

            // Determine if the cap floor is ATM
            bool isAtm = data.shiftStrikes.size() == 1 && data.shiftStrikes[0] == 0.0 && data.isRelative;

            for (Size j = 0; j < n_strikes; ++j) {
                Real strike = data.shiftStrikes[j];
                for (Size k = 0; k < n_expiries; ++k) {
                    RiskFactorKey rfkey(RiskFactorKey::KeyType::OptionletVolatility, key, k * n_strikes + j);
                    if (!dryRun && !relevantRiskFactors.empty() &&
                        relevantRiskFactors.find(rfkey) == relevantRiskFactors.end())
                        continue;
                    try {
                        if (simMarket != nullptr) {
                            yts = expDiscountCurve.empty() ? simMarket->discountCurve(ccy, marketConfiguration)
                                                           : simMarket->iborIndex(expDiscountCurve, marketConfiguration)
                                                                 ->forwardingTermStructure();
                            ovs = simMarket->capFloorVol(key, marketConfiguration);
                        }
                        Period term = data.shiftExpiries[k];
                        auto tmp = makeCapFloor(simMarket, ccy, indexName, term, strike, isAtm,
                                                parHelperDependencies_[rfkey], expDiscountCurve, marketConfiguration);
                        parCaps_[rfkey] = tmp;
                        parCapsYts_[rfkey] = yts;
                        parCapsVts_[rfkey] = ovs;
                        if (j == 0)
                            capFloorPillars_[key].push_back(term);
                        DLOG("Par cap/floor for key " << rfkey << " strike " << j << " tenor " << k << " built.");
                    } catch (const std::exception& e) {
                        if (continueOnError) {
                            StructuredAnalyticsErrorMessage("Par sensitivity conversion",
                                                            "Skipping par cap/floor for key " + key, e.what())
                                .log();
                        } else {
                            QL_FAIL(e.what());
                        }
                    }
                }
            }
        }
    }

    if (typesDisabled.count(RiskFactorKey::KeyType::SurvivalProbability) == 0) {

        // CDS Instruments
        LOG("ParSensitivityAnalysis: CDS par instruments");
        for (auto c : sensitivityData.creditCurveShiftData()) {
            string name = c.first;
            string ccy = sensitivityData.creditCcys().at(name);
            auto itr = sensitivityData.creditCurveShiftData().find(name);
            QL_REQUIRE(itr != sensitivityData.creditCurveShiftData().end(),
                       "creditCurveShiftData not found for " << name);
            SensitivityScenarioData::CurveShiftParData data =
                *QuantLib::ext::static_pointer_cast<SensitivityScenarioData::CurveShiftParData>(c.second);
            Size n_expiries = data.shiftTenors.size();
            for (Size k = 0; k < n_expiries; ++k) {
                string instType = data.parInstruments[k];
                RiskFactorKey key(RiskFactorKey::KeyType::SurvivalProbability, name, k);
                if (!dryRun && !relevantRiskFactors.empty() &&
                    relevantRiskFactors.find(key) == relevantRiskFactors.end())
                    continue;
                Period term = data.shiftTenors[k];
                std::pair<QuantLib::ext::shared_ptr<Instrument>, Date> ret;
                bool skipped = false;
                try {
                    map<string, string> conventionsMap = data.parInstrumentConventions;
                    QL_REQUIRE(conventionsMap.find(instType) != conventionsMap.end(),
                               "conventions not found for name " << name << " and instrument type " << instType);
                    QuantLib::ext::shared_ptr<Convention> convention = conventions->get(conventionsMap[instType]);

                    ret = makeCDS(simMarket, name, ccy, term, convention, parHelperDependencies_[key],
                                  data.discountCurve, marketConfiguration);
                } catch (const std::exception& e) {
                    skipped = true;
                    if (continueOnError) {
                        StructuredAnalyticsErrorMessage("Par sensitivity conversion",
                                                        "Skipping par instrument for cds " + name, e.what())
                            .log();
                    } else {
                        QL_FAIL(e.what());
                    }
                }
                if (!skipped) {
                    parHelpers_[key] = ret.first;
                    if (!simMarket) {
                        cdsPillars_[name].push_back((ret.second - asof) * Days);
                    }
                    DLOG("Par CDS for name " << name << " tenor " << k << " built.");
                }
            }
        }
    }

    if (typesDisabled.count(RiskFactorKey::KeyType::ZeroInflationCurve) == 0) {

        LOG("ParSensitivityAnalysis: ZCI curve par instruments");
        // Zero Inflation Curve instruments
        for (auto z : sensitivityData.zeroInflationCurveShiftData()) {
            string indexName = z.first;
            SensitivityScenarioData::CurveShiftParData data =
                *QuantLib::ext::static_pointer_cast<SensitivityScenarioData::CurveShiftParData>(z.second);
            Size n_ten = data.shiftTenors.size();
            for (Size j = 0; j < n_ten; ++j) {
                RiskFactorKey key(RiskFactorKey::KeyType::ZeroInflationCurve, indexName, j);
                if (!dryRun && !relevantRiskFactors.empty() &&
                    relevantRiskFactors.find(key) == relevantRiskFactors.end())
                    continue;
                Period term = data.shiftTenors[j];
                string instType = data.parInstruments[j];
                bool singleCurve = data.parInstrumentSingleCurve;
                try {
                    map<string, string> conventionsMap = data.parInstrumentConventions;
                    QL_REQUIRE(conventionsMap.find(instType) != conventionsMap.end(),
                               "conventions not found for zero inflation curve " << indexName << " and instrument type "
                                                                                 << instType);
                    QuantLib::ext::shared_ptr<Convention> convention = conventions->get(conventionsMap[instType]);

                    auto tmp =
                        makeZeroInflationSwap(simMarket, indexName, term, convention, singleCurve,
                                              parHelperDependencies_[key], data.discountCurve, marketConfiguration);
                    auto helper = QuantLib::ext::dynamic_pointer_cast<ZeroCouponInflationSwap>(tmp);
                    QuantLib::ext::shared_ptr<IndexedCashFlow> lastCoupon =
                        QuantLib::ext::dynamic_pointer_cast<IndexedCashFlow>(helper->inflationLeg().back());
                    Date latestRelevantDate = std::max(helper->maturityDate(), lastCoupon->fixingDate());
                    zeroInflationPillars_[indexName].push_back((latestRelevantDate - asof) * Days);

                    parHelpers_[key] = tmp;
                    DLOG("Par instrument for zero inflation index " << indexName << " tenor " << j << " built.");
                } catch (const std::exception& e) {
                    if (continueOnError) {
                        StructuredAnalyticsErrorMessage("Par sensitivity conversion",
                                                        "Skipping par instrument for zero inflation index " + indexName,
                                                        e.what())
                            .log();
                    } else {
                        QL_FAIL(e.what());
                    }
                }
            }
        }
    }

    if (typesDisabled.count(RiskFactorKey::KeyType::YoYInflationCurve) == 0) {

        // YoY Inflation Curve instruments
        LOG("ParSensitivityAnalysis: YOYI curve par instruments");
        for (auto y : sensitivityData.yoyInflationCurveShiftData()) {
            string indexName = y.first;
            SensitivityScenarioData::CurveShiftParData data =
                *QuantLib::ext::static_pointer_cast<SensitivityScenarioData::CurveShiftParData>(y.second);
            Size n_ten = data.shiftTenors.size();
            for (Size j = 0; j < n_ten; ++j) {
                Period term = data.shiftTenors[j];
                string instType = data.parInstruments[j];
                bool singleCurve = data.parInstrumentSingleCurve;

                RiskFactorKey key(RiskFactorKey::KeyType::YoYInflationCurve, indexName, j);
                bool recognised = true;
                try {
                    map<string, string> conventionsMap = data.parInstrumentConventions;
                    QL_REQUIRE(conventionsMap.find(instType) != conventionsMap.end(),
                               "conventions not found for zero inflation curve " << indexName << " and instrument type "
                                                                                 << instType);
                    QuantLib::ext::shared_ptr<Convention> convention = conventions->get(conventionsMap[instType]);

                    if (instType == "ZIS") {
                        auto tmp =
                            makeYoyInflationSwap(simMarket, indexName, term, convention, singleCurve, true,
                                                 parHelperDependencies_[key], data.discountCurve, marketConfiguration);
                        auto helper = dynamic_pointer_cast<YearOnYearInflationSwap>(tmp);
                        // set pillar date
                        QuantLib::ext::shared_ptr<YoYInflationCoupon> lastCoupon =
                            QuantLib::ext::dynamic_pointer_cast<YoYInflationCoupon>(helper->yoyLeg().back());
                        Date latestRelevantDate = std::max(helper->maturityDate(), lastCoupon->fixingDate());
                        instruments.yoyInflationPillars_[indexName].push_back((latestRelevantDate - asof) * Days);
                        parHelpers_[key] = tmp;
                    } else if (instType == "YYS") {
                        auto tmp =
                            makeYoyInflationSwap(simMarket, indexName, term, convention, singleCurve, false,
                                                 parHelperDependencies_[key], data.discountCurve, marketConfiguration);
                        auto helper = dynamic_pointer_cast<YearOnYearInflationSwap>(tmp);
                        // set pillar date
                        QuantLib::ext::shared_ptr<YoYInflationCoupon> lastCoupon =
                            QuantLib::ext::dynamic_pointer_cast<YoYInflationCoupon>(helper->yoyLeg().back());
                        Date latestRelevantDate = std::max(helper->maturityDate(), lastCoupon->fixingDate());
                        instruments.yoyInflationPillars_[indexName].push_back((latestRelevantDate - asof) * Days);
                        parHelpers_[key] = tmp;
                    } else
                        recognised = false;
                } catch (const std::exception& e) {
                    if (continueOnError) {
                        StructuredAnalyticsErrorMessage("Par sensitivity conversion",
                                                        "Skipping par instrument for yoy index " + indexName, e.what())
                            .log();
                    } else {
                        QL_FAIL(e.what());
                    }
                }
                if (!recognised)
                    QL_FAIL("Instrument type " << instType << " for par sensitivity conversion not recognised");
                DLOG("Par instrument for yoy inflation index " << indexName << " tenor " << j << " built.");
            }
        }
    }

    if (typesDisabled.count(RiskFactorKey::KeyType::YoYInflationCapFloorVolatility) == 0) {

        // YY Caps/Floors
        LOG("ParSensitivityAnalysis: YOYI Cap/Floor par instruments");
        for (auto y : sensitivityData.yoyInflationCapFloorVolShiftData()) {
            string indexName = y.first;
            SensitivityScenarioData::CapFloorVolShiftParData data =
                *QuantLib::ext::static_pointer_cast<SensitivityScenarioData::CapFloorVolShiftParData>(y.second);
            Size n_strikes = data.shiftStrikes.size();
            Size n_expiries = data.shiftExpiries.size();
            bool singleCurve = data.parInstrumentSingleCurve;
            for (Size j = 0; j < n_strikes; ++j) {
                Real strike = data.shiftStrikes[j];
                pair<string, Size> key(indexName, j);
                for (Size k = 0; k < n_expiries; ++k) {
                    RiskFactorKey key(RiskFactorKey::KeyType::YoYInflationCapFloorVolatility, indexName,
                                      k * n_strikes + j);

                    bool recognised = true;
                    string instType;
                    try {
                        instType = data.parInstruments[j];
                        map<string, string> conventionsMap = data.parInstrumentConventions;
                        QL_REQUIRE(conventionsMap.find(instType) != conventionsMap.end(),
                                   "conventions not found for zero inflation curve "
                                       << indexName << " and instrument type " << instType);
                        QuantLib::ext::shared_ptr<Convention> convention = conventions->get(conventionsMap[instType]);
                        Period term = data.shiftExpiries[k];
                        if (instType == "ZIS") {
                            makeYoYCapFloor(instruments, simMarket, indexName, term, strike, convention, singleCurve,
                                            true, data.discountCurve, key, marketConfiguration);
                        } else if (instType == "YYS") {
                            makeYoYCapFloor(instruments, simMarket, indexName, term, strike, convention, singleCurve,
                                            false, data.discountCurve, key, marketConfiguration);
                        } else
                            recognised = false;
                        if (j == 0)
                            yoyCapFloorPillars_[indexName].push_back(term);
                    } catch (const std::exception& e) {
                        if (continueOnError) {
                            StructuredAnalyticsErrorMessage(
                                "Par sensitivity conversion",
                                "Skipping par instrument for yoy cap floor index " + indexName, e.what())
                                .log();
                        } else {
                            QL_FAIL(e.what());
                        }
                    }
                    if (!recognised)
                        QL_FAIL("Instrument type " << instType << " for par sensitivity conversion not recognised");
                    DLOG("Par yoy cap/floor for index " << indexName << " strike " << j << " tenor " << k << " built.");
                }
            }
        }
    }

    LOG("Par instrument building done, got " << parHelpers_.size() + parCaps_.size() + parYoYCaps_.size()
                                             << " instruments");
} // createParInstruments

std::pair<QuantLib::ext::shared_ptr<Instrument>, Date> ParSensitivityInstrumentBuilder::makeSwap(
    const QuantLib::ext::shared_ptr<Market>& market, string ccy, string indexName, string yieldCurveName,
    string equityForecastCurveName, Period term, const QuantLib::ext::shared_ptr<Convention>& convention, bool singleCurve,
    std::set<ore::analytics::RiskFactorKey>& parHelperDependencies_, std::set<std::string>& removeTodaysFixingIndices,
    const string& expDiscountCurve, const string& marketConfiguration) const {
    // Curve priorities, use in the following order if ccy/indexName/yieldCurveName strings are not blank
    // 1) singleCurve = false
    //    - discounts: discountCurve(ccy) -> yieldCurve(yieldCurveName)
    //    - forwards:  iborIndex(indexName) -> iborIndex(conventions index name)
    // 2) singleCurve = true
    //    - discounts: iborIndex(indexName) -> yieldCurve(yieldCurveName) -> discountCurve(ccy)
    //    - forwards:  iborIndex(indexName) -> yieldCurve(yieldCurveName) -> discountCurve(ccy)
    QuantLib::ext::shared_ptr<Conventions> conventions = InstrumentConventions::instance().conventions();
    QuantLib::ext::shared_ptr<IRSwapConvention> conv = QuantLib::ext::dynamic_pointer_cast<IRSwapConvention>(convention);
    QL_REQUIRE(conv, "convention not recognised, expected IRSwapConvention");
    string name = indexName != "" ? indexName : conv->indexName();
    QuantLib::ext::shared_ptr<IborIndex> index;
    Handle<YieldTermStructure> discountCurve;
    if (market == nullptr) {
        index = parseIborIndex(name);
    } else {
        if (!expDiscountCurve.empty()) {
            // Look up the explicit discount curve in the market
            QuantLib::ext::shared_ptr<IborIndex> dummyIndex;
            if (tryParseIborIndex(expDiscountCurve, dummyIndex)) {
                auto discountIndex = market->iborIndex(expDiscountCurve, marketConfiguration);
                discountCurve = discountIndex->forwardingTermStructure();
            } else {
                discountCurve = market->yieldCurve(expDiscountCurve, marketConfiguration);
            }
        } else if (ccy != "")
            discountCurve = market->discountCurve(ccy, marketConfiguration);
        else if (yieldCurveName != "")
            discountCurve = market->yieldCurve(yieldCurveName, marketConfiguration);
        else if (equityForecastCurveName != "")
            discountCurve = market->equityForecastCurve(equityForecastCurveName, marketConfiguration);

        index = *market->iborIndex(name, marketConfiguration);

        if (singleCurve) {
            if (indexName != "")
                discountCurve = index->forwardingTermStructure();
            else if (yieldCurveName != "") {
                index = index->clone(market->yieldCurve(yieldCurveName, marketConfiguration));
                discountCurve = market->yieldCurve(yieldCurveName, marketConfiguration);
            } else if (ccy != "")
                index = index->clone(market->discountCurve(ccy, marketConfiguration));
            else if (equityForecastCurveName != "") {
                index = index->clone(market->equityForecastCurve(equityForecastCurveName, marketConfiguration));
                discountCurve = market->equityForecastCurve(equityForecastCurveName, marketConfiguration);
            } else
                QL_FAIL("Discount curve undetermined for Swap (ccy=" << ccy << ")");
        }
    }

    if (!singleCurve)
        parHelperDependencies_.emplace(RiskFactorKey::KeyType::IndexCurve, name, 0);

    QuantLib::ext::shared_ptr<Swap> helper;
    Date latestRelevantDate;

    auto bmaIndex = QuantLib::ext::dynamic_pointer_cast<QuantExt::BMAIndexWrapper>(index);
    if (bmaIndex) {
        // FIXME do we want to remove today's historic fixing from the index as we do for the Ibor case?
        helper = QuantLib::ext::shared_ptr<FixedBMASwap>(
            MakeFixedBMASwap(term, bmaIndex->bma(), 0.0, 0 * Days).withBMALegTenor(3 * Months));
        // need to do very little with the factory, as the market conventions are default
        // should maybe discount with Libor, as this is how we assume the quotes come in.
        QuantLib::ext::shared_ptr<AverageBMACoupon> lastCoupon =
            QuantLib::ext::dynamic_pointer_cast<AverageBMACoupon>(helper->leg(1).back());
        latestRelevantDate = std::max(helper->maturityDate(), lastCoupon->fixingDates().end()[-2]);
    } else if (conv->hasSubPeriod()) {
        removeTodaysFixingIndices.insert(index->name());
        auto subPeriodSwap = QuantLib::ext::shared_ptr<SubPeriodsSwap>(
            MakeSubPeriodsSwap(term, index, 0.0, Period(conv->floatFrequency()), 0 * Days)
                .withSettlementDays(index->fixingDays())
                .withFixedLegDayCount(conv->fixedDayCounter())
                .withFixedLegTenor(Period(conv->fixedFrequency()))
                .withFixedLegConvention(conv->fixedConvention())
                .withFixedLegCalendar(conv->fixedCalendar())
                .withSubCouponsType(conv->subPeriodsCouponType()));

        latestRelevantDate = subPeriodSwap->maturityDate();
        QuantLib::ext::shared_ptr<FloatingRateCoupon> lastCoupon =
            QuantLib::ext::dynamic_pointer_cast<FloatingRateCoupon>(subPeriodSwap->floatLeg().back());
        helper = subPeriodSwap;
        if (IborCoupon::Settings::instance().usingAtParCoupons()) {
            /* Subperiods coupons do not have a par approximation either... */
            if (QuantLib::ext::dynamic_pointer_cast<QuantExt::SubPeriodsCoupon1>(lastCoupon)) {
                Date fixingValueDate = index->valueDate(lastCoupon->fixingDate());
                Date endValueDate = index->maturityDate(fixingValueDate);
                latestRelevantDate = std::max(latestRelevantDate, endValueDate);
            }
        } else {
            /* May need to adjust latestRelevantDate if you are projecting libor based
            on tenor length rather than from accrual date to accrual date. */
            Date fixingValueDate = index->valueDate(lastCoupon->fixingDate());
            Date endValueDate = index->maturityDate(fixingValueDate);
            latestRelevantDate = std::max(latestRelevantDate, endValueDate);
        }
    } else {
        removeTodaysFixingIndices.insert(index->name());
        helper = QuantLib::ext::shared_ptr<VanillaSwap>(MakeVanillaSwap(term, index, 0.0, 0 * Days)
                                                    .withSettlementDays(index->fixingDays())
                                                    .withFixedLegDayCount(conv->fixedDayCounter())
                                                    .withFixedLegTenor(Period(conv->fixedFrequency()))
                                                    .withFixedLegConvention(conv->fixedConvention())
                                                    .withFixedLegTerminationDateConvention(conv->fixedConvention())
                                                    .withFixedLegCalendar(conv->fixedCalendar())
                                                    .withFloatingLegCalendar(conv->fixedCalendar()));
        QuantLib::ext::shared_ptr<IborCoupon> lastCoupon = QuantLib::ext::dynamic_pointer_cast<IborCoupon>(helper->leg(1).back());
        latestRelevantDate = std::max(helper->maturityDate(), lastCoupon->fixingEndDate());
    }

    if (market) {
        QuantLib::ext::shared_ptr<PricingEngine> swapEngine = QuantLib::ext::make_shared<DiscountingSwapEngine>(discountCurve);
        helper->setPricingEngine(swapEngine);
    }

    // set pillar date
    return std::pair<QuantLib::ext::shared_ptr<Instrument>, Date>(helper, latestRelevantDate);
}

std::pair<QuantLib::ext::shared_ptr<Instrument>, Date> ParSensitivityInstrumentBuilder::makeDeposit(
    const QuantLib::Date& asof, const QuantLib::ext::shared_ptr<Market>& market, string ccy, string indexName,
    string yieldCurveName, string equityForecastCurveName, Period term, const QuantLib::ext::shared_ptr<Convention>& convention,
    const string& marketConfiguration) const {

    // Curve priorities, use in the following order if ccy/indexName/yieldCurveName strings are not blank
    // Single curve setting only
    // - discounts: iborIndex(indexName) -> yieldCurve(yieldCurveName) -> discountCurve(ccy)
    // - forwards:  iborIndex(indexName) -> yieldCurve(yieldCurveName) -> discountCurve(ccy)
    QuantLib::ext::shared_ptr<Conventions> conventions = InstrumentConventions::instance().conventions();
    QuantLib::ext::shared_ptr<DepositConvention> conv = QuantLib::ext::dynamic_pointer_cast<DepositConvention>(convention);
    QL_REQUIRE(conv, "convention not recognised, expected DepositConvention");
    QuantLib::ext::shared_ptr<IborIndex> index;
    if (indexName == "" && conv->indexBased()) {
        // At this point, we may have an overnight index or an ibor index
        if (isOvernightIndex(conv->index())) {
            index = parseIborIndex(conv->index());
        } else {
            index = parseIborIndex(conv->index() + "-" + to_string(term));
        }
    } else if (indexName != "") {
        if (market != nullptr) {
            index = market->iborIndex(indexName, marketConfiguration).currentLink();
        } else {
            index = parseIborIndex(indexName);
        }
    }
    QuantLib::ext::shared_ptr<Deposit> helper;
    if (index != nullptr) {
        helper = QuantLib::ext::make_shared<Deposit>(1.0, 0.0, term, index->fixingDays(), index->fixingCalendar(),
                                             index->businessDayConvention(), index->endOfMonth(), index->dayCounter(),
                                             asof, true, 0 * Days);
    } else {
        QL_REQUIRE(!conv->indexBased(), "expected non-index-based deposit convention");
        helper = QuantLib::ext::make_shared<Deposit>(1.0, 0.0, term, conv->settlementDays(), conv->calendar(),
                                             conv->convention(), conv->eom(), conv->dayCounter(), asof, true, 0 * Days);
    }
    RelinkableHandle<YieldTermStructure> engineYts;
    QuantLib::ext::shared_ptr<PricingEngine> depositEngine = QuantLib::ext::make_shared<DepositEngine>(engineYts);
    helper->setPricingEngine(depositEngine);
    if (market != nullptr) {
        if (indexName != "")
            engineYts.linkTo(*index->forwardingTermStructure());
        else if (yieldCurveName != "")
            engineYts.linkTo(*market->yieldCurve(yieldCurveName, marketConfiguration));
        else if (equityForecastCurveName != "")
            engineYts.linkTo(*market->equityForecastCurve(equityForecastCurveName, marketConfiguration));
        else if (ccy != "")
            engineYts.linkTo(*market->discountCurve(ccy, marketConfiguration));
        else
            QL_FAIL("Yield term structure not found for deposit (ccy=" << ccy << ")");
    }
    // set pillar date
    Date latestRelevantDate = helper->maturityDate();
    return std::pair<QuantLib::ext::shared_ptr<Instrument>, Date>(helper, latestRelevantDate);
}

std::pair<QuantLib::ext::shared_ptr<Instrument>, Date> ParSensitivityInstrumentBuilder::makeFRA(
    const QuantLib::Date& asof, const QuantLib::ext::shared_ptr<Market>& market, string ccy, string indexName,
    string yieldCurveName, string equityForecastCurveName, Period term, const QuantLib::ext::shared_ptr<Convention>& convention,
    const string& marketConfiguration) const {
    // Curve priorities, use in the following order if ccy/indexName/yieldCurveName strings are not blank
    // - discounts: discountCurve(ccy) -> yieldCurve(yieldCurveName) -> iborIndex(indexName)
    // - forwards:  iborIndex(indexName) -> yieldCurve(yieldCurveName) -> discountCurve(ccy)
    QuantLib::ext::shared_ptr<Conventions> conventions = InstrumentConventions::instance().conventions();
    QuantLib::ext::shared_ptr<FraConvention> conv = QuantLib::ext::dynamic_pointer_cast<FraConvention>(convention);
    QL_REQUIRE(conv, "convention not recognised, expected FraConvention");
    string name = indexName != "" ? indexName : conv->indexName();
    QuantLib::ext::shared_ptr<IborIndex> index;
    if (market != nullptr) {
        index = *market->iborIndex(name, marketConfiguration);
        if (indexName == "") {
            if (yieldCurveName != "")
                index = market->iborIndex(name)->clone(market->yieldCurve(yieldCurveName, marketConfiguration));
            else if (equityForecastCurveName != "")
                index = market->iborIndex(name)->clone(
                    market->equityForecastCurve(equityForecastCurveName, marketConfiguration));
            else if (ccy != "")
                index = market->iborIndex(name)->clone(market->discountCurve(ccy, marketConfiguration));
            else
                QL_FAIL("index curve not identified for FRA (ccy=" << ccy << ")");
        }
    } else {
        index = parseIborIndex(name);
    }
    QuantLib::ext::shared_ptr<IborIndex> fraConvIdx =
        ore::data::parseIborIndex(conv->indexName(), index->forwardingTermStructure()); // used for setting up the FRA
    if (fraConvIdx->tenor() != index->tenor()) {
        WLOG("FRA building - mismatch between input index (" << indexName << ") and conventions (" << conv->indexName()
                                                             << ") - using conventions");
    }
    QL_REQUIRE((term.units() == Months) || (term.units() == Years), "term unit must be Months or Years");
    QL_REQUIRE(fraConvIdx->tenor().units() == Months, "index tenor unit must be Months (" << fraConvIdx->tenor() << ")("
                                                                                          << term << ")(" << indexName
                                                                                          << ")(" << name << ")");
    QL_REQUIRE(term > fraConvIdx->tenor(), "term must be larger than index tenor");
    Period startTerm = term - fraConvIdx->tenor(); // the input term refers to the end of the FRA accrual period
    Calendar fraCal = fraConvIdx->fixingCalendar();
    Date asofadj = fraCal.adjust(asof); // same as in FraRateHelper
    Date todaySpot = fraConvIdx->valueDate(asofadj);
    Date valueDate =
        fraCal.advance(todaySpot, startTerm, fraConvIdx->businessDayConvention(), fraConvIdx->endOfMonth());
    Date maturityDate = fraConvIdx->maturityDate(valueDate);
    Handle<YieldTermStructure> ytsTmp;
    if (market != nullptr) {
        if (ccy != "")
            ytsTmp = market->discountCurve(ccy, marketConfiguration);
        else if (yieldCurveName != "")
            ytsTmp = market->yieldCurve(yieldCurveName, marketConfiguration);
        else if (equityForecastCurveName != "")
            ytsTmp = market->equityForecastCurve(equityForecastCurveName, marketConfiguration);
        else
            ytsTmp = index->forwardingTermStructure();
    } else {
        // FRA instrument requires non-empty curves for its construction below
        ytsTmp = Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.00, Actual365Fixed()));
        fraConvIdx = fraConvIdx->clone(ytsTmp);
    }
    auto helper =
        QuantLib::ext::make_shared<QuantLib::ForwardRateAgreement>(fraConvIdx, valueDate, Position::Long, 0.0, 1.0, ytsTmp);
    // set pillar date
    // yieldCurvePillars_[indexName == "" ? ccy : indexName].push_back((maturityDate - asof) *
    // Days);
    return std::pair<QuantLib::ext::shared_ptr<Instrument>, Date>(helper, maturityDate);
}

std::pair<QuantLib::ext::shared_ptr<Instrument>, Date> ParSensitivityInstrumentBuilder::makeOIS(
    const QuantLib::ext::shared_ptr<Market>& market, string ccy, string indexName, string yieldCurveName,
    string equityForecastCurveName, Period term, const QuantLib::ext::shared_ptr<Convention>& convention, bool singleCurve,
    std::set<ore::analytics::RiskFactorKey>& parHelperDependencies_, std::set<std::string>& removeTodaysFixingIndices,
    const std::string& expDiscountCurve, const string& marketConfiguration) const {
    // Curve priorities, use in the following order if ccy/indexName/yieldCurveName strings are not blank
    // 1) singleCurve = false
    //    - discounts: discountCurve(ccy) -> yieldCurve(yieldCurveName)
    //    - forwards:  iborIndex(indexName) -> iborIndex(conventions index name)
    // 2) singleCurve = true
    //    - discounts: iborIndex(indexName) -> yieldCurve(yieldCurveName) -> discountCurve(ccy)
    //    - forwards:  iborIndex(indexName) -> yieldCurve(yieldCurveName) -> discountCurve(ccy)
    QuantLib::ext::shared_ptr<OisConvention> conv = QuantLib::ext::dynamic_pointer_cast<OisConvention>(convention);
    QL_REQUIRE(conv, "convention not recognised, expected OisConvention");
    QuantLib::ext::shared_ptr<IborIndex> index = parseIborIndex(conv->indexName());
    if (market == nullptr) {
        if (!expDiscountCurve.empty())
            parHelperDependencies_.emplace(RiskFactorKey::KeyType::IndexCurve, expDiscountCurve, 0);
        else
            parHelperDependencies_.emplace(RiskFactorKey::KeyType::DiscountCurve, ccy);
        if (!singleCurve)
            parHelperDependencies_.emplace(RiskFactorKey::KeyType::IndexCurve,
                                           indexName != "" ? indexName : conv->indexName(), 0);
    }
    QuantLib::ext::shared_ptr<OvernightIndex> overnightIndexTmp = QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(index);
    QL_REQUIRE(overnightIndexTmp,
               "ParSensitivityAnalysis::makeOIS(): expected OIS index, got  \"" << conv->indexName() << "\"");
    // makeOIS below requires non-empty ts
    Handle<YieldTermStructure> indexTs =
        Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.00, Actual365Fixed()));
    if (market != nullptr) {
        if (singleCurve) {
            if (indexName != "") {
                indexTs = market->iborIndex(indexName, marketConfiguration).currentLink()->forwardingTermStructure();
            } else if (yieldCurveName != "") {
                indexTs = market->yieldCurve(yieldCurveName, marketConfiguration);
            } else if (equityForecastCurveName != "") {
                indexTs = market->equityForecastCurve(equityForecastCurveName, marketConfiguration);
            } else if (ccy != "") {
                indexTs = market->discountCurve(ccy, marketConfiguration);
            } else {
                QL_FAIL("Index curve not identified in ParSensitivityAnalysis::makeOIS");
            }
        } else {
            indexTs = market->iborIndex(indexName != "" ? indexName : conv->indexName(), marketConfiguration)
                          .currentLink()
                          ->forwardingTermStructure();
        }
    }
    QuantLib::ext::shared_ptr<OvernightIndex> overnightIndex =
        QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(overnightIndexTmp->clone(indexTs));
    removeTodaysFixingIndices.insert(overnightIndex->name());
    QuantLib::ext::shared_ptr<OvernightIndexedSwap> helper =
        MakeOIS(term, overnightIndex, Null<Rate>(), 0 * Days).withTelescopicValueDates(true);

    if (market != nullptr) {
        RelinkableHandle<YieldTermStructure> engineYts;
        if (singleCurve) {
            if (indexName != "")
                engineYts.linkTo(*indexTs);
            else if (yieldCurveName != "")
                engineYts.linkTo(*market->yieldCurve(yieldCurveName, marketConfiguration));
            else if (equityForecastCurveName != "")
                engineYts.linkTo(*market->equityForecastCurve(equityForecastCurveName, marketConfiguration));
            else if (ccy != "")
                engineYts.linkTo(*market->discountCurve(ccy, marketConfiguration));
            else
                QL_FAIL("discount curve not identified in ParSensitivityAnalysis::makeOIS, single curve (ccy=" << ccy
                                                                                                               << ")");
        } else {
            if (!expDiscountCurve.empty()) {
                // Look up the explicit discount curve in the market
                auto discountIndex = market->iborIndex(expDiscountCurve, marketConfiguration);
                engineYts.linkTo(*discountIndex->forwardingTermStructure());
            } else if (ccy != "")
                engineYts.linkTo(*market->discountCurve(ccy, marketConfiguration));
            else if (yieldCurveName != "")
                engineYts.linkTo(*market->yieldCurve(yieldCurveName, marketConfiguration));
            else if (equityForecastCurveName != "")
                engineYts.linkTo(*market->equityForecastCurve(equityForecastCurveName, marketConfiguration));
            else
                QL_FAIL("discount curve not identified in ParSensitivityAnalysis::makeOIS, multi curve (ccy=" << ccy
                                                                                                              << ")");
        }
        QuantLib::ext::shared_ptr<PricingEngine> swapEngine = QuantLib::ext::make_shared<DiscountingSwapEngine>(engineYts);
        helper->setPricingEngine(swapEngine);
    }

    // set pillar date
    Date latestRelevantDate = helper->maturityDate();
    return std::pair<QuantLib::ext::shared_ptr<Instrument>, Date>(helper, latestRelevantDate);
}

std::pair<QuantLib::ext::shared_ptr<QuantLib::Instrument>, Date> ParSensitivityInstrumentBuilder::makeTenorBasisSwap(
    const QuantLib::Date& asof, const QuantLib::ext::shared_ptr<Market>& market, string ccy, string receiveIndexName,
    string payIndexName, string yieldCurveName, string equityForecastCurveName, Period term,
    const QuantLib::ext::shared_ptr<Convention>& convention, const bool singleCurve,
    std::set<ore::analytics::RiskFactorKey>& parHelperDependencies_, std::set<std::string>& removeTodaysFixingIndices,
    const string& expDiscountCurve, const string& marketConfiguration) const {

    QuantLib::ext::shared_ptr<TenorBasisSwapConvention> conv =
        QuantLib::ext::dynamic_pointer_cast<TenorBasisSwapConvention>(convention);
    QL_REQUIRE(conv, "convention not recognised, expected TenorBasisSwapConvention");
    Handle<YieldTermStructure> discountCurve, receiveIndexCurve, payIndexCurve;
    QuantLib::ext::shared_ptr<IborIndex> payIndex = parseIborIndex(conv->payIndexName());
    QuantLib::ext::shared_ptr<IborIndex> receiveIndex = parseIborIndex(conv->receiveIndexName());
    QuantLib::ext::shared_ptr<OvernightIndex> receiveIndexOn = QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(receiveIndex);
    QuantLib::ext::shared_ptr<OvernightIndex> payIndexOn = QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(payIndex);

    if (market != nullptr) {
        if (!expDiscountCurve.empty()) {
            // Look up the explicit discount curve in the market
            auto discountIndex = market->iborIndex(expDiscountCurve, marketConfiguration);
            discountCurve = discountIndex->forwardingTermStructure();
        } else if (ccy != "")
            discountCurve = market->discountCurve(ccy, marketConfiguration);
        else if (yieldCurveName != "")
            discountCurve = market->yieldCurve(yieldCurveName, marketConfiguration);
        else if (equityForecastCurveName != "")
            discountCurve = market->equityForecastCurve(equityForecastCurveName, marketConfiguration);
        else {
            QL_FAIL("tenor basis swap discount curve undetermined");
        }
        if (singleCurve)
            receiveIndexCurve = discountCurve;
        else
            receiveIndexCurve = market
                                    ->iborIndex(receiveIndexName != "" ? receiveIndexName : conv->receiveIndexName(),
                                                marketConfiguration)
                                    ->forwardingTermStructure();
        payIndexCurve = market->iborIndex(payIndexName != "" ? payIndexName : conv->payIndexName(), marketConfiguration)
                            ->forwardingTermStructure();
    }
    payIndex = payIndex->clone(payIndexCurve);
    receiveIndex = receiveIndex->clone(receiveIndexCurve);

    QuantLib::ext::shared_ptr<Libor> payIndexAsLibor = QuantLib::ext::dynamic_pointer_cast<Libor>(payIndex);
    QuantLib::ext::shared_ptr<Libor> receiveIndexAsLibor = QuantLib::ext::dynamic_pointer_cast<Libor>(receiveIndex);
    Calendar payIndexCalendar =
        payIndexAsLibor != nullptr ? payIndexAsLibor->jointCalendar() : payIndex->fixingCalendar();
    Calendar receiveIndexCalendar =
        receiveIndexAsLibor != nullptr ? receiveIndexAsLibor->jointCalendar() : receiveIndex->fixingCalendar();
    removeTodaysFixingIndices.insert(receiveIndex->name());
    removeTodaysFixingIndices.insert(payIndex->name());

    Date settlementDate = payIndexCalendar.advance(payIndexCalendar.adjust(asof), payIndex->fixingDays() * Days);

    bool telescopicValueDates = true;
    auto helper = QuantLib::ext::make_shared<TenorBasisSwap>(
        settlementDate, 1.0, term, payIndex, 0.0, conv->payFrequency(), receiveIndex, 0.0, conv->receiveFrequency(),
        DateGeneration::Backward, conv->includeSpread(), conv->spreadOnRec(), conv->subPeriodsCouponType(),
        telescopicValueDates);

    auto lastPayCoupon = helper->payLeg().back();
    auto lastReceiveCoupon = helper->recLeg().back();

    auto lastCouponDate = [](QuantLib::ext::shared_ptr<CashFlow> flow, Period freq, Calendar cal) -> Date {
        if (auto c = QuantLib::ext::dynamic_pointer_cast<IborCoupon>(flow))
            return c->fixingEndDate();
        Date d{};
        if (auto c = QuantLib::ext::dynamic_pointer_cast<SubPeriodsCoupon1>(flow))
            d = c->valueDates().back();
        else if (auto c = QuantLib::ext::dynamic_pointer_cast<QuantLib::OvernightIndexedCoupon>(flow)) {
            d = c->valueDates().back();
            freq = 1 * Days;
        } else {
            QL_FAIL("makeTenorBasisSwap(): Either IborCoupon, SubPeriodsCoupon1 or OvernightIndexedCoupon cashflow "
                    "expected.");
        }
        return cal.advance(d, freq);
    };

    Date maxDate1 = lastCouponDate(lastPayCoupon, conv->payFrequency(), payIndexCalendar);
    Date maxDate2 = lastCouponDate(lastReceiveCoupon, conv->receiveFrequency(), receiveIndexCalendar);
    Date latestRelevantDate = std::max(helper->maturityDate(), std::max(maxDate1, maxDate2));

    if (market != nullptr) {
        QuantLib::ext::shared_ptr<PricingEngine> swapEngine = QuantLib::ext::make_shared<DiscountingSwapEngine>(discountCurve);
        helper->setPricingEngine(swapEngine);
    } else {
        if (!singleCurve) {
            parHelperDependencies_.emplace(RiskFactorKey::KeyType::IndexCurve, receiveIndexName, 0);
        }
        parHelperDependencies_.emplace(RiskFactorKey::KeyType::IndexCurve, payIndexName, 0);
        if (!expDiscountCurve.empty())
            parHelperDependencies_.emplace(RiskFactorKey::KeyType::IndexCurve, expDiscountCurve, 0);
        else
            parHelperDependencies_.emplace(RiskFactorKey::KeyType::DiscountCurve, ccy);
    }

    // latest date and return result
    return std::pair<QuantLib::ext::shared_ptr<Instrument>, Date>(helper, latestRelevantDate);
}

QuantLib::ext::shared_ptr<CapFloor> ParSensitivityInstrumentBuilder::makeCapFloor(
    const QuantLib::ext::shared_ptr<Market>& market, string ccy, string indexName, Period term, Real strike, bool isAtm,
    std::set<ore::analytics::RiskFactorKey>& parHelperDependencies_, const std::string& expDiscountCurve,
    const string& marketConfiguration) const {

    QuantLib::ext::shared_ptr<CapFloor> inst;
    auto conventions = InstrumentConventions::instance().conventions();

    if (!market) {
        // No market so just return a dummy cap
        QuantLib::ext::shared_ptr<IborIndex> index = parseIborIndex(indexName);
        QL_REQUIRE(QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(index) == nullptr,
                   "ParSensitivityAnalysis::makeCapFloor(): OIS indices are not yet supported for par conversion");
        inst = MakeCapFloor(CapFloor::Cap, term, index, 0.03);
    } else {

        QuantLib::ext::shared_ptr<IborIndex> index = *market->iborIndex(indexName, marketConfiguration);
        QL_REQUIRE(QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(index) == nullptr,
                   "ParSensitivityAnalysis::makeCapFloor(): OIS indices are not yet supported for par conversion");
        QL_REQUIRE(index, "Index not found with name " << indexName);
        Handle<YieldTermStructure> discount;
        if (expDiscountCurve.empty())
            discount = market->discountCurve(ccy, marketConfiguration);
        else
            discount = market->iborIndex(expDiscountCurve, marketConfiguration)->forwardingTermStructure();
        QL_REQUIRE(!discount.empty(), "Discount curve not found for cap floor index " << indexName);

        // Create a dummy cap just to get the ATM rate
        // Note this construction excludes the first caplet which is what we want
        inst = MakeCapFloor(CapFloor::Cap, term, index, 0.03);
        Rate atmRate = inst->atmRate(**discount);
        // bool isAtm = strike == Null<Real>();
        // strike = isAtm ? atmRate : strike;
        strike = strike == Null<Real>() ? atmRate : strike;
        CapFloor::Type type = strike >= atmRate ? CapFloor::Cap : CapFloor::Floor;

        // Create the actual cap or floor instrument that we will use
        if (isAtm) {
            inst = MakeCapFloor(type, term, index, atmRate);
        } else {
            inst = MakeCapFloor(type, term, index, strike);
        }
        Handle<OptionletVolatilityStructure> ovs = market->capFloorVol(indexName, marketConfiguration);
        QL_REQUIRE(!ovs.empty(), "Optionlet volatility structure not found for index " << indexName);
        QL_REQUIRE(ovs->volatilityType() == ShiftedLognormal || ovs->volatilityType() == Normal,
                   "Optionlet volatility type " << ovs->volatilityType() << " not covered");
        QuantLib::ext::shared_ptr<PricingEngine> engine;
        if (ovs->volatilityType() == ShiftedLognormal) {
            engine = QuantLib::ext::make_shared<BlackCapFloorEngine>(discount, ovs, ovs->displacement());
        } else {
            engine = QuantLib::ext::make_shared<BachelierCapFloorEngine>(discount, ovs);
        }
        inst->setPricingEngine(engine);
    }
    parHelperDependencies_.emplace(RiskFactorKey::KeyType::DiscountCurve, ccy);
    parHelperDependencies_.emplace(RiskFactorKey::KeyType::IndexCurve, indexName);
    // set pillar date
    // if (generatePillar) {
    //     capFloorPillars_[ccy].push_back(term /*(end - asof) * Days*/);
    // }

    QL_REQUIRE(inst, "empty cap/floor par instrument pointer");
    return inst;
}

std::pair<QuantLib::ext::shared_ptr<Instrument>, Date> ParSensitivityInstrumentBuilder::makeCrossCcyBasisSwap(
    const QuantLib::ext::shared_ptr<Market>& market, string baseCcy, string ccy, Period term,
    const QuantLib::ext::shared_ptr<Convention>& convention, std::set<ore::analytics::RiskFactorKey>& parHelperDependencies_,
    std::set<std::string>& removeTodaysFixingIndices, const string& marketConfiguration) const {

    auto conventions = InstrumentConventions::instance().conventions();
    QuantLib::ext::shared_ptr<CrossCcyBasisSwapConvention> conv =
        QuantLib::ext::dynamic_pointer_cast<CrossCcyBasisSwapConvention>(convention);
    QL_REQUIRE(conv, "convention not recognised, expected CrossCcyBasisSwapConvention");
    QL_REQUIRE(baseCcy == conv->flatIndex()->currency().code() || baseCcy == conv->spreadIndex()->currency().code(),
               "base currency " << baseCcy << " not covered by convention " << conv->id());
    QL_REQUIRE(ccy == conv->flatIndex()->currency().code() || ccy == conv->spreadIndex()->currency().code(),
               "currency " << ccy << " not covered by convention " << conv->id());
    string baseIndexName, indexName;
    Period baseIndexTenor, indexTenor;
    if (baseCcy == conv->flatIndex()->currency().code()) {
        baseIndexName = conv->flatIndexName();
        baseIndexTenor = conv->flatTenor();
        indexName = conv->spreadIndexName();
        indexTenor = conv->spreadTenor();
    } else {
        baseIndexName = conv->spreadIndexName();
        baseIndexTenor = conv->spreadTenor();
        indexName = conv->flatIndexName();
        indexTenor = conv->flatTenor();
    }
    Currency baseCurrency = parseCurrency(baseCcy);
    Currency currency = parseCurrency(ccy);
    Handle<IborIndex> baseIndex, index;
    if (market != nullptr) {
        baseIndex = market->iborIndex(baseIndexName, marketConfiguration);
        index = market->iborIndex(indexName, marketConfiguration);
    } else {
        baseIndex = Handle<IborIndex>(parseIborIndex(baseIndexName));
        index = Handle<IborIndex>(parseIborIndex(indexName));
    }
    Date today = Settings::instance().evaluationDate();

    // For now, to mimic the xccy helper behaviour in the case that today is settlementCalendar holiday
    today = conv->settlementCalendar().adjust(today);

    Date start = conv->settlementCalendar().advance(today, conv->settlementDays() * Days, conv->rollConvention());
    Date end = conv->settlementCalendar().advance(start, term, conv->rollConvention());
    Schedule baseSchedule = MakeSchedule()
                                .from(start)
                                .to(end)
                                .withTenor(baseIndexTenor)
                                .withCalendar(conv->settlementCalendar())
                                .withConvention(conv->rollConvention())
                                .endOfMonth(conv->eom());
    Schedule schedule = MakeSchedule()
                            .from(start)
                            .to(end)
                            .withTenor(indexTenor)
                            .withCalendar(conv->settlementCalendar())
                            .withConvention(conv->rollConvention())
                            .endOfMonth(conv->eom());
    Real baseNotional = 1.0;
    // the fx spot is not needed in the dry run
    Handle<Quote> fxSpot =
        market != nullptr
            // use instantaneous fx rate
            ? market->fxRate(ccy + baseCcy, marketConfiguration)
            : Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(1.0)); // multiplicative conversion into base ccy
    Real notional = 1.0 / fxSpot->value();

    RelinkableHandle<YieldTermStructure> baseDiscountCurve;
    RelinkableHandle<YieldTermStructure> discountCurve;
    QuantLib::ext::shared_ptr<FxIndex> fxIndex =
        QuantLib::ext::make_shared<FxIndex>("dummy", conv->settlementDays(), currency, baseCurrency, conv->settlementCalendar(),
                                    fxSpot, discountCurve, baseDiscountCurve);
    auto m = [](Real x) { return 1.0 / x; };
    QuantLib::ext::shared_ptr<FxIndex> reversedFxIndex = QuantLib::ext::make_shared<FxIndex>(
        "dummyRev", conv->settlementDays(), baseCurrency, currency, conv->settlementCalendar(),
        Handle<Quote>(QuantLib::ext::make_shared<DerivedQuote<decltype(m)>>(fxSpot, m)), baseDiscountCurve, discountCurve);

    // LOG("Make Cross Ccy Swap for base ccy " << baseCcy << " currency " << ccy);
    // Set up first leg as spread leg, second as flat leg
    removeTodaysFixingIndices.insert(baseIndex->name());
    removeTodaysFixingIndices.insert(index->name());
    QuantLib::ext::shared_ptr<CrossCcySwap> helper;
    bool telescopicValueDates = true; // same as in the yield curve building

    if (baseCcy == conv->spreadIndex()->currency().code()) {         // base ccy index is spread index
        if (conv->isResettable() && conv->flatIndexIsResettable()) { // i.e. flat index leg is resettable
            DLOG("create resettable xccy par instrument (1), convention " << conv->id());
            helper = QuantLib::ext::make_shared<CrossCcyBasisMtMResetSwap>(
                baseNotional, baseCurrency, baseSchedule, *baseIndex, 0.0, // spread index leg => use fairForeignSpread
                currency, schedule, *index, 0.0, reversedFxIndex, true,    // resettable flat index leg
                conv->paymentLag(), conv->flatPaymentLag(), conv->includeSpread(), conv->lookback(), conv->fixingDays(),
                conv->rateCutoff(), conv->isAveraged(), conv->flatIncludeSpread(), conv->flatLookback(),
                conv->flatFixingDays(), conv->flatRateCutoff(), conv->flatIsAveraged(), telescopicValueDates,
                true);                                                       // fair spread leg is foreign
        } else if (conv->isResettable() && !conv->flatIndexIsResettable()) { // i.e. spread index leg is resettable
            DLOG("create resettable xccy par instrument (2), convention " << conv->id());
            helper = QuantLib::ext::make_shared<CrossCcyBasisMtMResetSwap>(
                notional, currency, schedule, *index, 0.0, // flat index leg
                baseCurrency, baseSchedule, *baseIndex, 0.0, fxIndex,
                true, // resettable spread index leg => use fairDomesticSpread
                conv->flatPaymentLag(), conv->paymentLag(), conv->flatIncludeSpread(), conv->flatLookback(),
                conv->flatFixingDays(), conv->flatRateCutoff(), conv->flatIsAveraged(), conv->includeSpread(),
                conv->lookback(), conv->fixingDays(), conv->rateCutoff(), conv->isAveraged(), telescopicValueDates,
                false); // fair spread leg is domestic
        } else {        // not resettable
            DLOG("create non-resettable xccy par instrument (3), convention " << conv->id());
            helper = QuantLib::ext::make_shared<CrossCcyBasisSwap>(
                baseNotional, baseCurrency, baseSchedule, *baseIndex, 0.0, 1.0, // spread index leg => use fairPaySpread
                notional, currency, schedule, *index, 0.0, 1.0,                 // flat index leg
                conv->paymentLag(), conv->flatPaymentLag(), conv->includeSpread(), conv->lookback(), conv->fixingDays(),
                conv->rateCutoff(), conv->isAveraged(), conv->flatIncludeSpread(), conv->flatLookback(),
                conv->flatFixingDays(), conv->flatRateCutoff(), conv->flatIsAveraged(), telescopicValueDates);
        }
    } else { // base ccy index is flat index
        if (conv->isResettable() && conv->flatIndexIsResettable()) {
            DLOG("create resettable xccy par instrument (4), convention " << conv->id());
            // second leg is resettable, so the second leg is the base currency leg
            helper = QuantLib::ext::make_shared<CrossCcyBasisMtMResetSwap>(
                notional, currency, schedule, *index, 0.0,                  // spread index leg => use fairForeignSpread
                baseCurrency, baseSchedule, *baseIndex, 0.0, fxIndex, true, // resettable flat index leg
                conv->paymentLag(), conv->flatPaymentLag(), conv->includeSpread(), conv->lookback(), conv->fixingDays(),
                conv->rateCutoff(), conv->isAveraged(), conv->flatIncludeSpread(), conv->flatLookback(),
                conv->flatFixingDays(), conv->flatRateCutoff(), conv->flatIsAveraged(), telescopicValueDates,
                true); // fair spread leg is foreign
        } else if (conv->isResettable() && !conv->flatIndexIsResettable()) {
            DLOG("create resettable xccy par instrument (5), convention " << conv->id());
            // second leg is resettable, so the second leg is the non-base non-flat spread leg
            helper = QuantLib::ext::make_shared<CrossCcyBasisMtMResetSwap>(
                baseNotional, baseCurrency, baseSchedule, *baseIndex, 0.0, // flat index leg
                currency, schedule, *index, 0.0, reversedFxIndex,
                true, // resettable spread index leg => use fairDomesticSpread
                conv->flatPaymentLag(), conv->paymentLag(), conv->flatIncludeSpread(), conv->flatLookback(),
                conv->flatFixingDays(), conv->flatRateCutoff(), conv->flatIsAveraged(), conv->includeSpread(),
                conv->lookback(), conv->fixingDays(), conv->rateCutoff(), conv->isAveraged(), telescopicValueDates,
                false); // fair spread leg is domestic
        } else {        // not resettable
            DLOG("create non-resettable xccy par instrument (6), convention " << conv->id());
            helper = QuantLib::ext::make_shared<CrossCcyBasisSwap>(
                notional, currency, schedule, *index, 0.0, 1.0,                 // spread index leg => use fairPaySpread
                baseNotional, baseCurrency, baseSchedule, *baseIndex, 0.0, 1.0, // flat index leg
                conv->paymentLag(), conv->flatPaymentLag(), conv->includeSpread(), conv->lookback(), conv->fixingDays(),
                conv->rateCutoff(), conv->isAveraged(), conv->flatIncludeSpread(), conv->flatLookback(),
                conv->flatFixingDays(), conv->flatRateCutoff(), conv->flatIsAveraged(), telescopicValueDates);
        }
    }

    bool isBaseDiscount = true;
    bool isNonBaseDiscount = true;
    if (market != nullptr) {
        baseDiscountCurve.linkTo(xccyYieldCurve(market, baseCcy, isBaseDiscount, marketConfiguration).currentLink());
        discountCurve.linkTo(xccyYieldCurve(market, ccy, isNonBaseDiscount, marketConfiguration).currentLink());
        QuantLib::ext::shared_ptr<PricingEngine> swapEngine =
            QuantLib::ext::make_shared<CrossCcySwapEngine>(baseCurrency, baseDiscountCurve, currency, discountCurve, fxSpot);
        helper->setPricingEngine(swapEngine);
    }

    if (isBaseDiscount)
        parHelperDependencies_.emplace(RiskFactorKey::KeyType::YieldCurve, baseCcy, 0);
    else
        parHelperDependencies_.emplace(RiskFactorKey::KeyType::DiscountCurve, baseCcy, 0);

    if (isNonBaseDiscount)
        parHelperDependencies_.emplace(RiskFactorKey::KeyType::YieldCurve, ccy, 0);
    else
        parHelperDependencies_.emplace(RiskFactorKey::KeyType::DiscountCurve, ccy, 0);

    parHelperDependencies_.emplace(RiskFactorKey::KeyType::IndexCurve, baseIndexName, 0);
    parHelperDependencies_.emplace(RiskFactorKey::KeyType::IndexCurve, indexName, 0);

    // set pillar date
    Date latestRelevantDate = helper->maturityDate();
    if (auto i = QuantLib::ext::dynamic_pointer_cast<CrossCcyBasisSwap>(helper)) {
        QuantLib::ext::shared_ptr<IborCoupon> lastCoupon0 =
            QuantLib::ext::dynamic_pointer_cast<IborCoupon>(helper->leg(0)[helper->leg(0).size() - 2]);
        QuantLib::ext::shared_ptr<IborCoupon> lastCoupon1 =
            QuantLib::ext::dynamic_pointer_cast<IborCoupon>(helper->leg(1)[helper->leg(1).size() - 2]);
        if (lastCoupon0 != nullptr)
            latestRelevantDate = std::max(latestRelevantDate, lastCoupon0->fixingEndDate());
        if (lastCoupon1 != nullptr)
            latestRelevantDate = std::max(latestRelevantDate, lastCoupon1->fixingEndDate());
        // yieldCurvePillars_[ccy].push_back((latestRelevantDate - asof) * Days);
    }

    return std::pair<QuantLib::ext::shared_ptr<Instrument>, Date>(helper, latestRelevantDate);
}

std::pair<QuantLib::ext::shared_ptr<Instrument>, Date>
ParSensitivityInstrumentBuilder::makeFxForward(const QuantLib::ext::shared_ptr<Market>& market, string baseCcy, string ccy,
                                               Period term, const QuantLib::ext::shared_ptr<Convention>& convention,
                                               std::set<ore::analytics::RiskFactorKey>& parHelperDependencies_,
                                               const string& marketConfiguration) const {

    QuantLib::ext::shared_ptr<FXConvention> conv = QuantLib::ext::dynamic_pointer_cast<FXConvention>(convention);
    QL_REQUIRE(conv, "convention not recognised, expected FXConvention");
    QL_REQUIRE(baseCcy == conv->sourceCurrency().code() || baseCcy == conv->targetCurrency().code(),
               "base currency " << baseCcy << " not covered by convention " << conv->id());
    QL_REQUIRE(ccy == conv->sourceCurrency().code() || ccy == conv->targetCurrency().code(),
               "currency " << ccy << " not covered by convention " << conv->id());
    Currency baseCurrency = parseCurrency(baseCcy);
    Currency currency = parseCurrency(ccy);
    Date today = Settings::instance().evaluationDate();
    Date spot = conv->advanceCalendar().advance(today, conv->spotDays() * Days);
    Date maturity = conv->advanceCalendar().advance(spot, term);
    Real baseNotional = 1.0;
    Handle<Quote> fxSpot =
        market != nullptr
            ? market->fxRate(ccy + baseCcy, marketConfiguration)
            : Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(1.0)); // multiplicative conversion into base ccy
    Real notional = 1.0 / fxSpot->value();
    QuantLib::ext::shared_ptr<FxForward> helper =
        QuantLib::ext::make_shared<FxForward>(baseNotional, baseCurrency, notional, currency, maturity, true);

    bool isBaseDiscount = true;
    bool isNonBaseDiscount = true;
    if (market != nullptr) {
        Handle<YieldTermStructure> baseDiscountCurve =
            xccyYieldCurve(market, baseCcy, isBaseDiscount, marketConfiguration);
        Handle<YieldTermStructure> discountCurve = xccyYieldCurve(market, ccy, isNonBaseDiscount, marketConfiguration);
        QuantLib::ext::shared_ptr<PricingEngine> engine = QuantLib::ext::make_shared<DiscountingFxForwardEngine>(
            baseCurrency, baseDiscountCurve, currency, discountCurve, fxSpot);
        helper->setPricingEngine(engine);
    }

    if (isBaseDiscount)
        parHelperDependencies_.emplace(RiskFactorKey::KeyType::DiscountCurve, baseCcy, 0);
    else
        parHelperDependencies_.emplace(RiskFactorKey::KeyType::YieldCurve, baseCcy, 0);

    if (isNonBaseDiscount)
        parHelperDependencies_.emplace(RiskFactorKey::KeyType::DiscountCurve, ccy, 0);
    else
        parHelperDependencies_.emplace(RiskFactorKey::KeyType::YieldCurve, ccy, 0);

    // set pillar date
    // yieldCurvePillars_[ccy].push_back((maturity - asof) * Days);
    return std::pair<QuantLib::ext::shared_ptr<Instrument>, Date>(helper, maturity);
}

std::pair<QuantLib::ext::shared_ptr<Instrument>, Date>
ParSensitivityInstrumentBuilder::makeCDS(const QuantLib::ext::shared_ptr<Market>& market, string name, string ccy, Period term,
                                         const QuantLib::ext::shared_ptr<Convention>& convention,
                                         std::set<ore::analytics::RiskFactorKey>& parHelperDependencies_,
                                         const std::string& expDiscountCurve, const string& marketConfiguration) const {

    QuantLib::ext::shared_ptr<CdsConvention> conv = QuantLib::ext::dynamic_pointer_cast<CdsConvention>(convention);
    QL_REQUIRE(conv, "convention not recognised, expected CdsConvention");

    QuantLib::ext::shared_ptr<QuantExt::CreditDefaultSwap> helper = MakeCreditDefaultSwap(term, 0.1)
                                                                .withNominal(1)
                                                                .withCouponTenor(Period(conv->frequency()))
                                                                .withDayCounter(conv->dayCounter())
                                                                .withDateGenerationRule(conv->rule())
                                                                .withSettlesAccrual(conv->settlesAccrual())
                                                                .withPaysAtDefaultTime(conv->paysAtDefaultTime())
        // .withPaysAtDefaultTime(conv->rebatesAccrual()) // FIXME: missing in conventions
        ;

    if (market != nullptr) {
        Handle<YieldTermStructure> yts;
        if (!expDiscountCurve.empty()) {
            // Look up the explicit discount curve in the market
            auto discountIndex = market->iborIndex(expDiscountCurve, marketConfiguration);
            yts = discountIndex->forwardingTermStructure();
        } else {
            yts = market->discountCurve(ccy, marketConfiguration);
        }

        Handle<DefaultProbabilityTermStructure> dpts = market->defaultCurve(name, marketConfiguration)->curve();
        Handle<Quote> recovery = market->recoveryRate(name, marketConfiguration);

        QuantLib::ext::shared_ptr<PricingEngine> cdsEngine =
            QuantLib::ext::make_shared<QuantExt::MidPointCdsEngine>(dpts, recovery->value(), yts);
        helper->setPricingEngine(cdsEngine);
    }

    parHelperDependencies_.emplace(RiskFactorKey::KeyType::DiscountCurve, ccy, 0);

    // set pillar date
    // Date maturity = helper->maturity();
    Date maturity = conv->calendar().adjust(helper->maturity(), conv->paymentConvention());
    // cdsPillars_[name].push_back((maturity - asof) * Days);
    return std::pair<QuantLib::ext::shared_ptr<Instrument>, Date>(helper, maturity);
}

QuantLib::ext::shared_ptr<Instrument> ParSensitivityInstrumentBuilder::makeZeroInflationSwap(
    const QuantLib::ext::shared_ptr<Market>& market, string indexName, Period term,
    const QuantLib::ext::shared_ptr<Convention>& convention, bool singleCurve,
    std::set<ore::analytics::RiskFactorKey>& parHelperDependencies_, const std::string& expDiscountCurve,
    const string& marketConfiguration) const {

    QuantLib::ext::shared_ptr<InflationSwapConvention> conv = QuantLib::ext::dynamic_pointer_cast<InflationSwapConvention>(convention);
    QL_REQUIRE(conv, "convention not recognised, expected InflationSwapConvention");
    string name = indexName != "" ? indexName : conv->indexName();
    QuantLib::ext::shared_ptr<ZeroInflationIndex> index = conv->index();
    Currency currency = index->currency();
    string ccy = currency.code();
    Handle<YieldTermStructure> discountCurve;
    if (market != nullptr) {
        // Get the inflation index
        index = *market->zeroInflationIndex(name, marketConfiguration);

        // Get the discount curve
        if (!expDiscountCurve.empty()) {
            // Look up the explicit discount curve in the market
            auto discountIndex = market->iborIndex(expDiscountCurve, marketConfiguration);
            discountCurve = discountIndex->forwardingTermStructure();
        } else {
            // Take the default discount curve for the inflation currency from the market
            discountCurve = market->discountCurve(ccy, marketConfiguration);
        }
    }

    // Potentially use conventions here to get an updated start date e.g. AU CPI conventions with a publication roll.
    Date start = Settings::instance().evaluationDate();
    start = getInflationSwapStart(start, *conv);
    Date end = start + term;
    QuantLib::ext::shared_ptr<ZeroCouponInflationSwap> helper(new ZeroCouponInflationSwap(
        ZeroCouponInflationSwap::Payer, 1.0, start, end, conv->infCalendar(), conv->infConvention(), conv->dayCounter(),
        0.0, index, conv->observationLag(), CPI::AsIndex));

    if (market != nullptr) {
        QuantLib::ext::shared_ptr<PricingEngine> swapEngine = QuantLib::ext::make_shared<DiscountingSwapEngine>(discountCurve);
        helper->setPricingEngine(swapEngine);
    }

    parHelperDependencies_.emplace(RiskFactorKey::KeyType::DiscountCurve, ccy, 0);

    // set pillar date
    return helper;
}

QuantLib::ext::shared_ptr<Instrument> ParSensitivityInstrumentBuilder::makeYoyInflationSwap(
    const QuantLib::ext::shared_ptr<Market>& market, string indexName, Period term,
    const QuantLib::ext::shared_ptr<Convention>& convention, bool singleCurve, bool fromZero,
    std::set<ore::analytics::RiskFactorKey>& parHelperDependencies_, const std::string& expDiscountCurve,
    const string& marketConfiguration) const {

    QuantLib::ext::shared_ptr<InflationSwapConvention> conv = QuantLib::ext::dynamic_pointer_cast<InflationSwapConvention>(convention);
    QL_REQUIRE(conv, "convention not recognised, expected InflationSwapConvention");
    string name = indexName != "" ? indexName : conv->indexName();

    QuantLib::ext::shared_ptr<ZeroInflationIndex> zeroIndex = conv->index();
    QuantLib::ext::shared_ptr<YoYInflationIndex> index =
        QuantLib::ext::make_shared<QuantExt::YoYInflationIndexWrapper>(zeroIndex, conv->interpolated());

    // Potentially use conventions here to get an updated start date e.g. AU CPI conventions with a publication roll.
    Date start = Settings::instance().evaluationDate();
    start = getInflationSwapStart(start, *conv);
    Date end = start + term;
    Period tenor(1, Years);
    Schedule fixSchedule = MakeSchedule()
                               .from(start)
                               .to(end)
                               .withTenor(tenor)
                               .withCalendar(conv->fixCalendar())
                               .withConvention(conv->fixConvention());
    Schedule yoySchedule = MakeSchedule()
                               .from(start)
                               .to(end)
                               .withTenor(tenor)
                               .withCalendar(conv->infCalendar())
                               .withConvention(conv->infConvention());

    Currency currency = index->currency();
    string ccy = currency.code();
    Handle<YieldTermStructure> discountCurve;
    if (market != nullptr) {

        // Get the inflation index
        if (fromZero) {
            zeroIndex = *market->zeroInflationIndex(name, marketConfiguration);
            index = QuantLib::ext::make_shared<QuantExt::YoYInflationIndexWrapper>(zeroIndex, false);
        } else {
            index = *market->yoyInflationIndex(name, marketConfiguration);
        }

        // Get the discount curve
        if (!expDiscountCurve.empty()) {
            // Look up the explicit discount curve in the market
            auto discountIndex = market->iborIndex(expDiscountCurve, marketConfiguration);
            discountCurve = discountIndex->forwardingTermStructure();
        } else {
            // Take the default discount curve for the inflation currency from the market
            discountCurve = market->discountCurve(ccy, marketConfiguration);
        }
    }

    QuantLib::ext::shared_ptr<YearOnYearInflationSwap> helper(new YearOnYearInflationSwap(
        YearOnYearInflationSwap::Payer, 1.0, fixSchedule, 0.0, conv->dayCounter(), yoySchedule, index,
        conv->observationLag(), 0.0, conv->dayCounter(), conv->infCalendar()));

    QuantLib::ext::shared_ptr<InflationCouponPricer> yoyCpnPricer = QuantLib::ext::make_shared<YoYInflationCouponPricer>(discountCurve);
    for (auto& c : helper->yoyLeg()) {
        auto cpn = QuantLib::ext::dynamic_pointer_cast<YoYInflationCoupon>(c);
        QL_REQUIRE(cpn, "yoy inflation coupon expected, could not cast");
        cpn->setPricer(yoyCpnPricer);
    }
    parHelperDependencies_.emplace(RiskFactorKey::KeyType::DiscountCurve, ccy, 0);

    if (fromZero) {
        parHelperDependencies_.emplace(RiskFactorKey::KeyType::ZeroInflationCurve, ccy, 0);
    }

    if (market != nullptr) {
        QuantLib::ext::shared_ptr<PricingEngine> swapEngine = QuantLib::ext::make_shared<DiscountingSwapEngine>(discountCurve);
        helper->setPricingEngine(swapEngine);
    }
    return helper;
}

void ParSensitivityInstrumentBuilder::makeYoYCapFloor(ParSensitivityInstrumentBuilder::Instruments& instruments,
                                                      const QuantLib::ext::shared_ptr<Market>& market, string indexName,
                                                      Period term, Real strike,
                                                      const QuantLib::ext::shared_ptr<Convention>& convention, bool singleCurve,
                                                      bool fromZero, const std::string& expDiscountCurve,
                                                      const RiskFactorKey& key,
                                                      const string& marketConfiguration) const {

    QuantLib::ext::shared_ptr<InflationSwapConvention> conv = QuantLib::ext::dynamic_pointer_cast<InflationSwapConvention>(convention);
    QL_REQUIRE(conv, "convention not recognised, expected InflationSwapConvention");
    string name = indexName != "" ? indexName : conv->indexName();

    QuantLib::ext::shared_ptr<ZeroInflationIndex> zeroIndex = conv->index();
    QuantLib::ext::shared_ptr<YoYInflationIndex> index =
        QuantLib::ext::make_shared<QuantExt::YoYInflationIndexWrapper>(zeroIndex, conv->interpolated());

    Date start = Settings::instance().evaluationDate();
    Date end = start + term;
    Period tenor(1, Years);
    Schedule yoySchedule = MakeSchedule()
                               .from(start)
                               .to(end)
                               .withTenor(tenor)
                               .withCalendar(conv->infCalendar())
                               .withConvention(conv->infConvention());

    Currency currency = index->currency();
    string ccy = currency.code();
    Handle<YieldTermStructure> discountCurve;
    if (market != nullptr) {

        // Get the inflation index
        if (fromZero) {
            zeroIndex = *market->zeroInflationIndex(name, marketConfiguration);
            index = QuantLib::ext::make_shared<QuantExt::YoYInflationIndexWrapper>(zeroIndex, conv->interpolated());
        } else {
            index = *market->yoyInflationIndex(name, marketConfiguration);
        }

        // Get the discount curve
        if (!expDiscountCurve.empty()) {
            // Look up the explicit discount curve in the market
            auto discountIndex = market->iborIndex(expDiscountCurve, marketConfiguration);
            discountCurve = discountIndex->forwardingTermStructure();
        } else {
            // Take the default discount curve for the inflation currency from the market
            discountCurve = market->discountCurve(ccy, marketConfiguration);
        }
    }

    // build the leg data and instrument
    Leg yoyLeg = yoyInflationLeg(yoySchedule, yoySchedule.calendar(), index, conv->observationLag())
                     .withNotionals(1.0)
                     .withPaymentDayCounter(conv->dayCounter())
                     .withRateCurve(discountCurve);

    if (market == nullptr)
        return;

    auto ovs = market->yoyCapFloorVol(name, marketConfiguration);
    QuantLib::ext::shared_ptr<PricingEngine> engine;
    if (ovs->volatilityType() == ShiftedLognormal) {
        if (close_enough(ovs->displacement(), 0.0)) {
            engine = QuantLib::ext::make_shared<QuantExt::YoYInflationBlackCapFloorEngine>(index, ovs, discountCurve);
        } else {
            engine =
                QuantLib::ext::make_shared<QuantExt::YoYInflationUnitDisplacedBlackCapFloorEngine>(index, ovs, discountCurve);
        }
    } else if (ovs->volatilityType() == Normal) {
        engine = QuantLib::ext::make_shared<QuantExt::YoYInflationBachelierCapFloorEngine>(index, ovs, discountCurve);
    } else {
        QL_FAIL("ParSensitivityAnalysis::makeYoYCapFloor(): volatility type " << ovs->volatilityType()
                                                                              << " not handled for index " << name);
    }

    QuantLib::ext::shared_ptr<YoYInflationCapFloor> atmHelper = QuantLib::ext::make_shared<YoYInflationCapFloor>(
        YoYInflationCapFloor::Cap, yoyLeg, std::vector<Real>(yoyLeg.size(), strike));
    Rate atmRate = atmHelper->atmRate(**discountCurve);
    strike = strike == Null<Real>() ? atmRate : strike;
    YoYInflationCapFloor::Type type = strike >= atmRate ? YoYInflationCapFloor::Cap : YoYInflationCapFloor::Floor;
    auto helper = QuantLib::ext::make_shared<YoYInflationCapFloor>(type, yoyLeg, std::vector<Real>(yoyLeg.size(), strike));
    helper->setPricingEngine(engine);

    instruments.parYoYCaps_[key] = helper;
    instruments.parYoYCapsYts_[key] = discountCurve;
    instruments.parYoYCapsIndex_[key] = Handle<YoYInflationIndex>(index);
    instruments.parYoYCapsVts_[key] = ovs;
}

} // namespace analytics
} // namespace ore
