/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

#include <orea/simm/crifmarket.hpp>

#include <ored/marketdata/structuredcurveerror.hpp>

#include <qle/cashflows/overnightindexedcoupon.hpp>
#include <qle/instruments/makeoiscapfloor.hpp>
#include <qle/math/flatextrapolation.hpp>
#include <qle/termstructures/strippedoptionletadapter.hpp>

#include <ql/instruments/makecapfloor.hpp>
#include <ql/termstructures/volatility/optionlet/strippedoptionlet.hpp>
#include <ql/termstructures/volatility/swaption/swaptionvolmatrix.hpp>

#include <boost/range/adaptor/indexed.hpp>

using namespace ore::analytics;
using namespace QuantLib;
using QuantExt::LinearFlat;
using std::exception;
using std::map;
using std::pair;
using std::string;

namespace {

// Create the OptionletVolatilityStructure for the given currency.
Handle<OptionletVolatilityStructure>
createOvs(const string& key, const Date& asof, const QuantLib::ext::shared_ptr<ScenarioSimMarket>& ssm,
          const QuantLib::ext::shared_ptr<SensitivityScenarioData::CapFloorVolShiftData>& sd,
          const QuantLib::ext::shared_ptr<CurveConfigurations>& curveConfigs) {

    DLOG("CrifMarket: creating ATM optionlet curve for key " << key << ".");

    // The ssm should have an OptionletVolatilityStructure for the key
    auto ovs = ssm->capFloorVol(key);

    // Get the index string for the cap floor structure from the curve configurations.
    string iborIndexName = sd->indexName;

    // Get the Ibor Index from ssm.
    QuantLib::ext::shared_ptr<IborIndex> iborIndex = *ssm->iborIndex(iborIndexName);

    // When building the option pillars / vol pairs below we are assuming that the ssm was build with
    // capFloorVolAdjustOptionletPillars = false!

    // for overnight indices get rate computation period from curve config
    bool isOis = QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(iborIndex) != nullptr;
    Period rateComputationPeriod;
    if (isOis) {
        QuantLib::ext::shared_ptr<CapFloorVolatilityCurveConfig> config;
        if (curveConfigs->hasCapFloorVolCurveConfig(key))
            config = curveConfigs->capFloorVolCurveConfig(key);
        else if (curveConfigs->hasCapFloorVolCurveConfig(iborIndex->currency().code()))
            config = curveConfigs->capFloorVolCurveConfig(iborIndex->currency().code());
        if (config == nullptr) {
            StructuredCurveErrorMessage(
                key, "CrifMarket: Could not determine rateComputationPeriod for overnight index, fall back to 3M",
                "No curve config found for either '" + key + "' or '" + iborIndex->currency().code() + "'.")
                .log();
            rateComputationPeriod = 3 * Months;
        } else {
            rateComputationPeriod = config->rateComputationPeriod();
        }
    }

    // For each tenor in cap floor vol shift data for this currency, get the optionlet volatility at that 
    // tenor and at a strike equal to the forward ibor rate.
    vector<Date> expiries;
    vector<Rate> strikes{Null<Real>()};
    vector<vector<Handle<Quote>>> volatilities;

    for (Size i = 0; i < sd->shiftExpiries.size(); i++) {
        Period tenor = sd->shiftExpiries[i];
        Date optionDate = iborIndex->fixingCalendar().adjust(ovs->optionDateFromTenor(tenor));
        Rate forward;
        if (isOis) {
	    Date startDate = iborIndex->valueDate(optionDate);
	    Date maturityDate = iborIndex->fixingCalendar().advance(startDate, rateComputationPeriod);
            QuantExt::OvernightIndexedCoupon coupon(maturityDate, 1.0, iborIndex->valueDate(optionDate), maturityDate,
                                                    QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(iborIndex));
            forward = coupon.rate();
        } else {
            forward = iborIndex->fixing(optionDate);
        }
	// skip duplicate or non-monotonic tenors
        if (!expiries.empty() && optionDate <= expiries.back())
            continue;
        expiries.push_back(optionDate);
        Volatility vol = ovs->volatility(optionDate, forward);
        volatilities.push_back({Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(vol))});
        TLOG("Added (date,vol) = "
             << "(" << io::iso_date(expiries.back()) << "," << std::fixed << std::setprecision(9) << vol << ") for key "
             << key << ". (tenor,forward) = "
             << "(" << tenor << "," << forward << ").");
    }

    // Create the stripped optionlet.
    // settlement days might not be provided by fixed reference date ovs
    Natural settlementDays = 0;
    try {
        settlementDays = ovs->settlementDays();
    } catch (...) {
    }
    auto so = QuantLib::ext::make_shared<StrippedOptionlet>(
        settlementDays, ovs->calendar(), ovs->businessDayConvention(), iborIndex, expiries, strikes,
        volatilities, ovs->dayCounter(), ovs->volatilityType(), ovs->displacement());

    // Return the OptionletVolatilityStructure.
    return Handle<OptionletVolatilityStructure>(
        QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<LinearFlat, LinearFlat>>(asof, so));
}

