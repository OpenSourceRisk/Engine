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
#include <orea/engine/parsensitivityinstrumentbuilder.hpp>
#include <orea/engine/parsensitivityutilities.hpp>
#include <orea/cube/inmemorycube.hpp>
#include <orea/engine/observationmode.hpp>
#include <orea/engine/parsensitivityanalysis.hpp>
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

//! Constructor
ParSensitivityAnalysis::ParSensitivityAnalysis(const Date& asof,
                                               const QuantLib::ext::shared_ptr<ScenarioSimMarketParameters>& simMarketParams,
                                               const SensitivityScenarioData& sensitivityData,
                                               const string& marketConfiguration, const bool continueOnError,
                                               const set<RiskFactorKey::KeyType>& typesDisabled)
    : asof_(asof), simMarketParams_(simMarketParams), sensitivityData_(sensitivityData),
      marketConfiguration_(marketConfiguration), continueOnError_(continueOnError), typesDisabled_(typesDisabled) {
    const QuantLib::ext::shared_ptr<Conventions>& conventions = InstrumentConventions::instance().conventions();
    QL_REQUIRE(conventions != nullptr, "conventions are empty");
    ParSensitivityInstrumentBuilder().createParInstruments(instruments_, asof_, simMarketParams_, sensitivityData_,
                                                           typesDisabled_, parTypes_, relevantRiskFactors_,
                                                           continueOnError_, marketConfiguration_, nullptr);
}

