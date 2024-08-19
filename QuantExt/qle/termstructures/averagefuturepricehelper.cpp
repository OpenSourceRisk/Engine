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

#include <qle/termstructures/averagefuturepricehelper.hpp>
#include <ql/utilities/null_deleter.hpp>

using QuantLib::AcyclicVisitor;
using QuantLib::Calendar;
using QuantLib::Date;
using QuantLib::Handle;
using QuantLib::Natural;
using QuantLib::Quote;
using QuantLib::Real;
using QuantLib::Visitor;

namespace QuantExt {

AverageFuturePriceHelper::AverageFuturePriceHelper(const Handle<Quote>& price,
    const QuantLib::ext::shared_ptr<CommodityIndex>& index,
    const Date& start,
    const Date& end,
    const QuantLib::ext::shared_ptr<FutureExpiryCalculator>& calc,
    const Calendar& calendar,
    Natural deliveryDateRoll,
    Natural futureMonthOffset,
    bool useBusinessDays,
    Natural dailyExpiryOffset)
    : PriceHelper(price) {
    init(index, start, end, calc, calendar, deliveryDateRoll, futureMonthOffset, useBusinessDays, dailyExpiryOffset);
}

AverageFuturePriceHelper::AverageFuturePriceHelper(Real price,
    const QuantLib::ext::shared_ptr<CommodityIndex>& index,
    const Date& start,
    const Date& end,
    const QuantLib::ext::shared_ptr<FutureExpiryCalculator>& calc,
    const Calendar& calendar,
    Natural deliveryDateRoll,
    Natural futureMonthOffset,
    bool useBusinessDays,
    Natural dailyExpiryOffset)
    : PriceHelper(price) {
    init(index, start, end, calc, calendar, deliveryDateRoll, futureMonthOffset, useBusinessDays, dailyExpiryOffset);
}

void AverageFuturePriceHelper::init(const QuantLib::ext::shared_ptr<CommodityIndex>& index,
    const Date& start, const Date& end, const QuantLib::ext::shared_ptr<FutureExpiryCalculator>& calc,
    const Calendar& calendar, Natural deliveryDateRoll, Natural futureMonthOffset,
    bool useBusinessDays, Natural dailyExpiryOffset) {

    // Make a copy of the commodity index linked to this price helper's price term structure handle, 
    // termStructureHandle_.
    auto indexClone = index->clone(Date(), termStructureHandle_);

    // While bootstrapping is happening, this price helper's price term structure handle, termStructureHandle_, will 
    // be updated multiple times. We don't want the index notified each time.
    indexClone->unregisterWith(termStructureHandle_);
    registerWith(indexClone);

    // Create the averaging cashflow referencing the commodity index.
    averageCashflow_ = QuantLib::ext::make_shared<CommodityIndexedAverageCashFlow>(1.0, start, end, end, indexClone,
        calendar, 0.0, 1.0, true, deliveryDateRoll, futureMonthOffset, calc, true, false, useBusinessDays,
        CommodityQuantityFrequency::PerCalculationPeriod, Null<Natural>(), dailyExpiryOffset);

    // Get the date index pairs involved in the averaging. The earliest date is the expiry date of the future contract 
    // referenced in the first element and the latest date is the expiry date of the future contract referenced in 
    // the last element.
    const auto& mp = averageCashflow_->indices();
    earliestDate_ = mp.begin()->second->expiryDate();
    pillarDate_ = mp.rbegin()->second->expiryDate();
}

Real AverageFuturePriceHelper::impliedQuote() const {
    QL_REQUIRE(termStructure_, "AverageFuturePriceHelper term structure not set.");
    averageCashflow_->update();
    return averageCashflow_->amount();
}

void AverageFuturePriceHelper::setTermStructure(PriceTermStructure* ts) {
    QuantLib::ext::shared_ptr<PriceTermStructure> temp(ts, null_deleter());
    // Do not set the relinkable handle as an observer i.e. registerAsObserver is false here.
    termStructureHandle_.linkTo(temp, false);
    PriceHelper::setTermStructure(ts);
}

void AverageFuturePriceHelper::accept(AcyclicVisitor& v) {
    if (auto vis = dynamic_cast<Visitor<AverageFuturePriceHelper>*>(&v))
        vis->visit(*this);
    else
        PriceHelper::accept(v);
}

QuantLib::ext::shared_ptr<CommodityIndexedAverageCashFlow> AverageFuturePriceHelper::averageCashflow() const {
    return averageCashflow_;
}

void AverageFuturePriceHelper::deepUpdate() {
    if (averageCashflow_)
        averageCashflow_->update();
    update();
}

} // namespace QuantExt