// Create the SwaptionVolatilityStructure for the given currency.
Handle<SwaptionVolatilityStructure> createSvs(const string& key, const Date& asof,
                                              const QuantLib::ext::shared_ptr<ScenarioSimMarket>& ssm,
                                              const SensitivityScenarioData::GenericYieldVolShiftData& sd) {

    DLOG("CrifMarket: creating ATM swaption surface for key " << key << ".");

    // The ssm should have a SwaptionVolatilityStructure for the currency.
    auto svs = ssm->swaptionVol(key);

    // Set strike to Null<Real>() to get the ATM volatility (by convention).
    Real strike = Null<Real>();

    // For each expiry tenor and swap tenor in the swaption vol shift data for this currency, get the ATM swaption 
    // volatility at that tenor and expiry and possibly the shift.
    Matrix vols(sd.shiftExpiries.size(), sd.shiftTerms.size(), 0.0);
    Matrix shifts = vols;
    for (Size i = 0; i < sd.shiftExpiries.size(); i++) {
        for (Size j = 0; j < sd.shiftTerms.size(); j++) {
            Period tenor = sd.shiftExpiries[i];
            Period term = sd.shiftTerms[j];
            vols[i][j] = svs->volatility(tenor, term, strike);
            if (svs->volatilityType() == ShiftedLognormal) {
                shifts[i][j] = svs->shift(tenor, term);
            }
            TLOG("Added (tenor,term,vol,shift) = "
                 << "(" << tenor << "," << term << "," << std::fixed << std::setprecision(9) << vols[i][j] << ","
                 << shifts[i][j] << ") for key " << key << ".");
        }
    }

    // Create the swaption volatility matrix.
    bool flatExtrapolation = true;
    return Handle<SwaptionVolatilityStructure>(QuantLib::ext::make_shared<SwaptionVolatilityMatrix>(asof, svs->calendar(),
        svs->businessDayConvention(), sd.shiftExpiries, sd.shiftTerms, vols, svs->dayCounter(), flatExtrapolation,
        svs->volatilityType(), shifts));
}
}

namespace ore {
namespace analytics {

CrifMarket::CrifMarket(const Date& asof) : MarketImpl(true) {
    asof_ = asof;
    LOG("Constructed empty CrifMarket.");
}

CrifMarket::CrifMarket(const Date& asof, const QuantLib::ext::shared_ptr<ScenarioSimMarket>& ssm,
                       const QuantLib::ext::shared_ptr<SensitivityScenarioData>& ssd,
                       const QuantLib::ext::shared_ptr<ore::data::CurveConfigurations>& curveConfigs)
    : MarketImpl(true), ssm_(ssm), ssd_(ssd), curveConfigs_(curveConfigs) {

    LOG("Start constructing CrifMarket.");
    asof_ = asof;
    addAtmOptionletVolatilities();
    addAtmSwaptionVolatilities();
    LOG("Finished constructing CrifMarket.");
}

const QuantLib::ext::shared_ptr<ScenarioSimMarket>& CrifMarket::simMarket() const {
    return ssm_;
}

const QuantLib::ext::shared_ptr<SensitivityScenarioData>& CrifMarket::sensiData() const {
    return ssd_;
}

void CrifMarket::addAtmOptionletVolatilities() {

    DLOG("Start adding ATM optionlet volatility curves to CrifMarket");

    // Loop over cap floor volatility shift data in ssd.
    // This indicates the cap floor volatility curves that we are bumping.
    for (const auto& kv : ssd_->capFloorVolShiftData()) {

        try {

            // Create the ATM OptionletVolatilityStructure.
            auto atmOvs = createOvs(kv.first, asof_, ssm_, kv.second, curveConfigs_);

            DLOG("Adding ATM optionlet curve for currency " << kv.first << ".");
            capFloorCurves_[make_pair(Market::defaultConfiguration, kv.first)] = atmOvs;

        } catch (const exception& e) {

            ALOG("Failed to add an ATM optionlet curve for currency " << kv.first << " to CrifMarket with error: " <<
                e.what() << ". CRIF generation will fail if it needs these volatilities.");

        }
        
    }

    DLOG("Finished adding ATM optionlet volatility curves to CrifMarket");

}

void CrifMarket::addAtmSwaptionVolatilities() {

    DLOG("Start adding ATM swaption volatility surfaces to CrifMarket");

    // Loop over swaption volatility shift data in ssd.
    for (const auto& kv : ssd_->swaptionVolShiftData()) {

        try {

            // Create the ATM OptionletVolatilityStructure.
            auto atmSvs = createSvs(kv.first, asof_, ssm_, *kv.second);

            DLOG("Adding ATM swaption surface for currency " << kv.first << ".");
            swaptionCurves_[make_pair(Market::defaultConfiguration, kv.first)] = atmSvs;

        } catch (const exception& e) {

            ALOG("Failed to add an ATM swaption surface for currency " << kv.first << " to CrifMarket with error: " <<
                e.what() << ". CRIF generation will fail if it needs these volatilities.");

        }
        
    }

    DLOG("Finished adding ATM swaption volatility surfaces to CrifMarket");
}

}
}