void ParSensitivityAnalysis::augmentRelevantRiskFactors() {
    LOG("Augment relevant risk factors, starting with " << relevantRiskFactors_.size() << " risk factors.");
    std::set<ore::analytics::RiskFactorKey> addFactors1 = relevantRiskFactors_, addFactors2, tmp;
    do {
        // DLOG("new pass with " << addFactors1.size() << " risk factors.");
        for (auto const& r : addFactors1) {
            // DLOG("candidate risk factor " << r);
            for (auto const& s : instruments_.parHelpers_) {
                if (riskFactorKeysAreSimilar(s.first, r)) {
                    tmp.insert(s.first);
                    // DLOG("factor " << r << ": found similar risk factor " << s.first);
                    for (auto const& d : instruments_.parHelperDependencies_[s.first]) {
                        if (std::find(relevantRiskFactors_.begin(), relevantRiskFactors_.end(), d) ==
                            relevantRiskFactors_.end())
                            addFactors2.insert(d);
                    }
                }
            }
            for (auto const& s : instruments_.parCaps_) {
                if (riskFactorKeysAreSimilar(s.first, r)) {
                    tmp.insert(s.first);
                    // DLOG("factor " << r << ": found similar risk factor " << s.first);
                    for (auto const& d : instruments_.parHelperDependencies_[s.first]) {
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

void ParSensitivityAnalysis::computeParInstrumentSensitivities(const QuantLib::ext::shared_ptr<ScenarioSimMarket>& simMarket) {

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
    TodaysFixingsRemover fixingRemover(instruments_.removeTodaysFixingIndices_);

    // We must have a ShiftScenarioGenerator
    QuantLib::ext::shared_ptr<ScenarioGenerator> simMarketScenGen = simMarket->scenarioGenerator();
    QuantLib::ext::shared_ptr<ShiftScenarioGenerator> scenarioGenerator =
        QuantLib::ext::dynamic_pointer_cast<ShiftScenarioGenerator>(simMarketScenGen);

    struct SimMarketResetter {
        SimMarketResetter(QuantLib::ext::shared_ptr<SimMarket> simMarket) : simMarket_(simMarket) {}
        ~SimMarketResetter() { simMarket_->reset(); }
        QuantLib::ext::shared_ptr<SimMarket> simMarket_;
    } simMarketResetter(simMarket);

    simMarket->reset();
    scenarioGenerator->reset();
    simMarket->update(asof_);

    if (!relevantRiskFactors_.empty())
        augmentRelevantRiskFactors();
    ParSensitivityInstrumentBuilder().createParInstruments(instruments_, asof_, simMarketParams_, sensitivityData_,
                                                           typesDisabled_, parTypes_, relevantRiskFactors_,
                                                           continueOnError_, marketConfiguration_, simMarket);

    map<RiskFactorKey, Real> parRatesBase, parCapVols; // for both ir and yoy caps

    for (auto& p : instruments_.parHelpers_) {
        try {
            Real parRate = impliedQuote(p.second);
            parRatesBase[p.first] = parRate;

            // Populate zero and par shift size for the current risk factor
            populateShiftSizes(p.first, parRate, simMarket);

        } catch (const std::exception& e) {
            QL_FAIL("could not imply quote for par helper " << p.first << ": " << e.what());
        }
    }

    for (auto& c : instruments_.parCaps_) {

        QL_REQUIRE(instruments_.parCapsYts_.count(c.first) > 0,
                   "computeParInstrumentSensitivities(): no cap yts found for key " << c.first);
        QL_REQUIRE(instruments_.parCapsVts_.count(c.first) > 0,
                   "computeParInstrumentSensitivities(): no cap vts found for key " << c.first);

        Real price = c.second->NPV();
        Volatility parVol = impliedVolatility(
            *c.second, price, instruments_.parCapsYts_.at(c.first), 0.01,
            instruments_.parCapsVts_.at(c.first)->volatilityType(), instruments_.parCapsVts_.at(c.first)->displacement());
        parCapVols[c.first] = parVol;
        TLOG("Fair implied cap volatility for key " << c.first << " is " << std::fixed << std::setprecision(12)
                                                    << parVol << ".");

        // Populate zero and par shift size for the current risk factor
        populateShiftSizes(c.first, parVol, simMarket);
    }

    for (auto& c : instruments_.parYoYCaps_) {

        QL_REQUIRE(instruments_.parYoYCapsYts_.count(c.first) > 0,
                   "computeParInstrumentSensitivities(): no cap yts found for key " << c.first);
        QL_REQUIRE(instruments_.parYoYCapsIndex_.count(c.first) > 0,
                   "computeParInstrumentSensitivities(): no cap index found for key " << c.first);
        QL_REQUIRE(instruments_.parYoYCapsVts_.count(c.first) > 0,
                   "computeParInstrumentSensitivities(): no cap vts found for key " << c.first);

        Real price = c.second->NPV();
        Volatility parVol = impliedVolatility(
            *c.second, price, instruments_.parYoYCapsYts_.at(c.first), 0.01,
            instruments_.parYoYCapsVts_.at(c.first)->volatilityType(),
            instruments_.parYoYCapsVts_.at(c.first)->displacement(), instruments_.parYoYCapsIndex_.at(c.first));
        parCapVols[c.first] = parVol;
        TLOG("Fair implied yoy cap volatility for key " << c.first << " is " << std::fixed << std::setprecision(12)
                                                        << parVol << ".");

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
    QL_REQUIRE(desc.size() == scenarioGenerator->samples(),
               "descriptions size " << desc.size() << " does not match samples " << scenarioGenerator->samples());

    std::set<RiskFactorKey> parKeysCheck, parKeysNonZero;
    std::set<RiskFactorKey> rawKeysCheck, rawKeysNonZero;

    for (auto const& p : instruments_.parHelpers_) {
        parKeysCheck.insert(p.first);
    }

    for (auto const& p : instruments_.parCaps_) {
        parKeysCheck.insert(p.first);
    }

    for (auto const& p : instruments_.parYoYCaps_) {
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
            for (auto it : instruments_.parHelpers_)
                it.second->deepUpdate();
            for (auto it : instruments_.parCaps_)
                it.second->deepUpdate();
            for (auto it : instruments_.parYoYCaps_)
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

        std::set<RiskFactorKey::KeyType> survivalAndRateCurveTypes = {
            RiskFactorKey::KeyType::SurvivalProbability, RiskFactorKey::KeyType::DiscountCurve,
            RiskFactorKey::KeyType::YieldCurve, RiskFactorKey::KeyType::IndexCurve};

        for (auto const& p : instruments_.parHelpers_) {

            // skip if par helper has no sensi to zero risk factor (except the special treatment below kicks in)

            if (p.second->isCalculated() &&
                (survivalAndRateCurveTypes.find(p.first.keytype) == survivalAndRateCurveTypes.end() ||
                 p.first != desc[i].key1())) {
                continue;
            }

            // compute fair and base quotes

            Real fair = impliedQuote(p.second);
            auto base = parRatesBase.find(p.first);
            QL_REQUIRE(base != parRatesBase.end(), "internal error: did not find parRatesBase[" << p.first << "]");

            Real tmp = (fair - base->second) / shiftSize;

            // special treatments for certain risk factors

            // for curves with survival probabilities / discount factors going to zero quickly we might see a
            // sensitivity that is close to zero, which we sanitise here in order to prevent the Jacobi matrix
            // getting ill-conditioned or even singular

            if (survivalAndRateCurveTypes.find(p.first.keytype) != survivalAndRateCurveTypes.end() &&
                p.first == desc[i].key1() && std::abs(tmp) < 0.01) {
                WLOG("Setting Diagonal Sensi " << p.first << " w.r.t. " << desc[i].key1() << " to 0.01 (got " << tmp
                                               << ")");
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

        for (auto const& p : instruments_.parCaps_) {

            if (p.second->isCalculated() && p.first != desc[i].key1())
                continue;

            auto fair = impliedVolatility(p.first, instruments_);
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

        for (auto const& p : instruments_.parYoYCaps_) {

            if (p.second->isCalculated() && p.first != desc[i].key1())
                continue;

            auto fair = impliedVolatility(p.first, instruments_);
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
    std::set<RiskFactorKey> problematicKeys;
    problematicKeys.insert(parKeysZero.begin(), parKeysZero.end());
    problematicKeys.insert(rawKeysZero.begin(), rawKeysZero.end());
    for (auto const& k : problematicKeys) {
        std::string type;
        if (parKeysZero.find(k) != parKeysZero.end())
            type = "par instrument is insensitive to all zero risk factors";
        else if (rawKeysZero.find(k) != rawKeysZero.end())
            type = "zero risk factor that does not affect an par instrument";
        else
            type = "unknown";
        Real parHelperValue = Null<Real>();
        if (auto tmp = instruments_.parHelpers_.find(k); tmp != instruments_.parHelpers_.end())
            parHelperValue = impliedQuote(tmp->second);
        else if (auto tmp = instruments_.parCaps_.find(k); tmp != instruments_.parCaps_.end())
            parHelperValue = tmp->second->NPV();
        else if (auto tmp = instruments_.parYoYCaps_.find(k); tmp != instruments_.parYoYCaps_.end())
            parHelperValue = tmp->second->NPV();
        Real zeroFactorValue = Null<Real>();
        if (simMarket->baseScenarioAbsolute()->has(k))
            zeroFactorValue = simMarket->baseScenarioAbsolute()->get(k);
        WLOG("zero/par relation problem for key '"
             << k << "', type " + type + ", par value = "
             << (parHelperValue == Null<Real>() ? "na" : std::to_string(parHelperValue))
             << ", zero value = " << (zeroFactorValue == Null<Real>() ? "na" : std::to_string(zeroFactorValue)));
    }

    LOG("Computing par rate and flat vol sensitivities done");
} // compute par instrument sensis

void ParSensitivityAnalysis::alignPillars() {
    LOG("Align simulation market pillars to actual latest relevant dates of par instruments");
    // If any of the yield curve types are still active, align the pillars.
    if (typesDisabled_.count(RiskFactorKey::KeyType::DiscountCurve) == 0 ||
        typesDisabled_.count(RiskFactorKey::KeyType::YieldCurve) == 0 ||
        typesDisabled_.count(RiskFactorKey::KeyType::IndexCurve) == 0) {
        for (auto const& p : instruments_.yieldCurvePillars_) {
            simMarketParams_->setYieldCurveTenors(p.first, p.second);
            DLOG("yield curve tenors for " << p.first << " set (" << p.second.size() << ")");
            for (auto const& t : p.second)
                TLOG("set tenor " << t.length() << " " << t.units())
        }
    }
    if (typesDisabled_.count(RiskFactorKey::KeyType::OptionletVolatility) == 0) {
        for (auto const& p : instruments_.capFloorPillars_) {
            simMarketParams_->setCapFloorVolExpiries(p.first, p.second);
            DLOG("cap floor expiries for " << p.first << " set (" << p.second.size() << ")");
            for (auto const& t : p.second)
                TLOG("set tenor " << t.length() << " " << t.units())
        }
    }
    if (typesDisabled_.count(RiskFactorKey::KeyType::YoYInflationCapFloorVolatility) == 0) {
        for (auto const& p : instruments_.yoyCapFloorPillars_) {
            simMarketParams_->setYoYInflationCapFloorVolExpiries(p.first, p.second);
            DLOG("yoy cap floor expiries for " << p.first << " set (" << p.second.size() << ")");
            for (auto const& t : p.second)
                TLOG("set tenor " << t.length() << " " << t.units())
        }
    }
    if (typesDisabled_.count(RiskFactorKey::KeyType::SurvivalProbability) == 0) {
        for (auto const& p : instruments_.cdsPillars_) {
            simMarketParams_->setDefaultTenors(p.first, p.second);
            DLOG("default expiries for " << p.first << " set (" << p.second.size() << ")");
            for (auto const& t : p.second)
                TLOG("set tenor " << t.length() << " " << t.units())
        }
    }
    if (typesDisabled_.count(RiskFactorKey::KeyType::ZeroInflationCurve) == 0) {
        for (auto const& p : instruments_.zeroInflationPillars_) {
            simMarketParams_->setZeroInflationTenors(p.first, p.second);
            DLOG("zero inflation expiries for " << p.first << " set (" << p.second.size() << ")");
            for (auto const& t : p.second)
                TLOG("set tenor " << t.length() << " " << t.units())
        }
    }
    if (typesDisabled_.count(RiskFactorKey::KeyType::YoYInflationCurve) == 0) {
        for (auto const& p : instruments_.yoyInflationPillars_) {
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

void ParSensitivityAnalysis::populateShiftSizes(const RiskFactorKey& key, Real parRate,
                                                const QuantLib::ext::shared_ptr<ScenarioSimMarket>& simMarket) {

    // Get zero and par shift size for the key
    Real zeroShiftSize = getShiftSize(key, sensitivityData_, simMarket);
    auto shiftData = sensitivityData_.shiftData(key.keytype, key.name);
    Real parShiftSize = shiftData.shiftSize;
    if (shiftData.shiftType == ShiftType::Relative)
        parShiftSize *= parRate;

    // Update the map
    shiftSizes_[key] = make_pair(zeroShiftSize, parShiftSize);

    TLOG("Zero and par shift size for risk factor '" << key << "' is (" << std::fixed << std::setprecision(12)
                                                     << zeroShiftSize << "," << parShiftSize << ")");
}

set<RiskFactorKey::KeyType> ParSensitivityAnalysis::parTypes_ = {
    RiskFactorKey::KeyType::DiscountCurve,       RiskFactorKey::KeyType::YieldCurve,
    RiskFactorKey::KeyType::IndexCurve,          RiskFactorKey::KeyType::OptionletVolatility,
    RiskFactorKey::KeyType::SurvivalProbability, RiskFactorKey::KeyType::ZeroInflationCurve,
    RiskFactorKey::KeyType::YoYInflationCurve,   RiskFactorKey::KeyType::YoYInflationCapFloorVolatility};

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
        StructuredAnalyticsErrorMessage("Par sensitivity conversion", "Transposed Jacobi matrix inversion failed",
                                        e.what())
            .log();
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
    QL_REQUIRE(jacobi_transp_inv_.size1() == dim, "Size mismatch between Transoposed Jacobi inverse matrix ["
                                                      << jacobi_transp_inv_.size1() << " x "
                                                      << jacobi_transp_inv_.size2() << "] and zero sensitivity array ["
                                                      << dim << "]");

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

void writeParConversionMatrix(const ParSensitivityAnalysis::ParContainer& parSensitivities, Report& report) {

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
