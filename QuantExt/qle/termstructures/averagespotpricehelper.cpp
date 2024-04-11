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

#include <qle/termstructures/averagespotpricehelper.hpp>
#include <ql/utilities/null_deleter.hpp>

using QuantLib::AcyclicVisitor;
using QuantLib::Calendar;
using QuantLib::Date;
using QuantLib::Handle;
using QuantLib::Quote;
using QuantLib::Real;
using QuantLib::Visitor;

namespace QuantExt {

AverageSpotPriceHelper::AverageSpotPriceHelper(const Handle<Quote>& price,
    const QuantLib::ext::shared_ptr<CommoditySpotIndex>& index,
    const Date& start,
    const Date& end,
    const Calendar& calendar,
    bool useBusinessDays)
    : PriceHelper(price) {
    init(index, start, end, calendar, useBusinessDays);
}

AverageSpotPriceHelper::AverageSpotPriceHelper(Real price,
    const QuantLib::ext::shared_ptr<CommoditySpotIndex>& index,
    const Date& start,
    const Date& end,
    const Calendar& calendar,
    bool useBusinessDays)
    : PriceHelper(price) {
    init(index, start, end, calendar, useBusinessDays);
}

void AverageSpotPriceHelper::init(const QuantLib::ext::shared_ptr<CommoditySpotIndex>& index,
    const Date& start, const Date& end, const Calendar& calendar, bool useBusinessDays) {

    // Make a copy of the commodity spot index linked to this price helper's price term structure handle, 
    // termStructureHandle_.
    auto indexClone = QuantLib::ext::make_shared<CommoditySpotIndex>(index->underlyingName(),
        index->fixingCalendar(), termStructureHandle_);

    // While bootstrapping is happening, this price helper's price term structure handle, termStructureHandle_, will 
    // be updated multiple times. We don't want the index notified each time.
    indexClone->unregisterWith(termStructureHandle_);
    registerWith(indexClone);

    // Create the averaging cashflow referencing the commodity spot index. Need to specify all the defaults 
    // here just to amend the final default parameter i.e. set excludeStartDate to false.
    averageCashflow_ = QuantLib::ext::make_shared<CommodityIndexedAverageCashFlow>(1.0, start, end, end, indexClone,
        calendar, 0.0, 1.0, false, 0, 0, nullptr, true, false, useBusinessDays);

    // Get the date index pairs involved in the averaging. The earliest date is the date of the first element 
    // and the latest date is the date of the last element.
    const auto& mp = averageCashflow_->indices();
    earliestDate_ = mp.begin()->first;
    pillarDate_ = mp.rbegin()->first;
}

Real AverageSpotPriceHelper::impliedQuote() const {
    QL_REQUIRE(termStructure_, "AverageSpotPriceHelper term structure not set.");
    return averageCashflow_->amount();
}

void AverageSpotPriceHelper::setTermStructure(PriceTermStructure* ts) {
    QuantLib::ext::shared_ptr<PriceTermStructure> temp(ts, null_deleter());
    // Do not set the relinkable handle as an observer i.e. registerAsObserver is false here.
    termStructureHandle_.linkTo(temp, false);
    PriceHelper::setTermStructure(ts);
}

void AverageSpotPriceHelper::accept(AcyclicVisitor& v) {
    if (auto vis = dynamic_cast<Visitor<AverageSpotPriceHelper>*>(&v))
        vis->visit(*this);
    else
        PriceHelper::accept(v);
}

QuantLib::ext::shared_ptr<CommodityIndexedAverageCashFlow> AverageSpotPriceHelper::averageCashflow() const {
    return averageCashflow_;
}

}
