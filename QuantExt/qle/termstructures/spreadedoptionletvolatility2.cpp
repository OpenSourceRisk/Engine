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

#include <qle/indexes/bmaindexwrapper.hpp>
#include <qle/termstructures/proxyoptionletvolatility.hpp>
#include <qle/termstructures/spreadedoptionletvolatility2.hpp>
#include <qle/termstructures/spreadedsmilesection2.hpp>
#include <qle/utilities/cashflows.hpp>
#include <qle/utilities/time.hpp>

#include <ql/math/interpolations/bilinearinterpolation.hpp>
#include <ql/math/interpolations/flatextrapolation2d.hpp>

#include <ql/time/calendars/nullcalendar.hpp>

namespace QuantExt {

using namespace QuantLib;
using std::vector;

SpreadedOptionletVolatility2::SpreadedOptionletVolatility2(
    const Handle<OptionletVolatilityStructure>& baseVol,
    const vector<Date>& optionDates,
    const vector<Real>& strikes,
    const vector<vector<Handle<Quote>>>& volSpreads,
    const ReactionToTimeDecay decayMode,
    Stickyness stickyness,
    ext::shared_ptr<IborIndex> index,
    ext::shared_ptr<IborIndex> initIndex,
    Period rateComputationPeriod)
    : OptionletVolatilityStructure(0, !baseVol->calendar().empty() ? baseVol->calendar() : NullCalendar(),
        baseVol->businessDayConvention(), baseVol->dayCounter()),
      baseVol_(baseVol), optionDates_(optionDates), strikes_(strikes), volSpreads_(volSpreads), decayMode_(decayMode),
      stickyness_(stickyness), index_(std::move(index)), initIndex_(std::move(initIndex)),
      rateComputationPeriod_(std::move(rateComputationPeriod)), t0_(0.0) {

    registerWith(baseVol_);

    QL_REQUIRE(!optionDates_.empty(), "SpreadedOptionletVolatility2: optionDates are empty");
    QL_REQUIRE(!strikes_.empty(), "SpreadedOptionletVolatility2: strikes are empty");
    QL_REQUIRE(stickyness_ == StickyStrike || stickyness_ == StickyMoneyness,
        "SpreadedOptionletVolatility2: stickyness should be either StickyStrike or StickyMoneyness");
    if (stickyness_ == StickyMoneyness) {
        QL_REQUIRE(index_, "SpreadedOptionletVolatility2: index cannot be null for StickyMoneyness");
        QL_REQUIRE(initIndex_, "SpreadedOptionletVolatility2: initial market index cannot be null for StickyMoneyness");
    }

    // add an artificial option date if we only have one to ensure the interpolation is working
    if (optionDates_.size() == 1) {
        optionDates_.push_back(optionDates_.back() + 1);
        volSpreads_.push_back(volSpreads_.back());
    }

    // add an artificial strike if we only have one to ensure the interpolation is working
    if (strikes_.size() == 1) {
        strikes_.push_back(strikes_.back() + 0.01);
        for (auto& v : volSpreads_)
            v.push_back(v.back());
    }

    optionTimes_.resize(optionDates_.size());
    volSpreadValues_ = Matrix(strikes_.size(), optionDates_.size());
    for (auto const& v : volSpreads_)
        for (auto const& q : v)
            registerWith(q);
}

Date SpreadedOptionletVolatility2::maxDate() const { return baseVol_->maxDate(); }
BusinessDayConvention SpreadedOptionletVolatility2::businessDayConvention() const {
    return baseVol_->businessDayConvention();
}
Rate SpreadedOptionletVolatility2::minStrike() const { return baseVol_->minStrike(); }
Rate SpreadedOptionletVolatility2::maxStrike() const { return baseVol_->maxStrike(); }
VolatilityType SpreadedOptionletVolatility2::volatilityType() const { return baseVol_->volatilityType(); }
Real SpreadedOptionletVolatility2::displacement() const { return baseVol_->displacement(); }
bool SpreadedOptionletVolatility2::useEffectiveVolatility() const { return baseVol_->useEffectiveVolatility(); }

ext::shared_ptr<SmileSection> SpreadedOptionletVolatility2::smileSectionImpl(Time optionTime) const {
    calculate();

    vector<Real> volSpreads(strikes_.size());
    for (Size k = 0; k < strikes_.size(); ++k)
        volSpreads[k] = volSpreadInterpolation_(optionTime, strikes_[k]);

    // Populate variables to use in creation of SpreadedSmileSection2 below when we have sticky moneyness.
    bool stickyMoneyness = stickyness_ == StickyMoneyness;
    Real initAtm = Null<Real>();
    Real atm = Null<Real>();
    Real anchorInitAtm = Null<Real>();
    Real anchorAtm = Null<Real>();
    if (stickyMoneyness) {
        Date fixingDate = dateFromTime(*this, optionTime + t0_);
        initAtm = getIndexRate(fixingDate, initIndex_, rateComputationPeriod_);
        atm = getIndexRate(fixingDate, index_, rateComputationPeriod_);
        if (originalRefDate_ != actualRefDate_ && decayMode_ != ConstantVariance) {
            anchorInitAtm = getIndexRate(actualRefDate_, initIndex_, rateComputationPeriod_);
            anchorAtm = getIndexRate(actualRefDate_, index_, rateComputationPeriod_);
        }
    }

    bool strikesRelativeToAtm = false;
    if (originalRefDate_ == actualRefDate_ || decayMode_ == ReactionToTimeDecay::ConstantVariance) {
        return ext::make_shared<SpreadedSmileSection2>(baseVol_->smileSection(optionTime), volSpreads, strikes_,
            strikesRelativeToAtm, initAtm, atm, stickyMoneyness);
    } else {
        return ext::make_shared<SpreadedSmileSection2>(baseVol_->smileSection(optionTime + t0_),
            baseVol_->smileSection(t0_), volSpreads, strikes_, strikesRelativeToAtm, initAtm, anchorInitAtm,
            atm, anchorAtm, stickyMoneyness);
    }
}

Volatility SpreadedOptionletVolatility2::volatilityImpl(Time optionTime, Rate strike) const {
    return smileSectionImpl(optionTime)->volatility(strike);
}

void SpreadedOptionletVolatility2::performCalculations() const {
    originalRefDate_ = baseVol_->referenceDate();
    actualRefDate_ = referenceDate();
    t0_ = baseVol_->timeFromReference(actualRefDate_);
    for (Size i = 0; i < optionDates_.size(); ++i)
        optionTimes_[i] = timeFromReference(optionDates_[i]);
    for (Size k = 0; k < strikes_.size(); ++k) {
        for (Size i = 0; i < optionDates_.size(); ++i) {
            QL_REQUIRE(!volSpreads_[i][k].empty(), "SpreadedOptionletVolatility2::performCalculations(): "
                "volSpread at " << i << ", " << k << " is empty");
            volSpreadValues_(k, i) = volSpreads_[i][k]->value();
        }
    }
    volSpreadInterpolation_ = FlatExtrapolator2D(QuantLib::ext::make_shared<BilinearInterpolation>(
        optionTimes_.begin(), optionTimes_.end(), strikes_.begin(), strikes_.end(), volSpreadValues_));
    volSpreadInterpolation_.enableExtrapolation();
}

void SpreadedOptionletVolatility2::update() {
    OptionletVolatilityStructure::update();
    LazyObject::update();
}

void SpreadedOptionletVolatility2::deepUpdate() {
    baseVol_->update();
    update();
}

AtmAdjustedSpreadedOptionletVolatility2::AtmAdjustedSpreadedOptionletVolatility2(
    const Handle<OptionletVolatilityStructure>& baseVol, const vector<Date>& optionDates,
    const vector<Real>& strikes, const vector<vector<Handle<Quote>>>& volSpreads,
    const QuantLib::ext::shared_ptr<QuantLib::IborIndex>& baseIndex,
    const QuantLib::ext::shared_ptr<QuantLib::IborIndex>& targetIndex,
    const QuantLib::Period& baseRateComputationPeriod, const QuantLib::Period& targetRateComputationPeriod,
    Real scalingFactor, ReactionToTimeDecay decayMode)
    : SpreadedOptionletVolatility2(baseVol, optionDates, strikes, volSpreads, decayMode), baseIndex_(baseIndex),
      targetIndex_(targetIndex), baseRateComputationPeriod_(baseRateComputationPeriod),
      targetRateComputationPeriod_(targetRateComputationPeriod), scalingFactor_(scalingFactor) {
    registerWith(baseVol);
    registerWith(baseIndex_);
    registerWith(targetIndex_);
}

QuantLib::ext::shared_ptr<SmileSection> AtmAdjustedSpreadedOptionletVolatility2::smileSectionImpl(
    const QuantLib::Date& fixingDate) const {
    calculate();

    Real baseAtmLevel = ProxyOptionletVolatility::getAtmLevel(fixingDate, baseIndex_, baseRateComputationPeriod_);
    Real targetAtmLevel = ProxyOptionletVolatility::getAtmLevel(fixingDate, targetIndex_, targetRateComputationPeriod_);

    Real t = timeFromReference(fixingDate);
    if (smileSectionCache_.find(t) == smileSectionCache_.end()) {
        auto atmAdjustedStrikes = strikes();
        for (Size k = 0; k < strikes().size(); ++k) {
            atmAdjustedStrikes[k] += baseAtmLevel - targetAtmLevel;
        }
        Interpolation2D volSpreadInterpolation = FlatExtrapolator2D(QuantLib::ext::make_shared<BilinearInterpolation>(
            optionTimes().begin(), optionTimes().end(), atmAdjustedStrikes.begin(), atmAdjustedStrikes.end(), volSpreadValues()));
        volSpreadInterpolation.enableExtrapolation();
        vector<Real> volSpreads(atmAdjustedStrikes.size());
        for (Size k = 0; k < atmAdjustedStrikes.size(); ++k) {
            volSpreads[k] = volSpreadInterpolation(t, atmAdjustedStrikes[k]);
        }
        smileSectionCache_[t] = QuantLib::ext::make_shared<SpreadedSmileSection2>(
            baseVol()->smileSection(t), volSpreads, atmAdjustedStrikes);
    }
    return smileSectionCache_[t];
}

QuantLib::ext::shared_ptr<SmileSection> AtmAdjustedSpreadedOptionletVolatility2::smileSectionImpl(Time optionTime) const {
    // imply a fixing date from the optionTime
    Date fixingDate = lowerDate(optionTime, referenceDate(), dayCounter());
    return smileSectionImpl(fixingDate);
}

Volatility AtmAdjustedSpreadedOptionletVolatility2::volatilityImpl(Time optionTime, Rate strike) const {
    return smileSectionImpl(optionTime)->volatility(strike) * scalingFactor_;
}

void AtmAdjustedSpreadedOptionletVolatility2::performCalculations() const {
    SpreadedOptionletVolatility2::performCalculations();
    smileSectionCache_.clear();
}

void AtmAdjustedSpreadedOptionletVolatility2::update() {
    SpreadedOptionletVolatility2::update();
    LazyObject::update();
}

void AtmAdjustedSpreadedOptionletVolatility2::deepUpdate() {
    SpreadedOptionletVolatility2::update();
    update();
}

} // namespace QuantExt
