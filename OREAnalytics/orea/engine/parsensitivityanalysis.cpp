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

#include <boost/lexical_cast.hpp>
#include <boost/numeric/ublas/vector.hpp>
#include <boost/numeric/ublas/operation.hpp>
#include <orea/cube/inmemorycube.hpp>
#include <orea/app/structuredanalyticserror.hpp>
#include <orea/engine/observationmode.hpp>
#include <orea/engine/valuationengine.hpp>
#include <orea/scenario/simplescenariofactory.hpp>
#include <orea/scenario/sensitivityscenariodata.hpp>
#include <orea/engine/parsensitivityanalysis.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/marketdata.hpp>
#include <ored/utilities/to_string.hpp>
#include <ored/marketdata/inflationcurve.hpp>
#include <ql/cashflows/indexedcashflow.hpp>
#include <ql/cashflows/overnightindexedcoupon.hpp>
#include <ql/cashflows/capflooredinflationcoupon.hpp>
#include <ql/errors.hpp>
#include <ql/indexes/ibor/libor.hpp>
#include <ql/instruments/creditdefaultswap.hpp>
#include <ql/instruments/forwardrateagreement.hpp>
#include <ql/instruments/makecapfloor.hpp>
#include <ql/instruments/makeois.hpp>
#include <ql/instruments/makevanillaswap.hpp>
#include <qle/instruments/fixedbmaswap.hpp>
#include <ql/instruments/yearonyearinflationswap.hpp>
#include <ql/instruments/zerocouponinflationswap.hpp>
#include <ql/math/solvers1d/newtonsafe.hpp>
#include <ql/pricingengines/capfloor/bacheliercapfloorengine.hpp>
#include <ql/pricingengines/capfloor/blackcapfloorengine.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/pricingengines/credit/midpointcdsengine.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/termstructures/yield/oisratehelper.hpp>
#include <qle/indexes/inflationindexwrapper.hpp>
#include <qle/instruments/brlcdiswap.hpp>
#include <qle/instruments/crossccybasisswap.hpp>
#include <qle/instruments/crossccybasismtmresetswap.hpp>
#include <qle/instruments/deposit.hpp>
#include <qle/instruments/fxforward.hpp>
#include <qle/instruments/makecds.hpp>
#include <qle/pricingengines/crossccyswapengine.hpp>
#include <qle/pricingengines/depositengine.hpp>
#include <qle/pricingengines/discountingfxforwardengine.hpp>
#include <qle/pricingengines/inflationcapfloorengines.hpp>
#include <qle/math/blockmatrixinverse.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;
using namespace ore::data;
using namespace ore::analytics;

using boost::numeric::ublas::element_div;
using boost::numeric::ublas::element_prod;

namespace ore {
namespace analytics {

namespace {

Real impliedQuote(const boost::shared_ptr<Instrument>& i) {
    if (boost::dynamic_pointer_cast<VanillaSwap>(i))
        return boost::dynamic_pointer_cast<VanillaSwap>(i)->fairRate();
    if (boost::dynamic_pointer_cast<Deposit>(i))
        return boost::dynamic_pointer_cast<Deposit>(i)->fairRate();
    if (boost::dynamic_pointer_cast<ForwardRateAgreement>(i))
        return boost::dynamic_pointer_cast<ForwardRateAgreement>(i)->forwardRate();
    if (boost::dynamic_pointer_cast<OvernightIndexedSwap>(i))
        return boost::dynamic_pointer_cast<OvernightIndexedSwap>(i)->fairRate();
    if (boost::dynamic_pointer_cast<CrossCcyBasisMtMResetSwap>(i))
      return boost::dynamic_pointer_cast<CrossCcyBasisMtMResetSwap>(i)->fairSpread(); 
    if (boost::dynamic_pointer_cast<CrossCcyBasisSwap>(i))
        return boost::dynamic_pointer_cast<CrossCcyBasisSwap>(i)->fairPaySpread();
    if (boost::dynamic_pointer_cast<FxForward>(i))
        return boost::dynamic_pointer_cast<FxForward>(i)->fairForwardRate().rate();
    if (boost::dynamic_pointer_cast<QuantExt::CreditDefaultSwap>(i))
        return boost::dynamic_pointer_cast<QuantExt::CreditDefaultSwap>(i)->fairSpreadClean();
    if (boost::dynamic_pointer_cast<ZeroCouponInflationSwap>(i))
        return boost::dynamic_pointer_cast<ZeroCouponInflationSwap>(i)->fairRate();
    if (boost::dynamic_pointer_cast<YearOnYearInflationSwap>(i))
        return boost::dynamic_pointer_cast<YearOnYearInflationSwap>(i)->fairRate();
    if (boost::dynamic_pointer_cast<TenorBasisSwap>(i))
        return boost::dynamic_pointer_cast<TenorBasisSwap>(i)->fairShortLegSpread(); // assume spread on short index
    if (boost::dynamic_pointer_cast<OvernightIndexedBasisSwap>(i))
        return boost::dynamic_pointer_cast<OvernightIndexedBasisSwap>(i)->fairOvernightSpread();
    if (boost::dynamic_pointer_cast<FixedBMASwap>(i))
        return boost::dynamic_pointer_cast<FixedBMASwap>(i)->fairRate();
    if (boost::dynamic_pointer_cast<SubPeriodsSwap>(i))
        return boost::dynamic_pointer_cast<SubPeriodsSwap>(i)->fairRate();
    QL_FAIL("SensitivityAnalysis: impliedQuote: unknown instrument (is null = " << std::boolalpha << (i == nullptr)
                                                                                << ")");
}

// true if key type and name are equal, do not care about the index though
bool similar(const ore::analytics::RiskFactorKey& x, const ore::analytics::RiskFactorKey& y) {
    return x.keytype == y.keytype && x.name == y.name;
}

} // anonymous namespace

/*! Utility for implying a flat volatility which reproduces the provided Cap/Floor price
    IR: CapFloorType = CapFloor, IndexType not used
    YY: CapFloorType = YoYInflationCapFloor, IndexType = YoYInflationIndex */
template <typename CapFloorType, typename IndexType = QuantLib::Index>
Volatility impliedVolatility(const CapFloorType& cap, Real targetValue, const Handle<YieldTermStructure>& d,
                             Volatility guess, VolatilityType type, Real displacement,
                             const Handle<IndexType>& index = Handle<IndexType>());

//! Constructor
ParSensitivityAnalysis::ParSensitivityAnalysis(const Date& asof,
                                               const boost::shared_ptr<ScenarioSimMarketParameters>& simMarketParams,
                                               const SensitivityScenarioData& sensitivityData,
                                               const string& marketConfiguration, const bool continueOnError,
                                               const set<RiskFactorKey::KeyType>& typesDisabled)
    : asof_(asof), simMarketParams_(simMarketParams), sensitivityData_(sensitivityData),
      marketConfiguration_(marketConfiguration), continueOnError_(continueOnError), typesDisabled_(typesDisabled) {
    const boost::shared_ptr<Conventions>& conventions = InstrumentConventions::instance().conventions();
    QL_REQUIRE(conventions != nullptr, "conventions are empty");
    createParInstruments(nullptr);
}

void ParSensitivityAnalysis::createParInstruments(const boost::shared_ptr<ScenarioSimMarket>& simMarket) {

    QL_REQUIRE(typesDisabled_ != parTypes_, "At least one par risk factor type must be enabled " <<
        "for a valid ParSensitivityAnalysis.");

    // this is called twice, first (dry run) with simMarket = nullptr to
    // - do pillar adjustments and
    // - populate the par helper dependencies
    // and second with simMarket != null to
    // - create the final par instruments linked to the sim market

    bool dryRun = simMarket == nullptr;
    if (dryRun) {
         Settings::instance().evaluationDate() = asof_;
    }
    
    LOG("Build par instruments...");
    
    parHelpers_.clear();
    parCaps_.clear();
    parYoYCaps_.clear();

    const boost::shared_ptr<Conventions>& conventions = InstrumentConventions::instance().conventions();
    QL_REQUIRE(conventions != nullptr, "conventions are empty");
    
    // Discount curve instruments
    if (typesDisabled_.count(RiskFactorKey::KeyType::DiscountCurve) == 0) {

        LOG("ParSensitivityAnalysis: Discount curve par instruments");
        for (auto c : sensitivityData_.discountCurveShiftData()) {
            string ccy = c.first;
            QL_REQUIRE(simMarket || yieldCurvePillars_.find(ccy) == yieldCurvePillars_.end(), "duplicate entry in yieldCurvePillars '" << ccy << "'");
            SensitivityScenarioData::CurveShiftParData data =
                *boost::dynamic_pointer_cast<SensitivityScenarioData::CurveShiftParData>(c.second);
            LOG("ParSensitivityAnalysis: Discount curve ccy=" << ccy);
            Size n_ten = data.shiftTenors.size();
            QL_REQUIRE(data.parInstruments.size() == n_ten,
                       "number of tenors does not match number of discount curve par instruments, "
                           << data.parInstruments.size() << " vs. " << n_ten << " ccy=" << ccy
                           << ", check sensitivity configuration.");
            for (Size j = 0; j < n_ten; ++j) {
                RiskFactorKey key(RiskFactorKey::KeyType::DiscountCurve, ccy, j);
                if (!dryRun && !relevantRiskFactors_.empty() &&
                    relevantRiskFactors_.find(key) == relevantRiskFactors_.end())
                    continue;
                Period term = data.shiftTenors[j];
                string instType = data.parInstruments[j];
                bool singleCurve = data.parInstrumentSingleCurve;
                string indexName = "";               // if empty, it will be picked from conventions
                string yieldCurveName = "";          // ignored, if empty
                string equityForecastCurveName = ""; // ignored, if empty
                std::pair<boost::shared_ptr<Instrument>, Date> ret;
                bool recognised = true, skipped = false;
                try {
                    map<string, string> conventionsMap = data.parInstrumentConventions;
                    QL_REQUIRE(conventionsMap.find(instType) != conventionsMap.end(),
                        "conventions not found for ccy " << ccy << " and instrument type " << instType);
                    boost::shared_ptr<Convention> convention = conventions->get(conventionsMap[instType]);
                    QL_REQUIRE(convention != nullptr, "convention is empty");
                    if (instType == "IRS")
                        ret = makeSwap(simMarket, ccy, indexName, yieldCurveName, equityForecastCurveName, term, convention,
                            singleCurve, parHelperDependencies_[key], data.discountCurve);
                    else if (instType == "DEP")
                        ret = makeDeposit(simMarket, ccy, indexName, yieldCurveName, equityForecastCurveName, term, convention);
                    else if (instType == "FRA")
                        ret = makeFRA(simMarket, ccy, indexName, yieldCurveName, equityForecastCurveName, term, convention);
                    else if (instType == "OIS")
                        ret = makeOIS(simMarket, ccy, indexName, yieldCurveName, equityForecastCurveName, term, convention,
                            singleCurve, parHelperDependencies_[key], data.discountCurve);
                    else if (instType == "XBS") {
                        string otherCurrency = data.otherCurrency.empty() ? simMarketParams_->baseCcy() : data.otherCurrency;
                        ret = makeCrossCcyBasisSwap(simMarket, otherCurrency, ccy, term, convention, parHelperDependencies_[key]);
                    } else if (instType == "FXF")
                        ret = makeFxForward(simMarket, simMarketParams_->baseCcy(), ccy, term, convention, parHelperDependencies_[key]);
                    else if (instType == "TBS")
                        ret = makeTenorBasisSwap(simMarket, ccy, "", "", "", "", term, convention, parHelperDependencies_[key], data.discountCurve);
                    else
                        recognised = false;
                } catch (const std::exception& e) {
                    skipped = true;
                    if (continueOnError_) {
                        ALOG(StructuredAnalyticsErrorMessage("par sensitivity conversion",
                                                             "skipping par instrument for discount curve " + ccy +
                                                                 ": " + e.what()));
                    } else {
                        QL_FAIL(e.what());
                    }
                }
                if (!recognised)
                    QL_FAIL("Instrument type " << instType << " for par sensitivity conversion not recognised");
                if (!skipped) {
                    parHelpers_[key] = ret.first;
                    if (!simMarket) {
                        yieldCurvePillars_[ccy].push_back((ret.second - asof_) * Days);
                    }
                    DLOG("Par instrument for discount curve, ccy " << ccy << " tenor " << j << ", type " << instType
                        << " built.");
                }
            }
        }
    }

    if (typesDisabled_.count(RiskFactorKey::KeyType::YieldCurve) == 0) {

        LOG("ParSensitivityAnalysis: Yield curve par instruments");
        // Yield curve instruments
        QL_REQUIRE(simMarketParams_->yieldCurveNames().size() == simMarketParams_->yieldCurveCurrencies().size(),
            "vector size mismatch in sim market parameters yield curve names/currencies");
        for (auto y : sensitivityData_.yieldCurveShiftData()) {
            string curveName = y.first;            
            QL_REQUIRE(simMarket || yieldCurvePillars_.find(curveName) == yieldCurvePillars_.end(),
                "duplicate entry in yieldCurvePillars '" << curveName << "'");
            string equityForecastCurveName = ""; // ignored, if empty
            string ccy = "";
            for (Size j = 0; j < simMarketParams_->yieldCurveNames().size(); ++j) {
                if (curveName == simMarketParams_->yieldCurveNames()[j])
                    ccy = simMarketParams_->yieldCurveCurrencies().at(curveName);
            }
            LOG("ParSensitivityAnalysis: yield curve name " << curveName);
            QL_REQUIRE(ccy != "", "yield curve currency not found for yield curve " << curveName);
            SensitivityScenarioData::CurveShiftParData data =
                *boost::static_pointer_cast<SensitivityScenarioData::CurveShiftParData>(y.second);
            Size n_ten = data.shiftTenors.size();
            QL_REQUIRE(data.parInstruments.size() == n_ten,
                "number of tenors does not match number of yield curve par instruments");
            for (Size j = 0; j < n_ten; ++j) {
                RiskFactorKey key(RiskFactorKey::KeyType::YieldCurve, curveName, j);
                if (!dryRun && !relevantRiskFactors_.empty() &&
                    relevantRiskFactors_.find(key) == relevantRiskFactors_.end())
                    continue;
                Period term = data.shiftTenors[j];
                string instType = data.parInstruments[j];
                bool singleCurve = data.parInstrumentSingleCurve;
                std::pair<boost::shared_ptr<Instrument>, Date> ret;
                bool recognised = true, skipped = false;
                try {
                    map<string, string> conventionsMap = data.parInstrumentConventions;
                    QL_REQUIRE(conventionsMap.find(instType) != conventionsMap.end(),
                        "conventions not found for ccy " << ccy << " and instrument type " << instType);
                    boost::shared_ptr<Convention> convention = conventions->get(conventionsMap[instType]);

                    if (instType == "IRS")
                        ret = makeSwap(simMarket, ccy, "", curveName, equityForecastCurveName, term, convention, singleCurve,
                            parHelperDependencies_[key], data.discountCurve);
                    else if (instType == "DEP")
                        ret = makeDeposit(simMarket, ccy, "", curveName, equityForecastCurveName, term, convention);
                    else if (instType == "FRA")
                        ret = makeFRA(simMarket, ccy, "", curveName, equityForecastCurveName, term, convention);
                    else if (instType == "OIS")
                        ret = makeOIS(simMarket, ccy, "", curveName, equityForecastCurveName, term, convention, singleCurve,
                            parHelperDependencies_[key], data.discountCurve);
                    else if (instType == "TBS")
                        ret = makeTenorBasisSwap(simMarket, ccy, "", "", curveName, "", term, convention,
                            parHelperDependencies_[key], data.discountCurve);
                    else if (instType == "XBS") {
                        string otherCurrency = data.otherCurrency.empty() ? simMarketParams_->baseCcy() : data.otherCurrency;
                        ret = makeCrossCcyBasisSwap(simMarket, otherCurrency, ccy, term, convention, parHelperDependencies_[key]);
                    } else
                        recognised = false;
                } catch (const std::exception& e) {
                    skipped = true;
                    if (continueOnError_) {
                        ALOG(StructuredAnalyticsErrorMessage("par sensitivity conversion",
                                                      "skipping par instrument for " + curveName + ": " + e.what()));
                    } else {
                        QL_FAIL(e.what());
                    }
                }
                if (!recognised)
                    QL_FAIL("Instrument type " << instType << " for par sensitivity conversion unexpected");
                if (!skipped) {
                    parHelpers_[key] = ret.first;
                    if (!simMarket) {
                        yieldCurvePillars_[curveName].push_back((ret.second - asof_) * Days);
                    }
                    DLOG("Par instrument for yield curve, ccy " << ccy << " tenor " << j << ", type " << instType
                        << " built.");
                }
            }
        }
    }
    
    if (typesDisabled_.count(RiskFactorKey::KeyType::IndexCurve) == 0) {

        LOG("ParSensitivityAnalysis: Index curve par instruments");
        // Index curve instruments
        for (auto index : sensitivityData_.indexCurveShiftData()) {
            string indexName = index.first;
            QL_REQUIRE(simMarket || yieldCurvePillars_.find(indexName) == yieldCurvePillars_.end(),
                "duplicate entry in yieldCurvePillars '" << indexName << "'");
            SensitivityScenarioData::CurveShiftParData data = *boost::static_pointer_cast<SensitivityScenarioData::CurveShiftParData>(index.second);
            Size n_ten = data.shiftTenors.size();
            QL_REQUIRE(data.parInstruments.size() == n_ten,
                "number of tenors does not match number of index curve par instruments");
            vector<string> tokens;
            boost::split(tokens, indexName, boost::is_any_of("-"));
            QL_REQUIRE(tokens.size() >= 2, "index name " << indexName << " unexpected");
            string ccy = tokens[0];
            QL_REQUIRE(ccy.length() == 3, "currency token not recognised");
            for (Size j = 0; j < n_ten; ++j) {
                RiskFactorKey key(RiskFactorKey::KeyType::IndexCurve, indexName, j);
                if (!dryRun && !relevantRiskFactors_.empty() &&
                    relevantRiskFactors_.find(key) == relevantRiskFactors_.end())
                    continue;
                Period term = data.shiftTenors[j];
                string instType = data.parInstruments[j];
                bool singleCurve = data.parInstrumentSingleCurve;
                string yieldCurveName = "";
                string equityForecastCurveName = ""; // ignored, if empty
                std::pair<boost::shared_ptr<Instrument>, Date> ret;
                bool recognised = true, skipped = false;
                try {
                    map<string, string> conventionsMap = data.parInstrumentConventions;
                    QL_REQUIRE(conventionsMap.find(instType) != conventionsMap.end(),
                        "conventions not found for ccy " << ccy << " and instrument type " << instType);
                    boost::shared_ptr<Convention> convention = conventions->get(conventionsMap[instType]);

                    if (instType == "IRS")
                        ret = makeSwap(simMarket, ccy, indexName, yieldCurveName, equityForecastCurveName, term, convention, singleCurve,
                            parHelperDependencies_[key], data.discountCurve);
                    else if (instType == "DEP")
                        ret = makeDeposit(simMarket, ccy, indexName, yieldCurveName, equityForecastCurveName, term, convention);
                    else if (instType == "FRA")
                        ret = makeFRA(simMarket, ccy, indexName, yieldCurveName, equityForecastCurveName, term, convention);
                    else if (instType == "OIS")
                        ret = makeOIS(simMarket, ccy, indexName, yieldCurveName, equityForecastCurveName, term, convention, singleCurve,
                            parHelperDependencies_[key], data.discountCurve);
                    else if (instType == "TBS")
                        ret = makeTenorBasisSwap(simMarket, ccy, "", "", "", "", term, convention,
                            parHelperDependencies_[key], data.discountCurve);
                    else
                        recognised = false;
                } catch (const std::exception& e) {
                    skipped = true;
                    if (continueOnError_) {
                        ALOG(StructuredAnalyticsErrorMessage("par sensitivity conversion",
                                                      "skipping par instrument for index curve " + indexName + ": " +
                                                          e.what()));
                    } else {
                        QL_FAIL(e.what());
                    }
                }
                if (!recognised)
                    QL_FAIL("Instrument type " << instType << " for par sensitivity conversion not recognised");
                if (!skipped) {
                    parHelpers_[key] = ret.first;
                    if (!simMarket) {
                        yieldCurvePillars_[indexName].push_back((ret.second - asof_) * Days);
                    }
                    DLOG("Par instrument for index " << indexName << " ccy " << ccy << " tenor " << j << " built.");
                }
            }
        }
    }

    if (typesDisabled_.count(RiskFactorKey::KeyType::OptionletVolatility) == 0) {

        // Caps/Floors
        LOG("ParSensitivityAnalysis: Cap/Floor par instruments");

        for (auto c : sensitivityData_.capFloorVolShiftData()) {
            string key = c.first;
            auto datap =
                boost::dynamic_pointer_cast<SensitivityScenarioData::CapFloorVolShiftParData>(c.second);
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
                    if (!dryRun && !relevantRiskFactors_.empty() &&
                        relevantRiskFactors_.find(rfkey) == relevantRiskFactors_.end())
                        continue;
                    try {
                        if (simMarket != nullptr) {
                            yts = expDiscountCurve.empty() ? simMarket->discountCurve(ccy, marketConfiguration_)
                                : simMarket->iborIndex(expDiscountCurve, marketConfiguration_)
                                ->forwardingTermStructure();
                            ovs = simMarket->capFloorVol(key, marketConfiguration_);
                        }
                        Period term = data.shiftExpiries[k];
                        auto tmp = makeCapFloor(simMarket, ccy, indexName, term, strike, isAtm, parHelperDependencies_[rfkey], expDiscountCurve);
                        parCaps_[rfkey] = tmp;
                        parCapsYts_[rfkey] = yts;
                        parCapsVts_[rfkey] = ovs;
                        if (j == 0)
                            capFloorPillars_[key].push_back(term);
                        DLOG("Par cap/floor for key " << rfkey << " strike " << j << " tenor " << k << " built.");
                    } catch (const std::exception& e) {
                        if (continueOnError_) {
                            ALOG(StructuredAnalyticsErrorMessage("par sensitivity conversion",
                                                          "skipping par cap/floor for key " + key + ": " + e.what()));
                        } else {
                            QL_FAIL(e.what());
                        }
                    }
                }
            }
        }
    }

