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
#include <qle/indexes/bmaindexwrapper.hpp>
#include <qle/termstructures/atmadjustedsmilesection.hpp>
#include <qle/termstructures/proxyoptionletvolatility.hpp>
#include <qle/utilities/cashflows.hpp>
#include <qle/utilities/time.hpp>

#include <ql/cashflows/averagebmacoupon.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/termstructures/volatility/smilesection.hpp>

namespace QuantExt {

using namespace QuantLib;

namespace {

bool isOis(const QuantLib::ext::shared_ptr<IborIndex>& index) {
    return QuantLib::ext::dynamic_pointer_cast<QuantLib::OvernightIndex>(index) != nullptr;
}

bool isBMA(const QuantLib::ext::shared_ptr<IborIndex>& index) {
    return QuantLib::ext::dynamic_pointer_cast<QuantExt::BMAIndexWrapper>(index) != nullptr;
}

} // namespace

ProxyOptionletVolatility::ProxyOptionletVolatility(const Handle<OptionletVolatilityStructure>& baseVol,
                                                   const QuantLib::ext::shared_ptr<IborIndex>& baseIndex,
                                                   const QuantLib::ext::shared_ptr<IborIndex>& targetIndex,
                                                   const Period& baseRateComputationPeriod,
                                                   const Period& targetRateComputationPeriod)
    : OptionletVolatilityStructure(baseVol->businessDayConvention(), baseVol->dayCounter()), baseVol_(baseVol),
      baseIndex_(baseIndex), targetIndex_(targetIndex), baseRateComputationPeriod_(baseRateComputationPeriod),
      targetRateComputationPeriod_(targetRateComputationPeriod) {

    QL_REQUIRE(baseIndex != nullptr, "ProxyOptionletVolatility: no base index given.");
    QL_REQUIRE(targetIndex != nullptr, "ProxyOptionletVolatility: no target index given.");
    QL_REQUIRE((!isOis(targetIndex_) && !isBMA(targetIndex)) || targetRateComputationPeriod != 0 * Days,
               "ProxyOptionletVolatility: target index is OIS or BMA/SIFMA ("
                   << targetIndex->name() << "), so targetRateComputationPeriod must be given and != 0D.");
    QL_REQUIRE((!isOis(baseIndex_) && !isBMA(baseIndex_)) || baseRateComputationPeriod != 0 * Days,
               "ProxyOptionletVolatility: base index is OIS or BMA/SIFMA ("
                   << baseIndex->name() << "), so baseRateComputationPeriod must be given and != 0D.");
    registerWith(baseVol_);
    registerWith(baseIndex_);
    registerWith(targetIndex_);
    enableExtrapolation(baseVol->allowsExtrapolation());
}

QuantLib::ext::shared_ptr<QuantLib::SmileSection> ProxyOptionletVolatility::smileSectionImpl(QuantLib::Time optionTime) const {
    // imply a fixing date from the optionTime
    Date fixingDate = lowerDate(optionTime, referenceDate(), dayCounter());
    return smileSectionImpl(fixingDate);
}

QuantLib::ext::shared_ptr<SmileSection> ProxyOptionletVolatility::smileSectionImpl(const Date& fixingDate) const {

    // compute the base and target forward rate levels

    Real baseAtmLevel;
    if (isOis(baseIndex_))
        baseAtmLevel = getOisAtmLevel(QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(baseIndex_),
                                      baseIndex_->fixingCalendar().adjust(fixingDate), baseRateComputationPeriod_);
    else if (isBMA(baseIndex_))
        baseAtmLevel = getBMAAtmLevel(QuantLib::ext::dynamic_pointer_cast<BMAIndexWrapper>(baseIndex_)->bma(),
                                      baseIndex_->fixingCalendar().adjust(fixingDate), baseRateComputationPeriod_);
    else
        baseAtmLevel = baseIndex_->fixing(baseIndex_->fixingCalendar().adjust(fixingDate));

    Real targetAtmLevel;
    if (isOis(targetIndex_))
        targetAtmLevel =
            getOisAtmLevel(QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(targetIndex_),
                           targetIndex_->fixingCalendar().adjust(fixingDate), targetRateComputationPeriod_);
    else if (isBMA(targetIndex_))
        targetAtmLevel =
            getBMAAtmLevel(QuantLib::ext::dynamic_pointer_cast<BMAIndexWrapper>(targetIndex_)->bma(),
                           targetIndex_->fixingCalendar().adjust(fixingDate), targetRateComputationPeriod_);
    else
        targetAtmLevel = targetIndex_->fixing(targetIndex_->fixingCalendar().adjust(fixingDate));

    // build the atm-adjusted smile section and return it

    QL_REQUIRE(!baseVol_.empty(), "ProxyOptionletVolatility: no base vol given.");
    return QuantLib::ext::make_shared<AtmAdjustedSmileSection>(baseVol_->smileSection(fixingDate, true), baseAtmLevel,
                                                       targetAtmLevel);
}

Volatility ProxyOptionletVolatility::volatilityImpl(Time optionTime, Rate strike) const {
    return smileSection(optionTime)->volatility(strike);
}

} // namespace QuantExt
