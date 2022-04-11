/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

#include <qle/cashflows/overnightindexedcoupon.hpp>
#include <qle/termstructures/proxyoptionletvolatility.hpp>
#include <qle/utilities/time.hpp>

#include <ql/indexes/iborindex.hpp>
#include <ql/termstructures/volatility/smilesection.hpp>

namespace QuantExt {

using namespace QuantLib;

namespace {

bool isOis(const boost::shared_ptr<IborIndex>& index) {
    return boost::dynamic_pointer_cast<QuantLib::OvernightIndex>(index) != nullptr;
}

Date getStartDate(const boost::shared_ptr<OvernightIndex>& on, const Date& fixingDate,
                  const Period& rateComputationPeriod) {
    // fixingDate is the last fixing of an ON coupon, we first compute the accrual end
    Date accEnd = on->fixingCalendar().advance(fixingDate, on->fixingDays() * Days);
    // from that we go back the rate computatino period
    return on->fixingCalendar().advance(accEnd, -rateComputationPeriod);
}

Date getEndDate(const boost::shared_ptr<IborIndex>& ibor, const Date& fixingDate) {
    // from the fixing date we go to the value date
    Date valueDate = ibor->fixingCalendar().advance(fixingDate, ibor->fixingDays() * Days);
    // from there to the maturity
    return ibor->maturityDate(valueDate);
}

Real getOisAtmLevel(const boost::shared_ptr<OvernightIndex>& on, const Date& fixingDate,
                    const Period& rateComputationPeriod) {
    Date today = Settings::instance().evaluationDate();
    Date start = getStartDate(on, fixingDate, rateComputationPeriod);
    Date end = on->fixingCalendar().advance(start, rateComputationPeriod);
    Date minStart = on->fixingCalendar().advance(today, on->fixingDays() * Days);
    Date minEnd = std::max(minStart + 1, end);
    OvernightIndexedCoupon cpn(end, 1.0, minStart, minEnd, on);
    cpn.setPricer(boost::make_shared<OvernightIndexedCouponPricer>());
    return cpn.rate();
}

class AtmAdjustedSmileSection : public QuantLib::SmileSection {
public:
    AtmAdjustedSmileSection(const boost::shared_ptr<SmileSection>& base, const Real baseAtmLevel,
                            const Real targetAtmLevel)
        : SmileSection(), base_(base), baseAtmLevel_(baseAtmLevel), targetAtmLevel_(targetAtmLevel) {}
    Real minStrike() const override { return base_->minStrike(); }
    Real maxStrike() const override { return base_->maxStrike(); }
    Real atmLevel() const override { return targetAtmLevel_; }
    const Date& exerciseDate() const override { return base_->exerciseDate(); }
    VolatilityType volatilityType() const override { return base_->volatilityType(); }
    Rate shift() const override { return base_->shift(); }
    const Date& referenceDate() const override { return base_->referenceDate(); }
    Time exerciseTime() const override { return base_->exerciseTime(); }
    const DayCounter& dayCounter() const override { return base_->dayCounter(); }

private:
    Volatility volatilityImpl(Rate strike) const override {
        // just a moneyness, but no vol adjustment, so this is only suitable for normal vols
        return base_->volatility(strike + baseAtmLevel_ - targetAtmLevel_);
    };

    boost::shared_ptr<SmileSection> base_;
    Real baseAtmLevel_;
    Real targetAtmLevel_;
};

} // namespace

ProxyOptionletVolatility::ProxyOptionletVolatility(const Handle<OptionletVolatilityStructure>& baseVol,
                                                   const boost::shared_ptr<IborIndex>& baseIndex,
                                                   const boost::shared_ptr<IborIndex>& targetIndex,
                                                   const Period& baseRateComputationPeriod,
                                                   const Period& targetRateComputationPeriod)
    : OptionletVolatilityStructure(baseVol->businessDayConvention(), baseVol->dayCounter()), baseVol_(baseVol),
      baseIndex_(baseIndex), targetIndex_(targetIndex), baseRateComputationPeriod_(baseRateComputationPeriod),
      targetRateComputationPeriod_(targetRateComputationPeriod) {

    QL_REQUIRE(baseIndex != nullptr, "ProxyOptionletVolatility: no base index given.");
    QL_REQUIRE(targetIndex != nullptr, "ProxyOptionletVolatility: no target index given.");
    QL_REQUIRE(boost::dynamic_pointer_cast<QuantLib::OvernightIndex>(targetIndex_) == nullptr ||
                   targetRateComputationPeriod != 0 * Days,
               "ProxyOptionletVolatility: target index is OIS ("
                   << targetIndex->name() << "), so targetRateComputationPeriod must be given and != 0D.");
    QL_REQUIRE(boost::dynamic_pointer_cast<QuantLib::OvernightIndex>(baseIndex_) == nullptr ||
                   baseRateComputationPeriod != 0 * Days,
               "ProxyOptionletVolatility: base index is OIS ("
                   << baseIndex->name() << "), so baseRateComputationPeriod must be given and != 0D.");
    registerWith(baseVol_);
    registerWith(baseIndex_);
    registerWith(targetIndex_);
    enableExtrapolation(baseVol->allowsExtrapolation());
}

boost::shared_ptr<QuantLib::SmileSection> ProxyOptionletVolatility::smileSectionImpl(QuantLib::Time optionTime) const {
    // imply a fixing date from the optionTime
    Date fixingDate = lowerDate(optionTime, referenceDate(), dayCounter());
    return smileSectionImpl(fixingDate);
}

boost::shared_ptr<SmileSection> ProxyOptionletVolatility::smileSectionImpl(const Date& fixingDate) const {

    // if base and target are both ibor or both ois the base fixing date will be the same as the target fixing date

    Date baseFixingDate = fixingDate;

    if (!isOis(baseIndex_) && isOis(targetIndex_))
        baseFixingDate =
            baseIndex_->fixingDate(getStartDate(boost::dynamic_pointer_cast<QuantLib::OvernightIndex>(targetIndex_),
                                                fixingDate, targetRateComputationPeriod_));
    else if (isOis(baseIndex_) && !isOis(targetIndex_))
        baseFixingDate = baseIndex_->fixingCalendar().advance(getEndDate(targetIndex_, fixingDate),
                                                              -static_cast<int>(baseIndex_->fixingDays()) * Days);

    Date today = Settings::instance().evaluationDate();
    baseFixingDate = std::max(today + 1, baseFixingDate);

    // compute the base and target forward rate levels
    Real baseAtmLevel = isOis(baseIndex_) ? getOisAtmLevel(boost::dynamic_pointer_cast<OvernightIndex>(baseIndex_),
                                                           baseFixingDate, baseRateComputationPeriod_)
                                          : baseIndex_->fixing(baseFixingDate);
    Real targetAtmLevel = isOis(targetIndex_)
                              ? getOisAtmLevel(boost::dynamic_pointer_cast<OvernightIndex>(targetIndex_), fixingDate,
                                               targetRateComputationPeriod_)
                              : targetIndex_->fixing(fixingDate);

    // build the atm-adjusted smile section and return it

    QL_REQUIRE(!baseVol_.empty(), "ProxyOptionletVolatility: no base vol given.");
    return boost::make_shared<AtmAdjustedSmileSection>(baseVol_->smileSection(baseFixingDate, true), baseAtmLevel,
                                                       targetAtmLevel);
}

Volatility ProxyOptionletVolatility::volatilityImpl(Time optionTime, Rate strike) const {
    return smileSection(optionTime)->volatility(strike);
}

} // namespace QuantExt