    if (typesDisabled_.count(RiskFactorKey::KeyType::SurvivalProbability) == 0) {

        // CDS Instruments
        LOG("ParSensitivityAnalysis: CDS par instruments");
        for (auto c : sensitivityData_.creditCurveShiftData()) {
            string name = c.first;
            string ccy = sensitivityData_.creditCcys()[name];
            auto itr = sensitivityData_.creditCurveShiftData().find(name);
            QL_REQUIRE(itr != sensitivityData_.creditCurveShiftData().end(), "creditCurveShiftData not found for " << name);
            SensitivityScenarioData::CurveShiftParData data =
                *boost::static_pointer_cast<SensitivityScenarioData::CurveShiftParData>(c.second);
            Size n_expiries = data.shiftTenors.size();
            for (Size k = 0; k < n_expiries; ++k) {
                string instType = data.parInstruments[k];
                RiskFactorKey key(RiskFactorKey::KeyType::SurvivalProbability, name, k);
                if (!dryRun && !relevantRiskFactors_.empty() &&
                    relevantRiskFactors_.find(key) == relevantRiskFactors_.end())
                    continue;
                Period term = data.shiftTenors[k];
                std::pair<boost::shared_ptr<Instrument>, Date> ret;
                bool skipped = false;
                try {
                    map<string, string> conventionsMap = data.parInstrumentConventions;
                    QL_REQUIRE(conventionsMap.find(instType) != conventionsMap.end(),
                        "conventions not found for name " << name << " and instrument type " << instType);
                    boost::shared_ptr<Convention> convention = conventions->get(conventionsMap[instType]);

                    ret = makeCDS(simMarket, name, ccy, term, convention, parHelperDependencies_[key], data.discountCurve);
                } catch (const std::exception& e) {
                    skipped = true;
                    if (continueOnError_) {
                        ALOG(StructuredAnalyticsErrorMessage("par sensitivity conversion",
                                                      "skipping par instrument for cds " + name + ": " + e.what()));
                    } else {
                        QL_FAIL(e.what());
                    }
                }
                if (!skipped) {
                    parHelpers_[key] = ret.first;
                    if (!simMarket) {
                        cdsPillars_[name].push_back((ret.second - asof_) * Days);
                    }
                    DLOG("Par CDS for name " << name << " tenor " << k << " built.");
                }
            }
        }
    }

    if (typesDisabled_.count(RiskFactorKey::KeyType::ZeroInflationCurve) == 0) {

        LOG("ParSensitivityAnalysis: ZCI curve par instruments");
        // Zero Inflation Curve instruments
        for (auto z : sensitivityData_.zeroInflationCurveShiftData()) {
            string indexName = z.first;
            SensitivityScenarioData::CurveShiftParData data =
                *boost::static_pointer_cast<SensitivityScenarioData::CurveShiftParData>(z.second);
            Size n_ten = data.shiftTenors.size();
            for (Size j = 0; j < n_ten; ++j) {
                RiskFactorKey key(RiskFactorKey::KeyType::ZeroInflationCurve, indexName, j);
                if (!dryRun && !relevantRiskFactors_.empty() &&
                    relevantRiskFactors_.find(key) == relevantRiskFactors_.end())
                    continue;
                Period term = data.shiftTenors[j];
                string instType = data.parInstruments[j];
                bool singleCurve = data.parInstrumentSingleCurve;
                try {
                    map<string, string> conventionsMap = data.parInstrumentConventions;
                    QL_REQUIRE(conventionsMap.find(instType) != conventionsMap.end(),
                        "conventions not found for zero inflation curve " << indexName << " and instrument type " << instType);
                    boost::shared_ptr<Convention> convention = conventions->get(conventionsMap[instType]);

                    auto tmp = makeZeroInflationSwap(simMarket, indexName, term, convention, singleCurve,
                        parHelperDependencies_[key], data.discountCurve);
                    parHelpers_[key] = tmp;
                    DLOG("Par instrument for zero inflation index " << indexName << " tenor " << j << " built.");
                } catch (const std::exception& e) {
                    if (continueOnError_) {
                        ALOG(StructuredAnalyticsErrorMessage("par sensitivity conversion",
                                                             "skipping par instrument for zero inflation index " +
                                                                 indexName + ": " + e.what()));
                    } else {
                        QL_FAIL(e.what());
                    }
                }
            }
        }
    }

