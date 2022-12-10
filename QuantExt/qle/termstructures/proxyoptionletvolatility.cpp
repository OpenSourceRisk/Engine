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
#include <qle/termstructures/atmadjustedsmilesection.hpp>
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

Real getOisAtmLevel(const boost::shared_ptr<OvernightIndex>& on, const Date& fixingDate,
                    const Period& rateComputationPeriod) {
    Date today = Settings::instance().evaluationDate();
    Date start = on->valueDate(fixingDate);
    Date end = on->fixingCalendar().advance(start, rateComputationPeriod);
    Date adjStart = std::max(start, today);
    Date adjEnd = std::max(adjStart + 1, end);
    OvernightIndexedCoupon cpn(end, 1.0, adjStart, adjEnd, on);
    cpn.setPricer(boost::make_shared<OvernightIndexedCouponPricer>());
    return cpn.rate();
}

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

    // compute the base and target forward rate levels

    Real baseAtmLevel = isOis(baseIndex_) ? getOisAtmLevel(boost::dynamic_pointer_cast<OvernightIndex>(baseIndex_),
                                           baseIndex_->fixingCalendar().adjust(fixingDate), baseRateComputationPeriod_)
                                          : baseIndex_->fixing(baseIndex_->fixingCalendar().adjust(fixingDate));
    Real targetAtmLevel = isOis(targetIndex_)
            ? getOisAtmLevel(boost::dynamic_pointer_cast<OvernightIndex>(targetIndex_),
                             targetIndex_->fixingCalendar().adjust(fixingDate), targetRateComputationPeriod_)
                              : targetIndex_->fixing(targetIndex_->fixingCalendar().adjust(fixingDate));

    // build the atm-adjusted smile section and return it

    QL_REQUIRE(!baseVol_.empty(), "ProxyOptionletVolatility: no base vol given.");
    return boost::make_shared<AtmAdjustedSmileSection>(baseVol_->smileSection(fixingDate, true), baseAtmLevel,
                                                       targetAtmLevel);
}

Volatility ProxyOptionletVolatility::volatilityImpl(Time optionTime, Rate strike) const {
    return smileSection(optionTime)->volatility(strike);
}

} // namespace QuantExt
