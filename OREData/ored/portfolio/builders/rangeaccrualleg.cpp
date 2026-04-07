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

#include <ored/portfolio/builders/rangeaccrualleg.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/indexparser.hpp>

#include <qle/cashflows/rangeaccrualcouponpricer.hpp>

#include <ql/cashflows/rangeaccrual.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/pricingengines/blackformula.hpp>
#include <ql/termstructures/volatility/smilesection.hpp>
#include <ql/termstructures/volatility/flatsmilesection.hpp>
#include <ql/termstructures/volatility/optionlet/constantoptionletvol.hpp>

namespace ore {
namespace data {

namespace {

//! Adapter that converts a Normal (Bachelier) SmileSection to Lognormal (Black).
/*! The BGM range-accrual pricer (RangeAccrualPricerByBgm) interprets the vol
    returned by SmileSection::volatility(strike) as a lognormal instantaneous vol.
    When the cap/floor surface is quoted in Normal vols, we must convert on the fly.
    For each strike we:
    1. Price an option using the Normal vol and Bachelier formula
    2. Invert the Black formula to recover the equivalent lognormal vol
*/
class NormalToLognormalSmileSection : public QuantLib::SmileSection {
public:
    NormalToLognormalSmileSection(const QuantLib::ext::shared_ptr<QuantLib::SmileSection>& normalSection,
                                 QuantLib::Real forward)
        : SmileSection(normalSection->exerciseTime(), normalSection->dayCounter(),
                       QuantLib::ShiftedLognormal,
                       // Displacement ensures forward + shift > 0 for lognormal inversion.
                       // When forward is positive, shift = 0 (plain lognormal).
                       std::max(0.0, -forward + 1e-4)),
          normalSection_(normalSection), forward_(forward),
          shift_(std::max(0.0, -forward + 1e-4)) {}

    QuantLib::Real minStrike() const override { return normalSection_->minStrike(); }
    QuantLib::Real maxStrike() const override { return normalSection_->maxStrike(); }
    QuantLib::Real atmLevel() const override { return forward_; }

protected:
    QuantLib::Volatility volatilityImpl(QuantLib::Rate strike) const override {
        Real normalVol = normalSection_->volatility(strike);
        Real t = exerciseTime();
        if (t <= 0.0 || normalVol <= 0.0)
            return 0.0;
        Real normalStdDev = normalVol * std::sqrt(t);
        Option::Type type = strike >= forward_ ? Option::Call : Option::Put;
        Real premium = bachelierBlackFormula(type, strike, forward_, normalStdDev);
        try {
            // shift_ is 0 when forward > 0 (standard lognormal inversion).
            // When forward ≤ 0, shift_ makes (forward + shift_) > 0 so the
            // shifted-Black inversion is well-defined. The BGM pricer treats
            // the returned vol as lognormal; the approximation error is small
            // for modest shifts.
            return blackFormulaImpliedStdDev(type, strike, forward_, premium, 1.0, shift_) /
                   std::sqrt(t);
        } catch (...) {
            // Fallback: first-order approximation  σ_SLN ≈ σ_N / (F + shift)
            return normalVol / (forward_ + shift_);
        }
    }

private:
    QuantLib::ext::shared_ptr<QuantLib::SmileSection> normalSection_;
    QuantLib::Real forward_;
    QuantLib::Real shift_;
};

}

QuantLib::ext::shared_ptr<FloatingRateCouponPricer> RangeAccrualLegEngineBuilder::engineImpl(
    const std::string& index, const Date& accrualStartDate, const Date& accrualEndDate) {
    auto config = configuration(MarketContext::pricing);
    Real corr = parseReal(engineParameter("Correlation", {}, false, "1.0"));
    bool isFlatVol = parseBool(engineParameter("withFlatVol", {}, false, "false"));
    bool callSpread = parseBool(engineParameter("ByCallSpread", {}, false, "true"));
    Handle<SwaptionVolatilityStructure> ovs = market_->swaptionVol(index, config);
    Date expiryDate = std::max(accrualStartDate, ovs->referenceDate() + 1);
    ext::shared_ptr<SmileSection>smileOnExpiry;
    ext::shared_ptr<SmileSection>smileOnPayment;
    if(!isFlatVol){
        // Use at least 1 day after reference date to avoid t=0 smile section
        // (stddev = vol * sqrt(0) = 0, then vol = stddev / sqrt(0) = NaN)
        smileOnExpiry = ovs->smileSection(expiryDate, true);
        smileOnPayment = ovs->smileSection(accrualEndDate, true);
    }else{
        // Use the day counter from the index in LegData
        auto iborIndex = market_->iborIndex(index, config);
        auto dayCounter = iborIndex->dayCounter();
        Real fVol = parseReal(engineParameter("FlatVol", {}, false, "1E-6"));
        smileOnExpiry = QuantLib::ext::shared_ptr<SmileSection>(new FlatSmileSection(expiryDate, fVol, dayCounter));
        smileOnPayment = QuantLib::ext::shared_ptr<SmileSection>(new FlatSmileSection(accrualEndDate, fVol, dayCounter));
    }

    

    // The BGM pricer interprets SmileSection vols as lognormal. If the swaption
    // surface is in Normal vol, we must convert via Bachelier→Black inversion.
    if (!isFlatVol && ovs->volatilityType() == QuantLib::Normal) {
        auto iborIndex = market_->iborIndex(index, config);
        // Use per-section forwards: the conversion should reflect the forward
        // at each smile section's expiry. Using the index's fixing calendar
        // ensures the dates are valid fixing dates.
        Calendar fixCal = iborIndex->fixingCalendar();
        Real forwardExpiry = iborIndex->forecastFixing(fixCal.adjust(expiryDate));
        Real forwardPayment = iborIndex->forecastFixing(fixCal.adjust(accrualEndDate));
        smileOnExpiry = QuantLib::ext::make_shared<NormalToLognormalSmileSection>(smileOnExpiry, forwardExpiry);
        smileOnPayment = QuantLib::ext::make_shared<NormalToLognormalSmileSection>(smileOnPayment, forwardPayment);
    }

    return QuantLib::ext::make_shared<RangeAccrualPricerByBgm>(
        corr, smileOnExpiry, smileOnPayment, !isFlatVol, callSpread);
}

QuantLib::ext::shared_ptr<FloatingRateCouponPricer> RangeAccrualLegCallSpreadEngineBuilder::engineImpl(
    const std::string& index) {
    auto config = configuration(MarketContext::pricing);

    Handle<OptionletVolatilityStructure> ovs = market_->capFloorVol(index, config);

    auto isFlatVol = parseBool(engineParameter("withFlatVol", {}, false, "false"));
    if (isFlatVol) {
        ovs = Handle<OptionletVolatilityStructure>(
            QuantLib::ext::make_shared<ConstantOptionletVolatility>(
                ovs->referenceDate(), ovs->calendar(), ovs->businessDayConvention(),
                0.0, ovs->dayCounter(), ovs->volatilityType(), ovs->displacement()));
    }
    QL_REQUIRE(!ovs.empty(), "RangeAccrualLegCallSpreadEngineBuilder: no capFloor vol for " << index);

    Real eps = 1.0e-4;
    if (engineParameter("CallSpreadEps", {}, false, "") != "")
        eps = parseReal(engineParameter("CallSpreadEps"));

    return QuantLib::ext::make_shared<QuantExt::RangeAccrualPricerByCallSpread>(ovs, eps);
}

} // namespace data
} // namespace ore