    if (typesDisabled_.count(RiskFactorKey::KeyType::YoYInflationCurve) == 0) {

        // YoY Inflation Curve instruments
        LOG("ParSensitivityAnalysis: YOYI curve par instruments");
        for (auto y : sensitivityData_.yoyInflationCurveShiftData()) {
            string indexName = y.first;
            SensitivityScenarioData::CurveShiftParData data = *boost::static_pointer_cast<SensitivityScenarioData::CurveShiftParData>(y.second);
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
                        "conventions not found for zero inflation curve " << indexName << " and instrument type " << instType);
                    boost::shared_ptr<Convention> convention = conventions->get(conventionsMap[instType]);

                    if (instType == "ZIS") {
                        auto tmp = makeYoyInflationSwap(simMarket, indexName, term, convention, singleCurve, true,
                            parHelperDependencies_[key], data.discountCurve);
                        parHelpers_[key] = tmp;
                    } else if (instType == "YYS") {
                        auto tmp = makeYoyInflationSwap(simMarket, indexName, term, convention, singleCurve, false,
                            parHelperDependencies_[key], data.discountCurve);
                        parHelpers_[key] = tmp;
                    } else
                        recognised = false;
                } catch (const std::exception& e) {
                    if (continueOnError_) {
                        ALOG(StructuredAnalyticsErrorMessage("par sensitivity conversion",
                                                      "skipping par instrument for yoy index " + indexName + ": " +
                                                          e.what()));
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

    if (typesDisabled_.count(RiskFactorKey::KeyType::YoYInflationCapFloorVolatility) == 0) {

        // YY Caps/Floors
        LOG("ParSensitivityAnalysis: YOYI Cap/Floor par instruments");
        for (auto y : sensitivityData_.yoyInflationCapFloorVolShiftData()) {
            string indexName = y.first;
            SensitivityScenarioData::CapFloorVolShiftParData data =
                *boost::static_pointer_cast<SensitivityScenarioData::CapFloorVolShiftParData>(y.second);
            Size n_strikes = data.shiftStrikes.size();
            Size n_expiries = data.shiftExpiries.size();
            bool singleCurve = data.parInstrumentSingleCurve;
            for (Size j = 0; j < n_strikes; ++j) {
                Real strike = data.shiftStrikes[j];
                pair<string, Size> key(indexName, j);
                for (Size k = 0; k < n_expiries; ++k) {
                    RiskFactorKey key(RiskFactorKey::KeyType::YoYInflationCapFloorVolatility, indexName, k * n_strikes + j);

                    bool recognised = true;
                    string instType;
                    try {
                        instType = data.parInstruments[j];
                        map<string, string> conventionsMap = data.parInstrumentConventions;
                        QL_REQUIRE(conventionsMap.find(instType) != conventionsMap.end(),
                            "conventions not found for zero inflation curve " << indexName << " and instrument type "
                            << instType);
                        boost::shared_ptr<Convention> convention = conventions->get(conventionsMap[instType]);
                        Period term = data.shiftExpiries[k];
                        if (instType == "ZIS") {
                            makeYoYCapFloor(simMarket, indexName, term, strike, convention, singleCurve, true,
                                data.discountCurve, key);
                        } else if (instType == "YYS") {
                            makeYoYCapFloor(simMarket, indexName, term, strike, convention, singleCurve, false,
                                data.discountCurve, key);
                        } else
                            recognised = false;
                        if (j == 0)
                            yoyCapFloorPillars_[indexName].push_back(term);
                    } catch (const std::exception& e) {
                        if (continueOnError_) {
                            ALOG(StructuredAnalyticsErrorMessage("par sensitivity conversion",
                                                                 "skipping par instrument for yoy cap floor index " +
                                                                     indexName + ": " + e.what()));
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

void ParSensitivityAnalysis::augmentRelevantRiskFactors() {
    LOG("Augment relevant risk factors, starting with " << relevantRiskFactors_.size() << " risk factors.");
    std::set<ore::analytics::RiskFactorKey> addFactors1 = relevantRiskFactors_, addFactors2, tmp;
    do {
        // DLOG("new pass with " << addFactors1.size() << " risk factors.");
        for (auto const& r : addFactors1) {
            // DLOG("candidate risk factor " << r);
            for (auto const& s : parHelpers_) {
                if (similar(s.first, r)) {
                    tmp.insert(s.first);
                    // DLOG("factor " << r << ": found similar risk factor " << s.first);
                    for (auto const& d : parHelperDependencies_[s.first]) {
                        if (std::find(relevantRiskFactors_.begin(), relevantRiskFactors_.end(), d) ==
                            relevantRiskFactors_.end())
                            addFactors2.insert(d);
                    }
                }
            }
            for (auto const& s : parCaps_) {
                if (similar(s.first, r)) {
                    tmp.insert(s.first);
                    // DLOG("factor " << r << ": found similar risk factor " << s.first);
                    for (auto const& d : parHelperDependencies_[s.first]) {
                        if (std::find(relevantRiskFactors_.begin(), relevantRiskFactors_.end(), d) ==
                            relevantRiskFactors_.end())
                            addFactors2.insert(d);
                    }
                }
            }
        }
        addFactors1.swap(addFactors2);
        addFactors2.clear();
        relevantRiskFactors_.insert(tmp.begin(), tmp.end());
        tmp.clear();
    } while (addFactors1.size() > 0);
    LOG("Done, relevant risk factor size now " << relevantRiskFactors_.size());
}

namespace {
void writeSensitivity(const RiskFactorKey& a, const RiskFactorKey& b, const Real value,
                      std::map<std::pair<RiskFactorKey, RiskFactorKey>, Real>& parSensi,
                      std::set<RiskFactorKey>& parKeysNonZero, std::set<RiskFactorKey>& rawKeysNonZero) {
    if (close_enough(value, 0.0))
        return;
    parKeysNonZero.insert(a);
    rawKeysNonZero.insert(b);
    parSensi[std::make_pair(a, b)] = value;
    DLOG("ParInstrument Sensi " << a << " w.r.t. " << b << " " << setprecision(6) << value);
}
} // namespace

void ParSensitivityAnalysis::computeParInstrumentSensitivities(const boost::shared_ptr<ScenarioSimMarket>& simMarket) {

    LOG("Cache base scenario par rates and flat vols");

    if (relevantRiskFactors_.empty()) {
        DLOG("Relevant risk factors not provided");
    } else {
        DLOG("Relevant risk factors provided:");
        for (auto const& rf : relevantRiskFactors_)
            DLOG("Relevant risk factor " << rf);
    }

    // remove todays fixings from relevant indices for the scope of this method
    struct TodaysFixingsRemover {
        TodaysFixingsRemover(const std::set<std::string>& names) : today_(Settings::instance().evaluationDate()) {
            Date today = Settings::instance().evaluationDate();
            for (auto const& n : names) {
                TimeSeries<Real> t = IndexManager::instance().getHistory(n);
                if (t[today] != Null<Real>()) {
                    DLOG("removing todays fixing (" << std::setprecision(6) << t[today] << ") from " << n);
                    savedFixings_.insert(std::make_pair(n, t[today]));
                    t[today] = Null<Real>();
                    IndexManager::instance().setHistory(n, t);
                }
            }
        }
        ~TodaysFixingsRemover() {
            for (auto const& p : savedFixings_) {
                TimeSeries<Real> t = IndexManager::instance().getHistory(p.first);
                t[today_] = p.second;
                IndexManager::instance().setHistory(p.first, t);
                DLOG("restored todays fixing (" << std::setprecision(6) << p.second << ") for " << p.first);
            }
        }
        const Date today_;
        std::set<std::pair<std::string, Real>> savedFixings_;
    };
    TodaysFixingsRemover fixingRemover(removeTodaysFixingIndices_);

    // We must have a ShiftScenarioGenerator
    boost::shared_ptr<ScenarioGenerator> simMarketScenGen = simMarket->scenarioGenerator();
    boost::shared_ptr<ShiftScenarioGenerator> scenarioGenerator = 
        boost::dynamic_pointer_cast<ShiftScenarioGenerator>(simMarketScenGen);
    
    simMarket->reset();
    scenarioGenerator->reset();
    simMarket->update(asof_);

    if (!relevantRiskFactors_.empty())
        augmentRelevantRiskFactors();
    createParInstruments(simMarket);

    map<RiskFactorKey, Real> parRatesBase, parCapVols; // for both ir and yoy caps

    for (auto& p : parHelpers_) {
        try {
            Real parRate = impliedQuote(p.second);
            parRatesBase[p.first] = parRate;
            
            // Populate zero and par shift size for the current risk factor
            populateShiftSizes(p.first, parRate, simMarket);

        } catch (const std::exception& e) {
            QL_FAIL("could not imply quote for par helper " << p.first << ": " << e.what());
        }
    }

    for (auto& c : parCaps_) {
        
        QL_REQUIRE(parCapsYts_.count(c.first) > 0,
                   "computeParInstrumentSensitivities(): no cap yts found for key " << c.first);
        QL_REQUIRE(parCapsVts_.count(c.first) > 0,
                   "computeParInstrumentSensitivities(): no cap vts found for key " << c.first);

        Real price = c.second->NPV();
        Volatility parVol = impliedVolatility<QuantLib::CapFloor>(*c.second, price, parCapsYts_.at(c.first), 0.01,
                                                                  parCapsVts_.at(c.first)->volatilityType(),
                                                                  parCapsVts_.at(c.first)->displacement());
        parCapVols[c.first] = parVol;
        TLOG("Fair implied cap volatility for key " << c.first << " is " << 
            std::fixed << std::setprecision(12) << parVol << ".");

        // Populate zero and par shift size for the current risk factor
        populateShiftSizes(c.first, parVol, simMarket);
    }

    for (auto& c : parYoYCaps_) {

        QL_REQUIRE(parYoYCapsYts_.count(c.first) > 0,
                   "computeParInstrumentSensitivities(): no cap yts found for key " << c.first);
        QL_REQUIRE(parYoYCapsIndex_.count(c.first) > 0,
                   "computeParInstrumentSensitivities(): no cap index found for key " << c.first);
        QL_REQUIRE(parYoYCapsVts_.count(c.first) > 0,
                   "computeParInstrumentSensitivities(): no cap vts found for key " << c.first);

        Real price = c.second->NPV();
        Volatility parVol = impliedVolatility<QuantLib::YoYInflationCapFloor, QuantLib::YoYInflationIndex>(
            *c.second, price, parYoYCapsYts_.at(c.first), 0.01, parYoYCapsVts_.at(c.first)->volatilityType(),
            parYoYCapsVts_.at(c.first)->displacement(), parYoYCapsIndex_.at(c.first));
        parCapVols[c.first] = parVol;
        TLOG("Fair implied yoy cap volatility for key " << c.first << " is " <<
            std::fixed << std::setprecision(12) << parVol << ".");

        // Populate zero and par shift size for the current risk factor
        populateShiftSizes(c.first, parVol, simMarket);
    }

    LOG("Caching base scenario par rates and float vols done.");

    /****************************************************************
     * Discount curve instrument fair rate sensitivity to zero shifts
     * Index curve instrument fair rate sensitivity to zero shifts
     * Cap/Floor flat vol sensitivity to optionlet vol shifts
     *
     * Step 3:
     * - Apply all single up-shift scenarios,
     * - Compute respective fair par rates and flat vols
     * - Compute par rate / flat vol sensitivities
     */
    LOG("Compute par rate and flat vol sensitivities");

    vector<ShiftScenarioGenerator::ScenarioDescription> desc = scenarioGenerator->scenarioDescriptions();
    QL_REQUIRE(desc.size() == scenarioGenerator->samples(), "descriptions size " << desc.size() <<
        " does not match samples " << scenarioGenerator->samples());

    std::set<RiskFactorKey> parKeysCheck, parKeysNonZero;
    std::set<RiskFactorKey> rawKeysCheck, rawKeysNonZero;

    for (auto const& p : parHelpers_) {
	parKeysCheck.insert(p.first);
    }

    for(auto const& p: parCaps_) {
	parKeysCheck.insert(p.first);
    }

    for(auto const& p: parYoYCaps_) {
	parKeysCheck.insert(p.first);
    }

    for (Size i = 1; i < scenarioGenerator->samples(); ++i) {

        simMarket->update(asof_);

        // use single "UP" shift scenarios only, use only scenarios relevant for par instruments,
        // use relevant scenarios only, if specified
        // ignore risk factor types that have been disabled
        if (desc[i].type() != ShiftScenarioGenerator::ScenarioDescription::Type::Up ||
            !isParType(desc[i].key1().keytype) || typesDisabled_.count(desc[i].key1().keytype) == 1 ||
            !(relevantRiskFactors_.empty() || relevantRiskFactors_.find(desc[i].key1()) != relevantRiskFactors_.end()))
            continue;

        // Since we are not using ValuationEngine we need to manually perform the trade updates here
        // TODO - explore means of utilising valuation engine
        if (ObservationMode::instance().mode() == ObservationMode::Mode::Disable) {
            for (auto it : parHelpers_)
                it.second->deepUpdate();
            for (auto it : parCaps_)
                it.second->deepUpdate();
            for (auto it : parYoYCaps_)
                it.second->deepUpdate();
        }

        rawKeysCheck.insert(desc[i].key1());

        // Get the absolute shift size and skip if close to zero

        Real shiftSize = getShiftSize(desc[i].key1(), sensitivityData_, simMarket);

        if (close_enough(shiftSize, 0.0)) {
            ALOG("Shift size for " << desc[i].key1() << " is zero, skipping");
            continue;
        }

        // process par helpers

        for (auto const& p : parHelpers_) {

            // skip if par helper has no sensi to zero risk factor (except the special treatment below kicks in)

            if (p.second->isCalculated() &&
                (p.first.keytype != RiskFactorKey::KeyType::SurvivalProbability || p.first != desc[i].key1()))
                continue;

            // compute fair and base quotes

            Real fair = impliedQuote(p.second);
            auto base = parRatesBase.find(p.first);
            QL_REQUIRE(base != parRatesBase.end(), "internal error: did not find parRatesBase[" << p.first << "]");

            Real tmp = (fair - base->second) / shiftSize;

            // special treatments for certain risk factors

            // for curves with survival probabilities going to zero quickly we might see a sensitivity
            // that is close to zero, which we sanitise here in order to prevent the Jacobi matrix
            // getting ill-conditioned or even singular

            if (p.first.keytype == RiskFactorKey::KeyType::SurvivalProbability && p.first == desc[i].key1() &&
                std::abs(tmp) < 0.01) {
                WLOG("Setting Diagonal Default Curve Sensi " << p.first << " w.r.t. " << desc[i].key1()
                                                             << " to 0.01 (got " << tmp << ")");
                tmp = 0.01;
            }

            // YoY diagnoal entries are 1.0

            if (p.first.keytype == RiskFactorKey::KeyType::YoYInflationCurve && p.first == desc[i].key1() &&
                close_enough(tmp, 0.0)) {
                tmp = 1.0;
            }

            // write sensitivity

            writeSensitivity(p.first, desc[i].key1(), tmp, parSensi_, parKeysNonZero, rawKeysNonZero);
        }

        // process par caps

        for (auto const& p : parCaps_) {

            if (p.second->isCalculated() && p.first != desc[i].key1())
                continue;

            Handle<OptionletVolatilityStructure> ovs = simMarket->capFloorVol(p.first.name, marketConfiguration_);
            auto yts = parCapsYts_.find(p.first);
            QL_REQUIRE(yts != parCapsYts_.end(), "internal error: did not find parCapYts[" << p.first << "]");

            Real price = p.second->NPV();
            Real fair = impliedVolatility<QuantLib::CapFloor>(*p.second, price, yts->second, 0.01,
                                                              ovs->volatilityType(), ovs->displacement());
            auto base = parCapVols.find(p.first);
            QL_REQUIRE(base != parCapVols.end(), "internal error: did not find parCapVols[" << p.first << "]");

            Real tmp = (fair - base->second) / shiftSize;

            // ensure Jacobi matrix is regular and not (too) ill-conditioned, this is necessary because
            // a) the shift size used to compute dpar / dzero might be close to zero and / or
            // b) the implied vol calculation has numerical inaccuracies

            if (p.first == desc[i].key1() && std::abs(tmp) < 0.01) {
                WLOG("Setting Diagonal CapFloorVol Sensi " << p.first << " w.r.t. " << desc[i].key1()
                                                           << " to 0.01 (got " << tmp << ")");
                tmp = 0.01;
            }

            // write sensitivity

            writeSensitivity(p.first, desc[i].key1(), tmp, parSensi_, parKeysNonZero, rawKeysNonZero);
        }

        // process par yoy caps

        for (auto const& p : parYoYCaps_) {

            if (p.second->isCalculated() && p.first != desc[i].key1())
                continue;

            Handle<QuantExt::YoYOptionletVolatilitySurface> ovs =
                simMarket->yoyCapFloorVol(p.first.name, marketConfiguration_);
            auto yts = parYoYCapsYts_.find(p.first);
            auto index = parYoYCapsIndex_.find(p.first);
            QL_REQUIRE(yts != parYoYCapsYts_.end(), "internal error: did not find parYoYCapsYts[" << p.first << "]");
            QL_REQUIRE(index != parYoYCapsIndex_.end(),
                       "internal error: did not find parYoYCapsIndex[" << p.first << "]");

            Real price = p.second->NPV();
            Real fair = impliedVolatility<QuantLib::YoYInflationCapFloor, QuantLib::YoYInflationIndex>(
                *p.second, price, yts->second, 0.01, ovs->volatilityType(), ovs->displacement(), index->second);
            auto base = parCapVols.find(p.first);
            QL_REQUIRE(base != parCapVols.end(), "internal error: did not find parCapVols[" << p.first << "]");

            Real tmp = (fair - base->second) / shiftSize;

            // ensure Jacobi matrix is regular and not (too) ill-conditioned, this is necessary because
            // a) the shift size used to compute dpar / dzero might be close to zero and / or
            // b) the implied vol calculation has numerical inaccuracies

            if (p.first == desc[i].key1() && std::abs(tmp) < 0.01) {
                WLOG("Setting Diagonal CapFloorVol Sensi " << p.first << " w.r.t. " << desc[i].key1()
                                                           << " to 0.01 (got " << tmp << ")");
                tmp = 0.01;
            }

            // write sensitivity

            writeSensitivity(p.first, desc[i].key1(), tmp, parSensi_, parKeysNonZero, rawKeysNonZero);
        }

    } // end of loop over samples

    // check for
    // a) par instruments which have no sensitivity to any of the risk factors
    // b) risk factors w.r.t. which no par instrument has a sensitivity
    std::set<RiskFactorKey> parKeysZero, rawKeysZero;
    std::set_difference(parKeysCheck.begin(), parKeysCheck.end(), parKeysNonZero.begin(), parKeysNonZero.end(),
                        std::inserter(parKeysZero, parKeysZero.begin()));
    std::set_difference(rawKeysCheck.begin(), rawKeysCheck.end(), rawKeysNonZero.begin(), rawKeysNonZero.end(),
                        std::inserter(rawKeysZero, rawKeysZero.begin()));
    for (auto const& k : parKeysZero) {
        WLOG("Found par instrument which has no sensitivity to any of the risk factors: \"" << k << "\"");
    }
    for (auto const& k : rawKeysZero) {
        WLOG("Found risk factor w.r.t. which no par instrument has a sensitivity: \"" << k << "\"");
    }

    LOG("Computing par rate and flat vol sensitivities done");

} // namespace sensitivity

void ParSensitivityAnalysis::alignPillars() {
    LOG("Align simulation market pillars to actual latest relevant dates of par instruments");
    // If any of the yield curve types are still active, align the pillars.
    if (typesDisabled_.count(RiskFactorKey::KeyType::DiscountCurve) == 0 ||
        typesDisabled_.count(RiskFactorKey::KeyType::YieldCurve) == 0 ||
        typesDisabled_.count(RiskFactorKey::KeyType::IndexCurve) == 0) {
        for (auto const& p : yieldCurvePillars_) {
            simMarketParams_->setYieldCurveTenors(p.first, p.second);
            DLOG("yield curve tenors for " << p.first << " set (" << p.second.size() << ")");
            for (auto const& t : p.second)
                TLOG("set tenor " << t.length() << " " << t.units())
        }
    }
    if (typesDisabled_.count(RiskFactorKey::KeyType::OptionletVolatility) == 0) {
        for (auto const& p : capFloorPillars_) {
            simMarketParams_->setCapFloorVolExpiries(p.first, p.second);
            DLOG("cap floor expiries for " << p.first << " set (" << p.second.size() << ")");
            for (auto const& t : p.second)
                TLOG("set tenor " << t.length() << " " << t.units())
        }
    }
    if (typesDisabled_.count(RiskFactorKey::KeyType::YoYInflationCapFloorVolatility) == 0) {
        for (auto const& p : yoyCapFloorPillars_) {
            simMarketParams_->setYoYInflationCapFloorVolExpiries(p.first, p.second);
            DLOG("yoy cap floor expiries for " << p.first << " set (" << p.second.size() << ")");
            for (auto const& t : p.second)
                TLOG("set tenor " << t.length() << " " << t.units())
        }
    }
    if (typesDisabled_.count(RiskFactorKey::KeyType::SurvivalProbability) == 0) {
        for (auto const& p : cdsPillars_) {
            simMarketParams_->setDefaultTenors(p.first, p.second);
            DLOG("default expiries for " << p.first << " set (" << p.second.size() << ")");
            for (auto const& t : p.second)
                TLOG("set tenor " << t.length() << " " << t.units())
        }
    }
    if (typesDisabled_.count(RiskFactorKey::KeyType::ZeroInflationCurve) == 0) {
        for (auto const& p : zeroInflationPillars_) {
            simMarketParams_->setZeroInflationTenors(p.first, p.second);
            DLOG("zero inflation expiries for " << p.first << " set (" << p.second.size() << ")");
            for (auto const& t : p.second)
                TLOG("set tenor " << t.length() << " " << t.units())
        }
    }
    if (typesDisabled_.count(RiskFactorKey::KeyType::YoYInflationCurve) == 0) {
        for (auto const& p : yoyInflationPillars_) {
            simMarketParams_->setYoyInflationTenors(p.first, p.second);
            DLOG("yoy inflation expiries for " << p.first << " set (" << p.second.size() << ")");
            for (auto const& t : p.second)
                TLOG("set tenor " << t.length() << " " << t.units())
        }
    }
    LOG("Alignment of pillars done.");
}

bool ParSensitivityAnalysis::isParType(RiskFactorKey::KeyType type) { return parTypes_.find(type) != parTypes_.end(); }

void ParSensitivityAnalysis::disable(const set<RiskFactorKey::KeyType>& types) {
    // Insert only types that are available for par sensitivity analysis in to the set of disabled types.
    for (const auto& type : types) {
        if (isParType(type)) {
            typesDisabled_.insert(type);
        }
    }
}

std::pair<boost::shared_ptr<Instrument>, Date>
ParSensitivityAnalysis::makeSwap(const boost::shared_ptr<Market>& market, string ccy, string indexName,
                                 string yieldCurveName, string equityForecastCurveName, Period term,
                                 const boost::shared_ptr<Convention>& convention, bool singleCurve,
                                 std::set<ore::analytics::RiskFactorKey>& parHelperDependencies_,
				 const string& expDiscountCurve) {
    // Curve priorities, use in the following order if ccy/indexName/yieldCurveName strings are not blank
    // 1) singleCurve = false
    //    - discounts: discountCurve(ccy) -> yieldCurve(yieldCurveName)
    //    - forwards:  iborIndex(indexName) -> iborIndex(conventions index name)
    // 2) singleCurve = true
    //    - discounts: iborIndex(indexName) -> yieldCurve(yieldCurveName) -> discountCurve(ccy)
    //    - forwards:  iborIndex(indexName) -> yieldCurve(yieldCurveName) -> discountCurve(ccy)
    boost::shared_ptr<Conventions> conventions = InstrumentConventions::instance().conventions(); 
    boost::shared_ptr<IRSwapConvention> conv = boost::dynamic_pointer_cast<IRSwapConvention>(convention);
    QL_REQUIRE(conv, "convention not recognised, expected IRSwapConvention");
    string name = indexName != "" ? indexName : conv->indexName();
    boost::shared_ptr<IborIndex> index;
    Handle<YieldTermStructure> discountCurve;
    if (market == nullptr) {
        index = parseIborIndex(name);
    } else {
        if (!expDiscountCurve.empty()) {
            // Look up the explicit discount curve in the market
            boost::shared_ptr<IborIndex> dummyIndex;
            if (tryParseIborIndex(expDiscountCurve, dummyIndex)) {
                auto discountIndex = market->iborIndex(expDiscountCurve, marketConfiguration_);
                discountCurve = discountIndex->forwardingTermStructure();
            } else {
                discountCurve = market->yieldCurve(expDiscountCurve, marketConfiguration_);
            }
        } else if (ccy != "")
            discountCurve = market->discountCurve(ccy, marketConfiguration_);
        else if (yieldCurveName != "")
            discountCurve = market->yieldCurve(yieldCurveName, marketConfiguration_);
        else if (equityForecastCurveName != "")
            discountCurve = market->equityForecastCurve(equityForecastCurveName, marketConfiguration_);

        index = *market->iborIndex(name, marketConfiguration_);

        if (singleCurve) {
            if (indexName != "")
                discountCurve = index->forwardingTermStructure();
            else if (yieldCurveName != "") {
                index = index->clone(market->yieldCurve(yieldCurveName, marketConfiguration_));
                discountCurve = market->yieldCurve(yieldCurveName, marketConfiguration_);
            } else if (ccy != "")
                index = index->clone(market->discountCurve(ccy, marketConfiguration_));
            else if (equityForecastCurveName != "") {
                index = index->clone(market->equityForecastCurve(equityForecastCurveName, marketConfiguration_));
                discountCurve = market->equityForecastCurve(equityForecastCurveName, marketConfiguration_);
            } else
                QL_FAIL("Discount curve undetermined for Swap (ccy=" << ccy << ")");
        }
    }

    if(!singleCurve)
        parHelperDependencies_.emplace(RiskFactorKey::KeyType::IndexCurve, name, 0);

    boost::shared_ptr<Swap> helper;
    Date latestRelevantDate;

    auto bmaIndex = boost::dynamic_pointer_cast<QuantExt::BMAIndexWrapper>(index);
    if (bmaIndex) {
        // FIXME do we want to remove today's historic fixing from the index as we do for the Ibor case?
        helper = boost::shared_ptr<FixedBMASwap>(
                     MakeFixedBMASwap(term, bmaIndex->bma(), 0.0, 0 * Days)
                     .withBMALegTenor(3*Months));
        // need to do very little with the factory, as the market conventions are default
        // should maybe discount with Libor, as this is how we assume the quotes come in.
        boost::shared_ptr<AverageBMACoupon> lastCoupon = boost::dynamic_pointer_cast<AverageBMACoupon>(helper->leg(1).back());
        latestRelevantDate = std::max(helper->maturityDate(), lastCoupon->fixingDates().end()[-2]);
    } else if (conv->hasSubPeriod()) {
        removeTodaysFixingIndices_.insert(index->name());
        auto subPeriodSwap = boost::shared_ptr<SubPeriodsSwap>(MakeSubPeriodsSwap(term, index, 0.0, Period(conv->floatFrequency()), 0*Days)
            .withSettlementDays(index->fixingDays())
            .withFixedLegDayCount(conv->fixedDayCounter())
            .withFixedLegTenor(Period(conv->fixedFrequency()))
            .withFixedLegConvention(conv->fixedConvention())
            .withFixedLegCalendar(conv->fixedCalendar())
            .withSubCouponsType(conv->subPeriodsCouponType()));
        
        latestRelevantDate = subPeriodSwap->maturityDate();
        boost::shared_ptr<FloatingRateCoupon> lastCoupon =
            boost::dynamic_pointer_cast<FloatingRateCoupon>(subPeriodSwap->floatLeg().back());
        helper = subPeriodSwap;
        #ifdef QL_USE_INDEXED_COUPON
            /* May need to adjust latestRelevantDate if you are projecting libor based
            on tenor length rather than from accrual date to accrual date. */
            Date fixingValueDate = indexWithoutFixings->valueDate(lastCoupon->fixingDate());
            Date endValueDate = indexWithoutFixings->maturityDate(fixingValueDate);
            latestRelevantDate = std::max(latestRelevantDate, endValueDate);
        #else
            /* Subperiods coupons do not have a par approximation either... */
            if (boost::dynamic_pointer_cast<QuantExt::SubPeriodsCoupon1>(lastCoupon)) {
                Date fixingValueDate = index->valueDate(lastCoupon->fixingDate());
                Date endValueDate = index->maturityDate(fixingValueDate);
                latestRelevantDate = std::max(latestRelevantDate, endValueDate);
            }
        #endif
    } else {
        removeTodaysFixingIndices_.insert(index->name());
        helper = boost::shared_ptr<VanillaSwap>(
                     MakeVanillaSwap(term, index, 0.0, 0 * Days)
                     .withSettlementDays(index->fixingDays())
                     .withFixedLegDayCount(conv->fixedDayCounter())
                     .withFixedLegTenor(Period(conv->fixedFrequency()))
                     .withFixedLegConvention(conv->fixedConvention())
                     .withFixedLegTerminationDateConvention(conv->fixedConvention())
                     .withFixedLegCalendar(conv->fixedCalendar())
                     .withFloatingLegCalendar(conv->fixedCalendar()));
        boost::shared_ptr<IborCoupon> lastCoupon = boost::dynamic_pointer_cast<IborCoupon>(helper->leg(1).back());
        latestRelevantDate = std::max(helper->maturityDate(), lastCoupon->fixingEndDate());
    }

    if (market) {
        boost::shared_ptr<PricingEngine> swapEngine = boost::make_shared<DiscountingSwapEngine>(discountCurve);
        helper->setPricingEngine(swapEngine);
    }

    // set pillar date
    return std::pair<boost::shared_ptr<Instrument>, Date>(helper, latestRelevantDate);
}

std::pair<boost::shared_ptr<Instrument>, Date>
ParSensitivityAnalysis::makeDeposit(const boost::shared_ptr<Market>& market, 
                                    string ccy, string indexName, string yieldCurveName, string equityForecastCurveName,
                                    Period term, const boost::shared_ptr<Convention>& convention) {
    
    // Curve priorities, use in the following order if ccy/indexName/yieldCurveName strings are not blank
    // Single curve setting only
    // - discounts: iborIndex(indexName) -> yieldCurve(yieldCurveName) -> discountCurve(ccy)
    // - forwards:  iborIndex(indexName) -> yieldCurve(yieldCurveName) -> discountCurve(ccy)
    boost::shared_ptr<Conventions> conventions = InstrumentConventions::instance().conventions();
    boost::shared_ptr<DepositConvention> conv = boost::dynamic_pointer_cast<DepositConvention>(convention);
    QL_REQUIRE(conv, "convention not recognised, expected DepositConvention");
    boost::shared_ptr<IborIndex> index;
    if (indexName == "" && conv->indexBased()) {
        // At this point, we may have an overnight index or an ibor index
        if (isOvernightIndex(conv->index())) {
            index = parseIborIndex(conv->index());
        } else {
            index = parseIborIndex(conv->index() + "-" + to_string(term));
        }
    } else if (indexName != "") {
        if (market != nullptr) {
            index = market->iborIndex(indexName, marketConfiguration_).currentLink();
        } else {
            index = parseIborIndex(indexName);
        }
    }
    boost::shared_ptr<Deposit> helper;
    if (index != nullptr) {
        helper = boost::make_shared<Deposit>(1.0, 0.0, term, index->fixingDays(), index->fixingCalendar(),
                                             index->businessDayConvention(), index->endOfMonth(), index->dayCounter(),
                                             asof_, true, 0 * Days);
    } else {
        QL_REQUIRE(!conv->indexBased(), "expected non-index-based deposit convention");
        helper =
            boost::make_shared<Deposit>(1.0, 0.0, term, conv->settlementDays(), conv->calendar(), conv->convention(),
                                        conv->eom(), conv->dayCounter(), asof_, true, 0 * Days);
    }
    RelinkableHandle<YieldTermStructure> engineYts;
    boost::shared_ptr<PricingEngine> depositEngine = boost::make_shared<DepositEngine>(engineYts);
    helper->setPricingEngine(depositEngine);
    if (market != nullptr) {
        if (indexName != "")
            engineYts.linkTo(*index->forwardingTermStructure());
        else if (yieldCurveName != "")
            engineYts.linkTo(*market->yieldCurve(yieldCurveName, marketConfiguration_));
        else if (equityForecastCurveName != "")
            engineYts.linkTo(*market->equityForecastCurve(equityForecastCurveName, marketConfiguration_));
        else if (ccy != "")
            engineYts.linkTo(*market->discountCurve(ccy, marketConfiguration_));
        else
            QL_FAIL("Yield term structure not found for deposit (ccy=" << ccy << ")");
    }
    // set pillar date
    Date latestRelevantDate = helper->maturityDate();
    return std::pair<boost::shared_ptr<Instrument>, Date>(helper, latestRelevantDate);
}

std::pair<boost::shared_ptr<Instrument>, Date>
ParSensitivityAnalysis::makeFRA(const boost::shared_ptr<Market>& market, string ccy, string indexName, 
    string yieldCurveName, string equityForecastCurveName, Period term, const boost::shared_ptr<Convention>& convention) {
    // Curve priorities, use in the following order if ccy/indexName/yieldCurveName strings are not blank
    // - discounts: discountCurve(ccy) -> yieldCurve(yieldCurveName) -> iborIndex(indexName)
    // - forwards:  iborIndex(indexName) -> yieldCurve(yieldCurveName) -> discountCurve(ccy)
    boost::shared_ptr<Conventions> conventions = InstrumentConventions::instance().conventions();
    boost::shared_ptr<FraConvention> conv = boost::dynamic_pointer_cast<FraConvention>(convention);
    QL_REQUIRE(conv, "convention not recognised, expected FraConvention");
    string name = indexName != "" ? indexName : conv->indexName();
    boost::shared_ptr<IborIndex> index;
    if (market != nullptr) {
        index = *market->iborIndex(name, marketConfiguration_);
        if (indexName == "") {
            if (yieldCurveName != "")
                index = market->iborIndex(name)->clone(market->yieldCurve(yieldCurveName, marketConfiguration_));
            else if (equityForecastCurveName != "")
                index = market->iborIndex(name)->clone(market->equityForecastCurve(equityForecastCurveName, marketConfiguration_));
            else if (ccy != "")
                index = market->iborIndex(name)->clone(market->discountCurve(ccy, marketConfiguration_));
            else
                QL_FAIL("index curve not identified for FRA (ccy=" << ccy << ")");
        }
    } else {
        index = parseIborIndex(name);
    }
    boost::shared_ptr<IborIndex> fraConvIdx = ore::data::parseIborIndex(
        conv->indexName(), index->forwardingTermStructure()); // used for setting up the FRA
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
    Date asof_adj = fraCal.adjust(asof_); // same as in FraRateHelper
    Date todaySpot = fraConvIdx->valueDate(asof_adj);
    Date valueDate =
        fraCal.advance(todaySpot, startTerm, fraConvIdx->businessDayConvention(), fraConvIdx->endOfMonth());
    Date maturityDate = fraConvIdx->maturityDate(valueDate);
    Handle<YieldTermStructure> ytsTmp;
    if (market != nullptr) {
        if (ccy != "")
            ytsTmp = market->discountCurve(ccy, marketConfiguration_);
        else if (yieldCurveName != "")
            ytsTmp = market->yieldCurve(yieldCurveName, marketConfiguration_);
        else if (equityForecastCurveName != "")
            ytsTmp = market->equityForecastCurve(equityForecastCurveName, marketConfiguration_);
        else
            ytsTmp = index->forwardingTermStructure();
    } else {
        // FRA instrument requires non-empty curves for its construction below
        ytsTmp = Handle<YieldTermStructure>(boost::make_shared<FlatForward>(0, NullCalendar(), 0.00, Actual365Fixed()));
        fraConvIdx = fraConvIdx->clone(ytsTmp);
    }
    auto helper =
        boost::make_shared<ForwardRateAgreement>(valueDate, maturityDate, Position::Long, 0.0, 1.0, fraConvIdx, ytsTmp);
    // set pillar date
    // yieldCurvePillars_[indexName == "" ? ccy : indexName].push_back((maturityDate - asof_) *
    // Days);
    return std::pair<boost::shared_ptr<Instrument>, Date>(helper, maturityDate);
}

std::pair<boost::shared_ptr<Instrument>, Date>
ParSensitivityAnalysis::makeOIS(const boost::shared_ptr<Market>& market, string ccy, string indexName,
                                string yieldCurveName, string equityForecastCurveName, Period term,
                                const boost::shared_ptr<Convention>& convention, bool singleCurve,
                                std::set<ore::analytics::RiskFactorKey>& parHelperDependencies_,
				const std::string& expDiscountCurve) {
    // Curve priorities, use in the following order if ccy/indexName/yieldCurveName strings are not blank
    // 1) singleCurve = false
    //    - discounts: discountCurve(ccy) -> yieldCurve(yieldCurveName)
    //    - forwards:  iborIndex(indexName) -> iborIndex(conventions index name)
    // 2) singleCurve = true
    //    - discounts: iborIndex(indexName) -> yieldCurve(yieldCurveName) -> discountCurve(ccy)
    //    - forwards:  iborIndex(indexName) -> yieldCurve(yieldCurveName) -> discountCurve(ccy)
    auto conventions = InstrumentConventions::instance().conventions();
    boost::shared_ptr<OisConvention> conv = boost::dynamic_pointer_cast<OisConvention>(convention);
    QL_REQUIRE(conv, "convention not recognised, expected OisConvention");
    string name = indexName != "" ? indexName : conv->indexName();
    boost::shared_ptr<IborIndex> index;
    if (market != nullptr) {
        index = market->iborIndex(name, marketConfiguration_).currentLink();
    } else {
        // make ois below requires a non empty ts
        auto h = Handle<YieldTermStructure>(boost::make_shared<FlatForward>(0, NullCalendar(), 0.00, Actual365Fixed()));
        index = parseIborIndex(name, h);
    }
    boost::shared_ptr<OvernightIndex> overnightIndexTmp = boost::dynamic_pointer_cast<OvernightIndex>(index);
    QL_REQUIRE(overnightIndexTmp, "ParSensitivityAnalysis::makeOIS(): expected OIS index, got  \"" << name << "\"");
    Handle<YieldTermStructure> indexTs = overnightIndexTmp->forwardingTermStructure();
    if (market != nullptr && singleCurve) {
        if (indexName != "")
            indexTs = overnightIndexTmp->forwardingTermStructure();
        else if (yieldCurveName != "")
            indexTs = market->yieldCurve(yieldCurveName, marketConfiguration_);
        else if (equityForecastCurveName != "")
            indexTs = market->equityForecastCurve(equityForecastCurveName, marketConfiguration_);
        else if (ccy != "")
            indexTs = market->discountCurve(ccy, marketConfiguration_);
        else
            QL_FAIL("Index curve not identified in ParSensitivityAnalysis::makeOIS (ccy=" << ccy << ")");
    }
    boost::shared_ptr<OvernightIndex> overnightIndex =
        boost::dynamic_pointer_cast<OvernightIndex>(overnightIndexTmp->clone(indexTs));
    
    // FIXME do we want to remove today's historic fixing from the index as we do for the Ibor case?
    boost::shared_ptr<OvernightIndexedSwap> helper = MakeOIS(term, overnightIndex, Null<Rate>(), 0 * Days)
        .withTelescopicValueDates(true);

    if (market != nullptr) {
        RelinkableHandle<YieldTermStructure> engineYts;
        if (singleCurve) {
            if (indexName != "")
                engineYts.linkTo(*indexTs);
            else if (yieldCurveName != "")
                engineYts.linkTo(*market->yieldCurve(yieldCurveName, marketConfiguration_));
            else if (equityForecastCurveName != "")
                engineYts.linkTo(*market->equityForecastCurve(equityForecastCurveName, marketConfiguration_));
            else if (ccy != "")
                engineYts.linkTo(*market->discountCurve(ccy, marketConfiguration_));
            else
                QL_FAIL("discount curve not identified in ParSensitivityAnalysis::makeOIS, single curve (ccy=" << ccy << ")");
        } else {
            if (!expDiscountCurve.empty()) {
                // Look up the explicit discount curve in the market
                auto discountIndex = market->iborIndex(expDiscountCurve, marketConfiguration_);
                engineYts.linkTo(*discountIndex->forwardingTermStructure());
            } else if (ccy != "")
                engineYts.linkTo(*market->discountCurve(ccy, marketConfiguration_));
            else if (yieldCurveName != "")
                engineYts.linkTo(*market->yieldCurve(yieldCurveName, marketConfiguration_));
            else if (equityForecastCurveName != "")
                engineYts.linkTo(*market->equityForecastCurve(equityForecastCurveName, marketConfiguration_));
            else
                QL_FAIL("discount curve not identified in ParSensitivityAnalysis::makeOIS, multi curve (ccy=" << ccy
                                                                                                              << ")");
        }
        boost::shared_ptr<PricingEngine> swapEngine = boost::make_shared<DiscountingSwapEngine>(engineYts);
        helper->setPricingEngine(swapEngine);
    }

    if(!singleCurve)
        parHelperDependencies_.emplace(RiskFactorKey::KeyType::IndexCurve, name, 0);

    // set pillar date
    Date latestRelevantDate = helper->maturityDate();
    
    return std::pair<boost::shared_ptr<Instrument>, Date>(helper, latestRelevantDate);
}

std::pair<boost::shared_ptr<QuantLib::Instrument>, Date>
ParSensitivityAnalysis::makeTenorBasisSwap(const boost::shared_ptr<Market>& market, string ccy, string shortIndexName,
                                           string longIndexName, string yieldCurveName, string equityForecastCurveName,
                                           Period term, const boost::shared_ptr<Convention>& convention,
                                           std::set<ore::analytics::RiskFactorKey>& parHelperDependencies_,
                                           const string& expDiscountCurve) {

    auto conventions = InstrumentConventions::instance().conventions();
    boost::shared_ptr<TenorBasisSwapConvention> conv =
        boost::dynamic_pointer_cast<TenorBasisSwapConvention>(convention);
    QL_REQUIRE(conv, "convention not recognised, expected TenorBasisSwapConvention");
    string shortIndexNameTmp = shortIndexName == "" ? conv->shortIndexName() : shortIndexName;
    string longIndexNameTmp = longIndexName == "" ? conv->longIndexName() : longIndexName;

    Handle<YieldTermStructure> discountCurve;
    boost::shared_ptr<IborIndex> longIndex, shortIndex;
    boost::shared_ptr<OvernightIndex> shortIndexOn;
    if (market == nullptr) {
        longIndex = parseIborIndex(longIndexNameTmp);
        shortIndex = parseIborIndex(shortIndexNameTmp);
        shortIndexOn = boost::dynamic_pointer_cast<OvernightIndex>(shortIndex);
    } else {
        if (!expDiscountCurve.empty()){
            // Look up the explicit discount curve in the market
            auto discountIndex = market->iborIndex(expDiscountCurve, marketConfiguration_);
            discountCurve = discountIndex->forwardingTermStructure();
        } else if (ccy != "")
            discountCurve = market->discountCurve(ccy, marketConfiguration_);
        else if (yieldCurveName != "")
            discountCurve = market->yieldCurve(yieldCurveName, marketConfiguration_);
        else if (equityForecastCurveName != "")
            discountCurve = market->equityForecastCurve(equityForecastCurveName, marketConfiguration_);
        shortIndex = *market->iborIndex(shortIndexNameTmp, marketConfiguration_);
        shortIndexOn = boost::dynamic_pointer_cast<OvernightIndex>(shortIndex);
        longIndex = *market->iborIndex(longIndexNameTmp, marketConfiguration_);
    }
    boost::shared_ptr<Swap> helper;
    Date latestRelevantDate;
    boost::shared_ptr<Libor> longIndexAsLibor = boost::dynamic_pointer_cast<Libor>(longIndex);
    boost::shared_ptr<Libor> shortIndexAsLibor = boost::dynamic_pointer_cast<Libor>(shortIndex);
    Calendar longIndexCalendar =
        longIndexAsLibor != nullptr ? longIndexAsLibor->jointCalendar() : longIndex->fixingCalendar();
    Calendar shortIndexCalendar =
        shortIndexAsLibor != nullptr ? shortIndexAsLibor->jointCalendar() : shortIndex->fixingCalendar();
    if (shortIndexOn) {
        // OIS vs Libor
        Date settlementDate = longIndexCalendar.advance(
            longIndexCalendar.adjust(asof_), longIndex->fixingDays() * Days);
        Schedule oisSchedule = MakeSchedule()
                                   .from(settlementDate)
                                   .to(settlementDate + term)
                                   .withTenor(conv->shortPayTenor())
                                   .withCalendar(shortIndexCalendar)
                                   .withConvention(shortIndex->businessDayConvention())
                                   .forwards();
        Schedule iborSchedule = MakeSchedule()
                                    .from(settlementDate)
                                    .to(settlementDate + term)
                                    .withTenor(longIndex->tenor())
                                    .withCalendar(longIndexCalendar)
                                    .withConvention(longIndex->businessDayConvention())
                                    .forwards();
        // FIXME do we want to remove today's historic fixing from the short index as we do for the Ibor case?
        removeTodaysFixingIndices_.insert(longIndex->name());
        helper = boost::make_shared<OvernightIndexedBasisSwap>(OvernightIndexedBasisSwap::Payer, 100.0, oisSchedule,
                                                               shortIndexOn, iborSchedule, longIndex);
        boost::shared_ptr<IborCoupon> lastCoupon1 = boost::dynamic_pointer_cast<IborCoupon>(
            boost::static_pointer_cast<OvernightIndexedBasisSwap>(helper)->iborLeg().back());
        boost::shared_ptr<QuantLib::OvernightIndexedCoupon> lastCoupon2 =
            boost::dynamic_pointer_cast<QuantLib::OvernightIndexedCoupon>(
                boost::static_pointer_cast<OvernightIndexedBasisSwap>(helper)->overnightLeg().back());
        latestRelevantDate = std::max(helper->maturityDate(),
                 std::max(lastCoupon1->fixingEndDate(),
                          shortIndexOn->fixingCalendar().advance(lastCoupon2->valueDates().back(), 1 * Days)));
    } else {
        // Libor vs Libor
        Date settlementDate = longIndexCalendar.advance(
            longIndexCalendar.adjust(asof_), longIndex->fixingDays() * Days);
        removeTodaysFixingIndices_.insert(longIndex->name());
        removeTodaysFixingIndices_.insert(shortIndex->name());
        helper = boost::make_shared<TenorBasisSwap>(settlementDate, 1.0, term, true, longIndex, 0.0, shortIndex, 0.0,
                                                    conv->shortPayTenor(), DateGeneration::Backward,
                                                    conv->includeSpread(), conv->subPeriodsCouponType());
        boost::shared_ptr<IborCoupon> lastCoupon1 = boost::dynamic_pointer_cast<IborCoupon>(
            boost::static_pointer_cast<TenorBasisSwap>(helper)->longLeg().back());
        Date maxDate2;
        boost::shared_ptr<IborCoupon> lastCoupon2 = boost::dynamic_pointer_cast<IborCoupon>(
            boost::static_pointer_cast<TenorBasisSwap>(helper)->shortLeg().back());
        if (lastCoupon2 != nullptr)
            maxDate2 = lastCoupon2->fixingEndDate();
        else {
            boost::shared_ptr<QuantExt::SubPeriodsCoupon1> lastCoupon2 = boost::dynamic_pointer_cast<QuantExt::SubPeriodsCoupon1>(
                boost::static_pointer_cast<TenorBasisSwap>(helper)->shortLeg().back());
            maxDate2 = shortIndexCalendar.advance(lastCoupon2->valueDates().back(), conv->shortPayTenor());
        }
        latestRelevantDate = std::max(helper->maturityDate(), std::max(lastCoupon1->fixingEndDate(), maxDate2));
    }
    if (market != nullptr) {
        boost::shared_ptr<PricingEngine> swapEngine = boost::make_shared<DiscountingSwapEngine>(discountCurve);
        helper->setPricingEngine(swapEngine);
    }

    parHelperDependencies_.emplace(RiskFactorKey::KeyType::IndexCurve, shortIndexNameTmp, 0);
    parHelperDependencies_.emplace(RiskFactorKey::KeyType::IndexCurve, longIndexNameTmp, 0);

    // latest date and return result
    return std::pair<boost::shared_ptr<Instrument>, Date>(helper, latestRelevantDate);
}

boost::shared_ptr<CapFloor>
ParSensitivityAnalysis::makeCapFloor(const boost::shared_ptr<Market>& market, string ccy, string indexName, Period term,
                                     Real strike, bool isAtm,
                                     std::set<ore::analytics::RiskFactorKey>& parHelperDependencies_,
				     const std::string& expDiscountCurve) {

    boost::shared_ptr<CapFloor> inst;
    auto conventions = InstrumentConventions::instance().conventions();

    if (!market) {
        // No market so just return a dummy cap
        boost::shared_ptr<IborIndex> index =
            parseIborIndex(indexName);
	QL_REQUIRE(boost::dynamic_pointer_cast<OvernightIndex>(index) == nullptr,
		   "ParSensitivityAnalysis::makeCapFloor(): OIS indices are not yet supported for par conversion");
        inst = MakeCapFloor(CapFloor::Cap, term, index, 0.03);
    } else {
        
        boost::shared_ptr<IborIndex> index = *market->iborIndex(indexName, marketConfiguration_);
	QL_REQUIRE(boost::dynamic_pointer_cast<OvernightIndex>(index) == nullptr,
		   "ParSensitivityAnalysis::makeCapFloor(): OIS indices are not yet supported for par conversion");
        QL_REQUIRE(index, "Index not found with name " << indexName);
        Handle<YieldTermStructure> discount;
        if(expDiscountCurve.empty())
            discount = market->discountCurve(ccy, marketConfiguration_);
        else
            discount = market->iborIndex(expDiscountCurve, marketConfiguration_)->forwardingTermStructure();
        QL_REQUIRE(!discount.empty(), "Discount curve not found for cap floor index " << indexName);

        // Create a dummy cap just to get the ATM rate
        // Note this construction excludes the first caplet which is what we want
        inst = MakeCapFloor(CapFloor::Cap, term, index, 0.03);
        Rate atmRate = inst->atmRate(**discount);
    	//bool isAtm = strike == Null<Real>();
        //strike = isAtm ? atmRate : strike;
        strike = strike == Null<Real>() ? atmRate : strike;
	CapFloor::Type type = strike >= atmRate ? CapFloor::Cap : CapFloor::Floor;

        // Create the actual cap or floor instrument that we will use
        if (isAtm) {
            inst = MakeCapFloor(type, term, index, atmRate);
        } else {
            inst = MakeCapFloor(type, term, index, strike);
        }
        Handle<OptionletVolatilityStructure> ovs = market->capFloorVol(indexName, marketConfiguration_);
        QL_REQUIRE(!ovs.empty(), "Optionlet volatility structure not found for index " << indexName);
        QL_REQUIRE(ovs->volatilityType() == ShiftedLognormal || ovs->volatilityType() == Normal, 
            "Optionlet volatility type " << ovs->volatilityType() << " not covered");
        boost::shared_ptr<PricingEngine> engine;
        if (ovs->volatilityType() == ShiftedLognormal) {
            engine = boost::make_shared<BlackCapFloorEngine>(discount, ovs, ovs->displacement());
        } else {
            engine = boost::make_shared<BachelierCapFloorEngine>(discount, ovs);
        }
        inst->setPricingEngine(engine);
    }
    parHelperDependencies_.emplace(RiskFactorKey::KeyType::DiscountCurve, ccy);
    parHelperDependencies_.emplace(RiskFactorKey::KeyType::IndexCurve, indexName);
    // set pillar date
    // if (generatePillar) {
    //     capFloorPillars_[ccy].push_back(term /*(end - asof_) * Days*/);
    // }

    QL_REQUIRE(inst, "empty cap/floor par instrument pointer");
    return inst;
}

std::pair<boost::shared_ptr<Instrument>, Date>
ParSensitivityAnalysis::makeCrossCcyBasisSwap(const boost::shared_ptr<Market>& market, string baseCcy, string ccy,
                                              Period term, const boost::shared_ptr<Convention>& convention,
                                              std::set<ore::analytics::RiskFactorKey>& parHelperDependencies_) {

    auto conventions = InstrumentConventions::instance().conventions();
    boost::shared_ptr<CrossCcyBasisSwapConvention> conv =
        boost::dynamic_pointer_cast<CrossCcyBasisSwapConvention>(convention);
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
        baseIndex = market->iborIndex(baseIndexName, marketConfiguration_);
        index = market->iborIndex(indexName, marketConfiguration_);
    } else {
        baseIndex =
            Handle<IborIndex>(parseIborIndex(baseIndexName));
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
            ? market->fxRate(ccy + baseCcy, marketConfiguration_)
            : Handle<Quote>(boost::make_shared<SimpleQuote>(1.0)); // multiplicative conversion into base ccy
    Real notional = 1.0 / fxSpot->value();

    RelinkableHandle<YieldTermStructure> baseDiscountCurve;
    RelinkableHandle<YieldTermStructure> discountCurve;
    boost::shared_ptr<FxIndex> fxIndex =
        boost::make_shared<FxIndex>("dummy", conv->settlementDays(), currency, baseCurrency, conv->settlementCalendar(),
                                    fxSpot, discountCurve, baseDiscountCurve);

    // LOG("Make Cross Ccy Swap for base ccy " << baseCcy << " currency " << ccy);
    // Set up first leg as spread leg, second as flat leg
    removeTodaysFixingIndices_.insert(baseIndex->name());
    removeTodaysFixingIndices_.insert(index->name());
    boost::shared_ptr<CrossCcySwap> helper;
    bool telescopicValueDates = true; // same as in the yield curve building
    
    if (baseCcy == conv->spreadIndex()->currency().code()) { // base ccy index is spread index
        if (conv->isResettable() && conv->flatIndexIsResettable()) {       // i.e. flat index leg is resettable
	    DLOG("create resettable xccy par instrument (1), convention " << conv->id());
	    helper = boost::make_shared<CrossCcyBasisMtMResetSwap>(
		baseNotional, baseCurrency, baseSchedule, *baseIndex, 0.0, // spread index leg => use fairForeignSpread
		currency, schedule, *index, 0.0, fxIndex, true,            // resettable flat index leg
		conv->paymentLag(), conv->flatPaymentLag(),
		conv->includeSpread(), conv->lookback(), conv->fixingDays(), conv->rateCutoff(), conv->isAveraged(),
		conv->flatIncludeSpread(), conv->flatLookback(), conv->flatFixingDays(), conv->flatRateCutoff(), conv->flatIsAveraged(),
		telescopicValueDates, true);                               // fair spread leg is foreign
	}
	else if (conv->isResettable() && !conv->flatIndexIsResettable()) {  // i.e. spread index leg is resettable
	    DLOG("create resettable xccy par instrument (2), convention " << conv->id());
	    helper = boost::make_shared<CrossCcyBasisMtMResetSwap>(
		notional, currency, schedule, *index, 0.0,                  // flat index leg
		baseCurrency, baseSchedule, *baseIndex, 0.0, fxIndex, true, // resettable spread index leg => use fairDomesticSpread
		conv->flatPaymentLag(), conv->paymentLag(),
		conv->flatIncludeSpread(), conv->flatLookback(), conv->flatFixingDays(), conv->flatRateCutoff(), conv->flatIsAveraged(),
		conv->includeSpread(), conv->lookback(), conv->fixingDays(), conv->rateCutoff(), conv->isAveraged(),
		telescopicValueDates, false);                               // fair spread leg is domestic
	}
	else { // not resettable
	    DLOG("create non-resettable xccy par instrument (3), convention " << conv->id());
	    helper = boost::make_shared<CrossCcyBasisSwap>(
		baseNotional, baseCurrency, baseSchedule, *baseIndex, 0.0, 1.0, // spread index leg => use fairPaySpread
		notional, currency, schedule, *index, 0.0, 1.0,                 // flat index leg
		conv->paymentLag(), conv->flatPaymentLag(),
		conv->includeSpread(), conv->lookback(), conv->fixingDays(), conv->rateCutoff(), conv->isAveraged(),
		conv->flatIncludeSpread(), conv->flatLookback(), conv->flatFixingDays(), conv->flatRateCutoff(), conv->flatIsAveraged(),
		telescopicValueDates);
	}
    }
    else { // base ccy index is flat index
	if (conv->isResettable() && conv->flatIndexIsResettable()) {
	    DLOG("create resettable xccy par instrument (4), convention " << conv->id());
	    // second leg is resettable, so the second leg is the base currency leg 
	    helper = boost::make_shared<CrossCcyBasisMtMResetSwap>(
	        notional, currency, schedule, *index, 0.0,                  // spread index leg => use fairForeignSpread
		baseCurrency, baseSchedule, *baseIndex, 0.0, fxIndex, true, // resettable flat index leg
		conv->paymentLag(), conv->flatPaymentLag(),
		conv->includeSpread(), conv->lookback(), conv->fixingDays(), conv->rateCutoff(), conv->isAveraged(),
		conv->flatIncludeSpread(), conv->flatLookback(), conv->flatFixingDays(), conv->flatRateCutoff(), conv->flatIsAveraged(),
		telescopicValueDates, true);                                // fair spread leg is foreign
	}
	else if (conv->isResettable() && !conv->flatIndexIsResettable()) {
	    DLOG("create resettable xccy par instrument (5), convention " << conv->id());
	    // second leg is resettable, so the second leg is the non-base non-flat spread leg  
	    helper = boost::make_shared<CrossCcyBasisMtMResetSwap>(
		baseNotional, baseCurrency, baseSchedule, *baseIndex, 0.0, // flat index leg
		currency, schedule, *index, 0.0, fxIndex, true,            // resettable spread index leg => use fairDomesticSpread
		conv->flatPaymentLag(), conv->paymentLag(), 
		conv->flatIncludeSpread(), conv->flatLookback(), conv->flatFixingDays(), conv->flatRateCutoff(), conv->flatIsAveraged(),
		conv->includeSpread(), conv->lookback(), conv->fixingDays(), conv->rateCutoff(), conv->isAveraged(),
		telescopicValueDates, false);                              // fair spread leg is domestic
	}
	else { // not resettable
	    DLOG("create non-resettable xccy par instrument (6), convention " << conv->id());
	    helper = boost::make_shared<CrossCcyBasisSwap>(
	        notional, currency, schedule, *index, 0.0, 1.0,                 // spread index leg => use fairPaySpread
		baseNotional, baseCurrency, baseSchedule, *baseIndex, 0.0, 1.0, // flat index leg
		conv->paymentLag(), conv->flatPaymentLag(), 
		conv->includeSpread(), conv->lookback(), conv->fixingDays(), conv->rateCutoff(), conv->isAveraged(),
		conv->flatIncludeSpread(), conv->flatLookback(), conv->flatFixingDays(), conv->flatRateCutoff(), conv->flatIsAveraged(),
		telescopicValueDates);
	}
    }
    
    
    bool isBaseDiscount = true;
    bool isNonBaseDiscount = true;
    if (market != nullptr) {
        baseDiscountCurve.linkTo(xccyYieldCurve(market, baseCcy, isBaseDiscount, marketConfiguration_).currentLink());
	discountCurve.linkTo(xccyYieldCurve(market, ccy, isNonBaseDiscount, marketConfiguration_).currentLink());
        boost::shared_ptr<PricingEngine> swapEngine =
            boost::make_shared<CrossCcySwapEngine>(baseCurrency, baseDiscountCurve, currency, discountCurve, fxSpot);
        helper->setPricingEngine(swapEngine);
    }

    if (isBaseDiscount)
        parHelperDependencies_.emplace(RiskFactorKey::KeyType::DiscountCurve, baseCcy, 0);
    else
        parHelperDependencies_.emplace(RiskFactorKey::KeyType::YieldCurve, baseCcy, 0);

    if (isNonBaseDiscount)
        parHelperDependencies_.emplace(RiskFactorKey::KeyType::DiscountCurve, ccy, 0);
    else
        parHelperDependencies_.emplace(RiskFactorKey::KeyType::YieldCurve, ccy, 0);

    parHelperDependencies_.emplace(RiskFactorKey::KeyType::IndexCurve, baseIndexName, 0);
    parHelperDependencies_.emplace(RiskFactorKey::KeyType::IndexCurve, indexName, 0);

    // set pillar date
    Date latestRelevantDate = helper->maturityDate();
    if (auto i = boost::dynamic_pointer_cast<CrossCcyBasisSwap>(helper)){
        boost::shared_ptr<IborCoupon> lastCoupon0 =
	    boost::dynamic_pointer_cast<IborCoupon>(helper->leg(0)[helper->leg(0).size() - 2]);
	boost::shared_ptr<IborCoupon> lastCoupon1 =
	    boost::dynamic_pointer_cast<IborCoupon>(helper->leg(1)[helper->leg(1).size() - 2]);
	if (lastCoupon0 != nullptr)
	    latestRelevantDate = std::max(latestRelevantDate, lastCoupon0->fixingEndDate());
	if (lastCoupon1 != nullptr)
	    latestRelevantDate = std::max(latestRelevantDate, lastCoupon1->fixingEndDate());
	// yieldCurvePillars_[ccy].push_back((latestRelevantDate - asof_) * Days);
    }

    return std::pair<boost::shared_ptr<Instrument>, Date>(helper, latestRelevantDate);
}

std::pair<boost::shared_ptr<Instrument>, Date>
ParSensitivityAnalysis::makeFxForward(const boost::shared_ptr<Market>& market, string baseCcy, string ccy, Period term,
                                      const boost::shared_ptr<Convention>& convention,
                                      std::set<ore::analytics::RiskFactorKey>& parHelperDependencies_) {

    boost::shared_ptr<FXConvention> conv = boost::dynamic_pointer_cast<FXConvention>(convention);
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
            ? market->fxRate(ccy + baseCcy, marketConfiguration_)
            : Handle<Quote>(boost::make_shared<SimpleQuote>(1.0)); // multiplicative conversion into base ccy
    Real notional = 1.0 / fxSpot->value();
    boost::shared_ptr<FxForward> helper =
        boost::make_shared<FxForward>(baseNotional, baseCurrency, notional, currency, maturity, true);
    
    bool isBaseDiscount = true;
    bool isNonBaseDiscount = true;
    if (market != nullptr) {
        Handle<YieldTermStructure> baseDiscountCurve = xccyYieldCurve(
            market, baseCcy, isBaseDiscount, marketConfiguration_);
        Handle<YieldTermStructure> discountCurve = xccyYieldCurve(
            market, ccy, isNonBaseDiscount, marketConfiguration_);
        boost::shared_ptr<PricingEngine> engine = boost::make_shared<DiscountingFxForwardEngine>(
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
    // yieldCurvePillars_[ccy].push_back((maturity - asof_) * Days);
    return std::pair<boost::shared_ptr<Instrument>, Date>(helper, maturity);
}

std::pair<boost::shared_ptr<Instrument>, Date>
ParSensitivityAnalysis::makeCDS(const boost::shared_ptr<Market>& market, string name, string ccy, Period term,
                                const boost::shared_ptr<Convention>& convention,
                                std::set<ore::analytics::RiskFactorKey>& parHelperDependencies_,
                                const std::string& expDiscountCurve) {

    boost::shared_ptr<CdsConvention> conv = boost::dynamic_pointer_cast<CdsConvention>(convention);
    QL_REQUIRE(conv, "convention not recognised, expected CdsConvention");

    boost::shared_ptr<QuantExt::CreditDefaultSwap> helper = MakeCreditDefaultSwap(term, 0.1).withNominal(1)
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
            auto discountIndex = market->iborIndex(expDiscountCurve, marketConfiguration_);
            yts = discountIndex->forwardingTermStructure();
        } else {
            yts = market->discountCurve(ccy, marketConfiguration_);
        }

        Handle<DefaultProbabilityTermStructure> dpts = market->defaultCurve(name, marketConfiguration_)->curve();
        Handle<Quote> recovery =
            market->recoveryRate(name, marketConfiguration_);

        boost::shared_ptr<PricingEngine> cdsEngine =
            boost::make_shared<QuantExt::MidPointCdsEngine>(dpts, recovery->value(), yts);
        helper->setPricingEngine(cdsEngine);
    }

    parHelperDependencies_.emplace(RiskFactorKey::KeyType::DiscountCurve, ccy, 0);

    // set pillar date
    //Date maturity = helper->maturity();
    Date maturity = conv->calendar().adjust(helper->maturity(), conv->paymentConvention());
    // cdsPillars_[name].push_back((maturity - asof_) * Days);
    return std::pair<boost::shared_ptr<Instrument>, Date>(helper, maturity);
}

boost::shared_ptr<Instrument>
ParSensitivityAnalysis::makeZeroInflationSwap(const boost::shared_ptr<Market>& market, string indexName, Period term,
                                              const boost::shared_ptr<Convention>& convention, bool singleCurve,
                                              std::set<ore::analytics::RiskFactorKey>& parHelperDependencies_,
                                              const std::string& expDiscountCurve) {

    boost::shared_ptr<InflationSwapConvention> conv = boost::dynamic_pointer_cast<InflationSwapConvention>(convention);
    QL_REQUIRE(conv, "convention not recognised, expected InflationSwapConvention");
    string name = indexName != "" ? indexName : conv->indexName();
    boost::shared_ptr<ZeroInflationIndex> index = conv->index();
    Currency currency = index->currency();
    string ccy = currency.code();
    Handle<YieldTermStructure> discountCurve;
    if (market != nullptr) {
        // Get the inflation index
        index = *market->zeroInflationIndex(name, marketConfiguration_);

        // Get the discount curve
        if (!expDiscountCurve.empty()) {
            // Look up the explicit discount curve in the market
            auto discountIndex = market->iborIndex(expDiscountCurve, marketConfiguration_);
            discountCurve = discountIndex->forwardingTermStructure();
        } else {
            // Take the default discount curve for the inflation currency from the market
            discountCurve = market->discountCurve(ccy, marketConfiguration_);
        }
    }

    // Potentially use conventions here to get an updated start date e.g. AU CPI conventions with a publication roll.
    Date start = Settings::instance().evaluationDate();
    start = getInflationSwapStart(start, *conv);
    Date end = start + term;
    boost::shared_ptr<ZeroCouponInflationSwap> helper(
        new ZeroCouponInflationSwap(ZeroCouponInflationSwap::Payer, 1.0, start, end, conv->infCalendar(),
                                    conv->infConvention(), conv->dayCounter(), 0.0, index, conv->observationLag(), CPI::AsIndex));

    if (market != nullptr) {
        boost::shared_ptr<PricingEngine> swapEngine = boost::make_shared<DiscountingSwapEngine>(discountCurve);
        helper->setPricingEngine(swapEngine);
    }

    parHelperDependencies_.emplace(RiskFactorKey::KeyType::DiscountCurve, ccy, 0);

    // set pillar date
    boost::shared_ptr<IndexedCashFlow> lastCoupon =
        boost::dynamic_pointer_cast<IndexedCashFlow>(helper->inflationLeg().back());
    Date latestRelevantDate = std::max(helper->maturityDate(), lastCoupon->fixingDate());
    zeroInflationPillars_[indexName].push_back((latestRelevantDate - asof_) * Days);
    return helper;
}

boost::shared_ptr<Instrument>
ParSensitivityAnalysis::makeYoyInflationSwap(const boost::shared_ptr<Market>& market, string indexName, Period term,
                                             const boost::shared_ptr<Convention>& convention, bool singleCurve,
                                             bool fromZero,
                                             std::set<ore::analytics::RiskFactorKey>& parHelperDependencies_,
                                             const std::string& expDiscountCurve) {

    boost::shared_ptr<InflationSwapConvention> conv = boost::dynamic_pointer_cast<InflationSwapConvention>(convention);
    QL_REQUIRE(conv, "convention not recognised, expected InflationSwapConvention");
    string name = indexName != "" ? indexName : conv->indexName();

    boost::shared_ptr<ZeroInflationIndex> zeroIndex = conv->index();
    boost::shared_ptr<YoYInflationIndex> index =
        boost::make_shared<QuantExt::YoYInflationIndexWrapper>(zeroIndex, conv->interpolated());

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
            zeroIndex = *market->zeroInflationIndex(name, marketConfiguration_);
            index = boost::make_shared<QuantExt::YoYInflationIndexWrapper>(zeroIndex, false);
        } else {
            index = *market->yoyInflationIndex(name, marketConfiguration_);
        }

        // Get the discount curve
        if (!expDiscountCurve.empty()) {
            // Look up the explicit discount curve in the market
            auto discountIndex = market->iborIndex(expDiscountCurve, marketConfiguration_);
            discountCurve = discountIndex->forwardingTermStructure();
        } else {
            // Take the default discount curve for the inflation currency from the market
            discountCurve = market->discountCurve(ccy, marketConfiguration_);
        }
    }

    boost::shared_ptr<YearOnYearInflationSwap> helper(new YearOnYearInflationSwap(
        YearOnYearInflationSwap::Payer, 1.0, fixSchedule, 0.0, conv->dayCounter(), yoySchedule, index,
        conv->observationLag(), 0.0, conv->dayCounter(), conv->infCalendar()));

    boost::shared_ptr<InflationCouponPricer> yoyCpnPricer = boost::make_shared<YoYInflationCouponPricer>(discountCurve);
    for (auto& c : helper->yoyLeg()) {
        auto cpn = boost::dynamic_pointer_cast<YoYInflationCoupon>(c);
        QL_REQUIRE(cpn, "yoy inflation coupon expected, could not cast");
        cpn->setPricer(yoyCpnPricer);
    }
    parHelperDependencies_.emplace(RiskFactorKey::KeyType::DiscountCurve, ccy, 0);

    if (fromZero) {
        parHelperDependencies_.emplace(RiskFactorKey::KeyType::ZeroInflationCurve, ccy, 0);
    }

    if (market != nullptr) {
        boost::shared_ptr<PricingEngine> swapEngine = boost::make_shared<DiscountingSwapEngine>(discountCurve);
        helper->setPricingEngine(swapEngine);
    }

    // set pillar date
    boost::shared_ptr<YoYInflationCoupon> lastCoupon =
        boost::dynamic_pointer_cast<YoYInflationCoupon>(helper->yoyLeg().back());
    Date latestRelevantDate = std::max(helper->maturityDate(), lastCoupon->fixingDate());

    yoyInflationPillars_[indexName].push_back((latestRelevantDate - asof_) * Days);
    return helper;
}

boost::shared_ptr<QuantLib::YoYInflationCapFloor>
ParSensitivityAnalysis::makeYoYCapFloor(const boost::shared_ptr<Market>& market, string indexName, Period term,
                                        Real strike, const boost::shared_ptr<Convention>& convention, bool singleCurve,
                                        bool fromZero, const std::string& expDiscountCurve, const RiskFactorKey& key) {

    boost::shared_ptr<InflationSwapConvention> conv = boost::dynamic_pointer_cast<InflationSwapConvention>(convention);
    QL_REQUIRE(conv, "convention not recognised, expected InflationSwapConvention");
    string name = indexName != "" ? indexName : conv->indexName();

    boost::shared_ptr<ZeroInflationIndex> zeroIndex = conv->index();
    boost::shared_ptr<YoYInflationIndex> index =
        boost::make_shared<QuantExt::YoYInflationIndexWrapper>(zeroIndex, conv->interpolated());

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
            zeroIndex = *market->zeroInflationIndex(name, marketConfiguration_);
            index = boost::make_shared<QuantExt::YoYInflationIndexWrapper>(zeroIndex, conv->interpolated());
        } else {
            index = *market->yoyInflationIndex(name, marketConfiguration_);
        }

        // Get the discount curve
        if (!expDiscountCurve.empty()) {
            // Look up the explicit discount curve in the market
            auto discountIndex = market->iborIndex(expDiscountCurve, marketConfiguration_);
            discountCurve = discountIndex->forwardingTermStructure();
        } else {
            // Take the default discount curve for the inflation currency from the market
            discountCurve = market->discountCurve(ccy, marketConfiguration_);
        }
    }

    // build the leg data and instrument
    Leg yoyLeg = yoyInflationLeg(yoySchedule, yoySchedule.calendar(), index, conv->observationLag())
                     .withNotionals(1.0)
                     .withPaymentDayCounter(conv->dayCounter())
                     .withRateCurve(discountCurve);

    if(market == nullptr)
        return boost::make_shared<YoYInflationCapFloor>(YoYInflationCapFloor::Cap, yoyLeg,
                                                          std::vector<Real>(yoyLeg.size(), strike));

    auto ovs = market->yoyCapFloorVol(name, marketConfiguration_);
    boost::shared_ptr<PricingEngine> engine;
    if (ovs->volatilityType() == ShiftedLognormal) {
        if (close_enough(ovs->displacement(), 0.0)) {
            engine = boost::make_shared<QuantExt::YoYInflationBlackCapFloorEngine>(index, ovs, discountCurve);
        } else {
            engine =
                boost::make_shared<QuantExt::YoYInflationUnitDisplacedBlackCapFloorEngine>(index, ovs, discountCurve);
        }
    } else if (ovs->volatilityType() == Normal) {
        engine = boost::make_shared<QuantExt::YoYInflationBachelierCapFloorEngine>(index, ovs, discountCurve);
    } else {
        QL_FAIL("ParSensitivityAnalysis::makeYoYCapFloor(): volatility type " << ovs->volatilityType()
                                                                              << " not handled for index " << name);
    }

    boost::shared_ptr<YoYInflationCapFloor> atmHelper = boost::make_shared<YoYInflationCapFloor>(
        YoYInflationCapFloor::Cap, yoyLeg, std::vector<Real>(yoyLeg.size(), strike));
    Rate atmRate = atmHelper->atmRate(**discountCurve);
    strike = strike == Null<Real>() ? atmRate : strike;
    YoYInflationCapFloor::Type type = strike >= atmRate ? YoYInflationCapFloor::Cap : YoYInflationCapFloor::Floor;
    auto helper = boost::make_shared<YoYInflationCapFloor>(type, yoyLeg, std::vector<Real>(yoyLeg.size(), strike));
    helper->setPricingEngine(engine);

    parYoYCaps_[key] = helper;
    parYoYCapsYts_[key] = discountCurve;
    parYoYCapsIndex_[key] = Handle<YoYInflationIndex>(index);
    parYoYCapsVts_[key] = ovs;

    return helper;
}

void ParSensitivityAnalysis::populateShiftSizes(const RiskFactorKey& key, Real parRate,
    const boost::shared_ptr<ScenarioSimMarket>& simMarket) {

    // Get zero and par shift size for the key
    Real zeroShiftSize = getShiftSize(key, sensitivityData_, simMarket);
    auto shiftData = sensitivityData_.shiftData(key.keytype, key.name);
    Real parShiftSize = shiftData.shiftSize;
    if (shiftData.shiftType == "Relative")
        parShiftSize *= parRate;
    
    // Update the map
    shiftSizes_[key] = make_pair(zeroShiftSize, parShiftSize);
    
    TLOG("Zero and par shift size for risk factor '" << key << "' is (" << std::fixed <<
        std::setprecision(12) << zeroShiftSize << "," << parShiftSize << ")");
}

set<RiskFactorKey::KeyType> ParSensitivityAnalysis::parTypes_ = {
    RiskFactorKey::KeyType::DiscountCurve,
    RiskFactorKey::KeyType::YieldCurve,
    RiskFactorKey::KeyType::IndexCurve,
    RiskFactorKey::KeyType::OptionletVolatility,
    RiskFactorKey::KeyType::SurvivalProbability,
    RiskFactorKey::KeyType::ZeroInflationCurve,
    RiskFactorKey::KeyType::YoYInflationCurve,
    RiskFactorKey::KeyType::YoYInflationCapFloorVolatility
};

ParSensitivityConverter::ParSensitivityConverter(const ParSensitivityAnalysis::ParContainer& parSensitivities,
    const map<RiskFactorKey, pair<Real, Real>>& shiftSizes) {
    
    // Populate the set of par keys (rows of Jacobi) and raw zero keys (columns of Jacobi)
    for (auto parEntry : parSensitivities) {
        parKeys_.insert(parEntry.first.first);
        rawKeys_.insert(parEntry.first.second);
    }

    Size n_par = parKeys_.size();
    Size n_raw = rawKeys_.size();
    SparseMatrix jacobi_transp(n_raw, n_par); // transposed Jacobi
    LOG("Transposed Jacobi matrix dimension " << n_raw << " x " << n_par);
    if (parKeys_ != rawKeys_) {
        std::set<RiskFactorKey> parMinusRaw, rawMinusPar;
        std::set_difference(parKeys_.begin(), parKeys_.end(), rawKeys_.begin(), rawKeys_.end(),
                            std::inserter(parMinusRaw, parMinusRaw.begin()));
        std::set_difference(rawKeys_.begin(), rawKeys_.end(), parKeys_.begin(), parKeys_.end(),
                            std::inserter(rawMinusPar, rawMinusPar.begin()));
        for (auto const& p : parMinusRaw) {
            ALOG("par key '" << p << "' not in raw key set");
        }
        for (auto const& p : rawMinusPar) {
            ALOG("raw key '" << p << "' not in par key set");
        }
        QL_FAIL("Zero and par keys should be equal for par conversion, see log for differences");
    }
    QL_REQUIRE(n_raw > 0, "Transposed Jacobi matrix has size 0");

    LOG("Populating the vector of zero and par shift sizes");
    zeroShifts_.resize(n_raw);
    parShifts_.resize(n_par);
    Size i = 0;
    for (const auto& key : rawKeys_) {
        QL_REQUIRE(shiftSizes.count(key) == 1, "ParSensitivityConverter is missing shift sizes for key " << key);
        // avoid division by zero below: if we have a zero shift, the corresponding zero sensi will be zero, too,
        // but if we divide this sensi by zero, we will get nan instead of zero
        zeroShifts_[i] = std::max(shiftSizes.at(key).first, 1E-10);
        parShifts_[i] = shiftSizes.at(key).second;
        i++;
    }

    LOG("Populating Transposed Jacobi matrix");
    for (auto const& p : parSensitivities) {
        Size parIdx = std::distance(parKeys_.begin(), parKeys_.find(p.first.first));
        Size rawIdx = std::distance(rawKeys_.begin(), rawKeys_.find(p.first.second));
        QL_REQUIRE(parIdx < n_par, "internal error: parKey " << p.first.first << " not found in parKeys_");
        QL_REQUIRE(rawIdx < n_raw, "internal error: rawKey " << p.first.second << " not found in parKeys_");
        jacobi_transp(rawIdx, parIdx) = p.second;
        TLOG("Matrix entry [" << rawIdx << ", " << parIdx << "] ~ [raw:" << p.first.second << ", par:" << p.first.first
                              << "]: " << p.second);
    }
    LOG("Finished populating transposed Jacobi matrix, non-zero entries = "
        << parSensitivities.size() << " ("
        << 100.0 * static_cast<Real>(parSensitivities.size()) / static_cast<Real>(n_par * n_raw) << "%)");

    LOG("Populating block indices");
    vector<Size> blockIndices;
    pair<RiskFactorKey::KeyType, string> previousGroup(RiskFactorKey::KeyType::None, "");
    Size blockIndex = 0;
    for (auto r : rawKeys_) {
        // Update block indices with the index where the risk factor group changes
        // e.g. when move from (DiscountCurve, EUR) to (DiscountCurve, USD)
        pair<RiskFactorKey::KeyType, string> thisGroup(r.keytype, r.name);
        if (blockIndex > 0 && previousGroup != thisGroup) {
            blockIndices.push_back(blockIndex);
            TLOG("Adding block index " << blockIndex);
        }
        ++blockIndex;

        // Update the risk factor group
        previousGroup = thisGroup;
    }
    blockIndices.push_back(blockIndex);
    TLOG("Adding block index " << blockIndex);
    LOG("Finished Populating block indices.");

    LOG("Invert Transposed Jacobi matrix");
    bool success = true;
    try {
        jacobi_transp_inv_ = blockMatrixInverse(jacobi_transp, blockIndices);
    } catch (const std::exception& e) {
        // something went wrong during the matrix inversion, so we run an extended analysis on the original matrix
        // to see whether there are zero or linearly dependent rows / columns
        ALOG(StructuredAnalyticsErrorMessage("par sensitivity conversion",
                                             "Transposed Jacobi matrix inversion failed: " + std::string(e.what())));
        LOG("Running extended matrix diagnostics (looking for zero or linearly dependent rows / columns...)");
        constexpr Size nOp = 1000; // number of operations for close_enough comparisons below
        LOG("Checking for zero rows...");
        for (auto it1 = jacobi_transp.begin1(); it1 != jacobi_transp.end1(); ++it1) {
            Real tmp = 0.0;
            for (auto it2 = it1.begin(); it2 != it1.end(); ++it2) {
                tmp += (*it2) * (*it2);
            }
            if (close_enough(tmp, 0.0, n_par * nOp)) {
                WLOG("row " << it1.index1() << " is zero");
            }
        }
        LOG("Checking for zero columns...");
        for (auto it2 = jacobi_transp.begin2(); it2 != jacobi_transp.end2(); ++it2) {
            Real tmp = 0.0;
            for (auto it1 = it2.begin(); it1 != it2.end(); ++it1) {
                tmp += (*it1) * (*it1);
            }
            if (close_enough(tmp, 0.0, n_par * nOp)) {
                WLOG("column " << it2.index2() << " is zero");
            }
        }
        LOG("Checking for linearly dependent rows...");
        for (auto it1 = jacobi_transp.begin1(); it1 != jacobi_transp.end1(); ++it1) {
            for (auto it1b = jacobi_transp.begin1(); it1b != jacobi_transp.end1(); ++it1b) {
                if (it1b.index1() <= it1.index1())
                    continue;
                Real ratio = Null<Real>();
                if (it1.begin() != it1.end() && it1b.begin() != it1b.end()) {
                    bool dependent = true;
                    auto it2b = it1b.begin();
                    for (auto it2 = it1.begin(); it2 != it1.end() && dependent; ++it2) {
                        if (close_enough(*it2, 0.0, nOp))
                            continue;
                        bool foundMatchingIndex = false;
                        while (it2b != it1b.end() && it2b.index2() <= it2.index2() && dependent) {
                            if (it2b.index2() < it2.index2()) {
                                if (!close_enough(*it2b, 0.0, nOp))
                                    dependent = false;
                            } else {
                                foundMatchingIndex = true;
                                if (close_enough(*it2b, 0.0, nOp)) {
                                    dependent = false;
                                } else if (ratio == Null<Real>()) {
                                    ratio = *it2b / *it2;
                                } else if (!close_enough(*it2b / *it2, ratio, nOp)) {
                                    dependent = false;
                                }
                            }
                            ++it2b;
                        }
                        if (!foundMatchingIndex)
                            dependent = false;
                    }
                    while (it2b != it1b.end() && dependent) {
                        if (!close_enough(*it2b, 0.0))
                            dependent = false;
                        ++it2b;
                    }
                    if (dependent) {
                        WLOG("rows " << it1.index1() << " and " << it1b.index1() << " are linearly dependent.");
                    }
                }
            }
        }
        LOG("Checking for linearly dependent columns...");
        for (auto it1 = jacobi_transp.begin2(); it1 != jacobi_transp.end2(); ++it1) {
            for (auto it1b = jacobi_transp.begin2(); it1b != jacobi_transp.end2(); ++it1b) {
                if (it1b.index2() <= it1.index2())
                    continue;
                Real ratio = Null<Real>();
                if (it1.begin() != it1.end() && it1b.begin() != it1b.end()) {
                    bool dependent = true;
                    auto it2b = it1b.begin();
                    for (auto it2 = it1.begin(); it2 != it1.end() && dependent; ++it2) {
                        if (close_enough(*it2, 0.0, nOp))
                            continue;
                        bool foundMatchingIndex = false;
                        while (it2b != it1b.end() && it2b.index1() <= it2.index1() && dependent) {
                            if (it2b.index1() < it2.index1()) {
                                if (!close_enough(*it2b, 0.0, nOp))
                                    dependent = false;
                            } else {
                                foundMatchingIndex = true;
                                if (close_enough(*it2b, 0.0, nOp)) {
                                    dependent = false;
                                } else if (ratio == Null<Real>()) {
                                    ratio = *it2b / *it2;
                                } else if (!close_enough(*it2b / *it2, ratio, nOp)) {
                                    dependent = false;
                                }
                            }
                            ++it2b;
                        }
                        if (!foundMatchingIndex)
                            dependent = false;
                    }
                    while (it2b != it1b.end() && dependent) {
                        if (!close_enough(*it2b, 0.0))
                            dependent = false;
                        ++it2b;
                    }
                    if (dependent) {
                        WLOG("columns " << it1.index2() << " and " << it1b.index2() << " are linearly dependent.");
                    }
                }
            }
        }
        LOG("Extended matrix diagnostics done. Exiting application.");
        success = false;
    }
    QL_REQUIRE(success, "Jacobi matrix inversion failed, see log file for more details.");
    Real conditionNumber = modifiedMaxNorm(jacobi_transp) * modifiedMaxNorm(jacobi_transp_inv_);
    LOG("Inverse Jacobi done, condition number of Jacobi matrix is " << conditionNumber);
    DLOG("Diagonal entries of Jacobi and inverse Jacobi:");
    DLOG("row/col              Jacobi             Inverse");
    for (Size j = 0; j < jacobi_transp.size1(); ++j) {
        DLOG(right << setw(7) << j << setw(20) << jacobi_transp(j, j) << setw(20) << jacobi_transp_inv_(j, j));
    }
}

boost::numeric::ublas::vector<Real>
ParSensitivityConverter::convertSensitivity(const boost::numeric::ublas::vector<Real>& zeroSensitivities) {
    
    DLOG("Start sensitivity conversion");
    
    Size dim = zeroSensitivities.size();
    QL_REQUIRE(jacobi_transp_inv_.size1() == dim,
               "Size mismatch between Transoposed Jacobi inverse matrix ["
                   << jacobi_transp_inv_.size1() << " x " << jacobi_transp_inv_.size2()
                   << "] and zero sensitivity array [" << dim << "]");

    // Vector storing approximation for \frac{\partial V}{\partial z_i} for each zero factor z_i
    boost::numeric::ublas::vector<Real> zeroDerivs(dim);
    zeroDerivs = element_div(zeroSensitivities, zeroShifts_);

    // Vector initially storing approximation for \frac{\partial V}{\partial c_i} for each par factor c_i
    boost::numeric::ublas::vector<Real> parSensitivities(dim);
    boost::numeric::ublas::axpy_prod(jacobi_transp_inv_, zeroDerivs, parSensitivities, true);
    
    // Update parSensitivities vector to hold the first order approximation of the NPV change due to the configured 
    // shift in each of the par factors c_i
    parSensitivities = element_prod(parSensitivities, parShifts_);

    DLOG("Sensitivity conversion done");
    
    return parSensitivities;
}

void ParSensitivityConverter::writeConversionMatrix(Report& report) const {
    
    // Report headers
    report.addColumn("RawFactor(z)", string());
    report.addColumn("ParFactor(c)", string());
    report.addColumn("dz/dc", double(), 12);

    // Write report contents i.e. entries where sparse matrix is non-zero
    Size parIdx = 0;
    for (const auto& parKey : parKeys_) {
        Size rawIdx = 0;
        for (const auto& rawKey : rawKeys_) {
            if (!close(jacobi_transp_inv_(parIdx, rawIdx), 0.0)) {
                report.next();
                report.add(to_string(rawKey));
                report.add(to_string(parKey));
                report.add(jacobi_transp_inv_(parIdx, rawIdx));
            }
            rawIdx++;
        }
        parIdx++;
    }

    // Close report
    report.end();
}

/* Helper class for implying the fair flat cap/floor volatility
   This class is copied from QuantLib's capfloor.cpp and generalised to cover both normal and lognormal volatilities */
class ImpliedCapFloorVolHelper {
public:
    ImpliedCapFloorVolHelper(const QuantLib::Instrument& cap,
                             const std::function<boost::shared_ptr<PricingEngine>(const Handle<Quote>)> engineGenerator,
                             const Real targetValue);
    Real operator()(Volatility x) const;
    Real derivative(Volatility x) const;

private:
    Real targetValue_;
    boost::shared_ptr<PricingEngine> engine_;
    boost::shared_ptr<SimpleQuote> vol_;
    const Instrument::results* results_;
};

ImpliedCapFloorVolHelper::ImpliedCapFloorVolHelper(
    const QuantLib::Instrument& cap, const std::function<boost::shared_ptr<PricingEngine>(const Handle<Quote>)> engineGenerator,
    const Real targetValue)
    : targetValue_(targetValue) {
    // set an implausible value, so that calculation is forced
    // at first ImpliedCapFloorVolHelper::operator()(Volatility x) call
    vol_ = boost::shared_ptr<SimpleQuote>(new SimpleQuote(-1));
    engine_ = engineGenerator(Handle<Quote>(vol_));
    cap.setupArguments(engine_->getArguments());
    results_ = dynamic_cast<const Instrument::results*>(engine_->getResults());
}

Real ImpliedCapFloorVolHelper::operator()(Volatility x) const {
    if (x != vol_->value()) {
        vol_->setValue(x);
        engine_->calculate();
    }
    return results_->value - targetValue_;
}

Real ImpliedCapFloorVolHelper::derivative(Volatility x) const {
    if (x != vol_->value()) {
        vol_->setValue(x);
        engine_->calculate();
    }
    std::map<std::string, boost::any>::const_iterator vega_ = results_->additionalResults.find("vega");
    QL_REQUIRE(vega_ != results_->additionalResults.end(), "vega not provided");
    return boost::any_cast<Real>(vega_->second);
}

Volatility impliedVolatility(const CapFloor& cap, Real targetValue, const Handle<YieldTermStructure>& d,
                             Volatility guess, VolatilityType type, Real displacement, Real accuracy,
                             Natural maxEvaluations, Volatility minVolLognormal, Volatility maxVolLognormal,
                             Volatility minVolNormal, Volatility maxVolNormal, const Handle<Index>& notUsed) {
    QL_REQUIRE(!cap.isExpired(), "instrument expired");
    std::function<boost::shared_ptr<PricingEngine>(const Handle<Quote>)> engineGenerator;
        if (type == ShiftedLognormal)
            engineGenerator = [&d, displacement](const Handle<Quote>& h) {
                return boost::make_shared<BlackCapFloorEngine>(d, h, Actual365Fixed(), displacement);
            };
        else if (type == Normal)
            engineGenerator = [&d](const Handle<Quote>& h) {
                return boost::make_shared<BachelierCapFloorEngine>(d, h, Actual365Fixed());
            };
        else
            QL_FAIL("volatility type " << type << " not implemented");
    ImpliedCapFloorVolHelper f(cap, engineGenerator, targetValue);
    NewtonSafe solver;
    solver.setMaxEvaluations(maxEvaluations);
    Real minVol = type == Normal ? minVolNormal : minVolLognormal;
    Real maxVol = type == Normal ? maxVolNormal : maxVolLognormal;
    return solver.solve(f, accuracy, guess, minVol, maxVol);
}

Volatility impliedVolatility(const QuantLib::YoYInflationCapFloor& cap, Real targetValue,
                             const Handle<YieldTermStructure>& d, Volatility guess, VolatilityType type,
                             Real displacement, Real accuracy, Natural maxEvaluations, Volatility minVolLognormal,
                             Volatility maxVolLognormal, Volatility minVolNormal, Volatility maxVolNormal,
                             const Handle<YoYInflationIndex>& index) {
    QL_REQUIRE(!cap.isExpired(), "instrument expired");
    std::function<boost::shared_ptr<PricingEngine>(const Handle<Quote>)> engineGenerator;
    if (type == ShiftedLognormal) {
        if (close_enough(displacement, 0.0))
            engineGenerator = [&d, &index](const Handle<Quote>& h) {
                // hardcode A365F as for ir caps, or should we use the dc from the original market vol ts ?
                // calendar, bdc not needed here, settlement days should be zero so that the
                // reference date is = evaluation date
                auto c = Handle<QuantLib::YoYOptionletVolatilitySurface>(
                    boost::make_shared<QuantExt::ConstantYoYOptionletVolatility>(
                        h, 0, NullCalendar(), Unadjusted, Actual365Fixed(),
                        index->yoyInflationTermStructure()->observationLag(), index->frequency(),
                        index->interpolated()));
                return boost::make_shared<QuantExt::YoYInflationBlackCapFloorEngine>(*index, c, d);
            };
        else
            engineGenerator = [&d, &index](const Handle<Quote>& h) {
                auto c = Handle<QuantLib::YoYOptionletVolatilitySurface>(
                    boost::make_shared<QuantExt::ConstantYoYOptionletVolatility>(
                        h, 0, NullCalendar(), Unadjusted, Actual365Fixed(),
                        index->yoyInflationTermStructure()->observationLag(), index->frequency(),
                        index->interpolated()));
                return boost::make_shared<QuantExt::YoYInflationUnitDisplacedBlackCapFloorEngine>(*index, c, d);
            };
    } else if (type == Normal)
        engineGenerator = [&d, &index](const Handle<Quote>& h) {
            auto c = Handle<QuantLib::YoYOptionletVolatilitySurface>(
                boost::make_shared<QuantExt::ConstantYoYOptionletVolatility>(
                    h, 0, NullCalendar(), Unadjusted, Actual365Fixed(),
                    index->yoyInflationTermStructure()->observationLag(), index->frequency(), index->interpolated()));
            return boost::make_shared<QuantExt::YoYInflationBachelierCapFloorEngine>(*index, c, d);
        };
    else
        QL_FAIL("volatility type " << type << " not implemented");
    ImpliedCapFloorVolHelper f(cap, engineGenerator, targetValue);
    NewtonSafe solver;
    solver.setMaxEvaluations(maxEvaluations);
    Real minVol = type == Normal ? minVolNormal : minVolLognormal;
    Real maxVol = type == Normal ? maxVolNormal : maxVolLognormal;
    return solver.solve(f, accuracy, guess, minVol, maxVol);
}

// wrapper function, does not throw
template <typename CapFloorType, typename IndexType>
Volatility impliedVolatility(const CapFloorType& cap, Real targetValue, const Handle<YieldTermStructure>& d,
                             Volatility guess, VolatilityType type, Real displacement, const Handle<IndexType>& index) {

    string strikeStr = "?";

    try {

        Real accuracy = 1.0e-6;
        Natural maxEvaluations = 100;
        Volatility minVolLognormal = 1.0e-7;
        Volatility maxVolLognormal = 4.0;
        Volatility minVolNormal = 1.0e-7;
        Volatility maxVolNormal = 0.05;

        // 1. Get strike for logging
        std::ostringstream oss;
        if (!cap.capRates().empty()) {
            oss << "Cap: " 
                << cap.capRates().size() << " strikes, starting with "
                << cap.capRates().front() << "."; // they are probably all the same here
        }
        if (!cap.floorRates().empty()) {
            oss << "Floor: "
                << cap.floorRates().size() << " strikes, starting with "
                << cap.floorRates().front() << "."; // they are probably all the same here
        }
        strikeStr = oss.str();

        // 2. Try to get implied Vol with defaults
        TLOG("Getting impliedVolatility for cap (" << cap.maturityDate() << " strike " << strikeStr << ")");
        try {
            Volatility vol = impliedVolatility(cap, targetValue, d, guess, type, displacement, accuracy, maxEvaluations,
                                               minVolLognormal, maxVolLognormal, minVolNormal, maxVolNormal, index);
            TLOG("Got vol " << vol << " on first attempt");
            return vol;
        } catch (std::exception& e) {
            ALOG("Exception getting implied Vol for Cap (" << cap.maturityDate() <<
                 " strike " << strikeStr << ") " << e.what());
        }

        // 3. Try with bigger bounds
        try {
            Volatility vol = impliedVolatility(cap, targetValue, d, guess, type, displacement, accuracy, maxEvaluations,
                                               minVolLognormal / 100.0, maxVolLognormal * 100.0,
                                               minVolNormal / 100.0, maxVolNormal * 100.0, index);
            TLOG("Got vol " << vol << " on second attempt");
            return vol;
        } catch (std::exception& e) {
            ALOG("Exception getting implied Vol for Cap (" << cap.maturityDate() <<
                 " strike " << strikeStr << ") " << e.what());
        }

    } catch(...) {
        // pass through to below
    }

    ALOG("Cap impliedVolatility() failed for Cap (" << cap.type() <<
         ", maturity " << cap.maturityDate() <<
         ", strike " << strikeStr <<
         " for target " << targetValue <<
         ". Returning Initial guess " << guess << " and continuing");
    return guess;
}

void writeParConversionMatrix(const ParSensitivityAnalysis::ParContainer& parSensitivities,
    Report& report) {

    // Report headers
    report.addColumn("ParFactor", string());
    report.addColumn("RawFactor", string());
    report.addColumn("ParSensitivity", double(), 12);

    // Report body
    for (const auto& parSensitivity : parSensitivities) {
        RiskFactorKey parKey = parSensitivity.first.first;
        RiskFactorKey rawKey = parSensitivity.first.second;
        Real sensi = parSensitivity.second;

        report.next();
        report.add(to_string(parKey));
        report.add(to_string(rawKey));
        report.add(sensi);
    }

    report.end();
}

} // namespace analytics
} // namespace ore
