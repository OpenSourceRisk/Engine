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

#include <qle/termstructures/averageoffpeakpowerhelper.hpp>
#include <ql/utilities/null_deleter.hpp>

using QuantLib::AcyclicVisitor;
using QuantLib::Calendar;
using QuantLib::Date;
using QuantLib::Handle;
using QuantLib::Natural;
using QuantLib::Quote;
using QuantLib::Real;
using QuantLib::Visitor;
using std::max;
using std::min;

namespace QuantExt {

AverageOffPeakPowerHelper::AverageOffPeakPowerHelper(const Handle<Quote>& price,
    const QuantLib::ext::shared_ptr<CommodityIndex>& index,
    const Date& start,
    const Date& end,
    const QuantLib::ext::shared_ptr<FutureExpiryCalculator>& calc,
    const QuantLib::ext::shared_ptr<CommodityIndex>& peakIndex,
    const Calendar& peakCalendar,
    Natural peakHoursPerDay)
    : PriceHelper(price) {
    init(index, start, end, calc, peakIndex, peakCalendar, peakHoursPerDay);
}

AverageOffPeakPowerHelper::AverageOffPeakPowerHelper(Real price,
    const QuantLib::ext::shared_ptr<CommodityIndex>& index,
    const Date& start,
    const Date& end,
    const QuantLib::ext::shared_ptr<FutureExpiryCalculator>& calc,
    const QuantLib::ext::shared_ptr<CommodityIndex>& peakIndex,
    const Calendar& peakCalendar,
    Natural peakHoursPerDay)
    : PriceHelper(price) {
    init(index, start, end, calc, peakIndex, peakCalendar, peakHoursPerDay);
}

void AverageOffPeakPowerHelper::init(const QuantLib::ext::shared_ptr<CommodityIndex>& index,
    const Date& start, const Date& end, const QuantLib::ext::shared_ptr<FutureExpiryCalculator>& calc,
    const QuantLib::ext::shared_ptr<CommodityIndex>& peakIndex, const Calendar& peakCalendar,
    Natural peakHoursPerDay) {

    QL_REQUIRE(peakHoursPerDay <= 24, "AverageOffPeakPowerHelper: peak hours per day should not be greater than 24.");
    Natural ophpd = 24 - peakHoursPerDay;

    // Make a copy of the commodity index linked to this price helper's price term structure handle, 
    // termStructureHandle_.
    auto indexClone = index->clone(Date(), termStructureHandle_);

    // While bootstrapping is happening, this price helper's price term structure handle, termStructureHandle_, will 
    // be updated multiple times. We don't want the index notified each time.
    indexClone->unregisterWith(termStructureHandle_);
    registerWith(indexClone);

    // Create the business day off-peak portion of the cashflow.
    bool useBusinessDays = true;
    businessOffPeak_ = QuantLib::ext::make_shared<CommodityIndexedAverageCashFlow>(1.0, start, end, end, indexClone,
        peakCalendar, 0.0, 1.0, true, 0, 0, calc, true, false, useBusinessDays);
    peakDays_ = businessOffPeak_->indices().size();

    // Create the holiday off-peak portion of the cashflow.
    useBusinessDays = false;
    holidayOffPeak_ = QuantLib::ext::make_shared<CommodityIndexedAverageCashFlow>(ophpd / 24.0, start, end, end, indexClone,
        peakCalendar, 0.0, 1.0, true, 0, 0, calc, true, false, useBusinessDays);
    nonPeakDays_ = holidayOffPeak_->indices().size();

    // Create the holiday peak portion of the cashflow.
    holidayPeak_ = QuantLib::ext::make_shared<CommodityIndexedAverageCashFlow>(peakHoursPerDay / 24.0, start, end, end,
        peakIndex, peakCalendar, 0.0, 1.0, true, 0, 0, calc, true, false, useBusinessDays);

    // Get the date index pairs involved in the averaging. The earliest date is the expiry date of the future contract 
    // referenced in the first element and the latest date is the expiry date of the future contract referenced in 
    // the last element. We must look at both off peak cashflows here.
    const auto& mpBop = businessOffPeak_->indices();
    const auto& mpHop = holidayOffPeak_->indices();
    earliestDate_ = min(mpBop.begin()->second->expiryDate(), mpHop.begin()->second->expiryDate());
    pillarDate_ = max(mpBop.rbegin()->second->expiryDate(), mpHop.rbegin()->second->expiryDate());
}

Real AverageOffPeakPowerHelper::impliedQuote() const {
    QL_REQUIRE(termStructure_, "AverageFuturePriceHelper term structure not set.");
    businessOffPeak_->update();
    holidayOffPeak_->update();
    holidayPeak_->update();
    return (peakDays_ * businessOffPeak_->amount() + nonPeakDays_ * (holidayOffPeak_->amount()
        + holidayPeak_->amount())) / (peakDays_ + nonPeakDays_);
}

void AverageOffPeakPowerHelper::setTermStructure(PriceTermStructure* ts) {
    QuantLib::ext::shared_ptr<PriceTermStructure> temp(ts, null_deleter());
    // Do not set the relinkable handle as an observer i.e. registerAsObserver is false here.
    termStructureHandle_.linkTo(temp, false);
    PriceHelper::setTermStructure(ts);
}

void AverageOffPeakPowerHelper::accept(AcyclicVisitor& v) {
    if (auto vis = dynamic_cast<Visitor<AverageOffPeakPowerHelper>*>(&v))
        vis->visit(*this);
    else
        PriceHelper::accept(v);
}

void AverageOffPeakPowerHelper::deepUpdate() {
    if (businessOffPeak_)
        businessOffPeak_->update();
    if (holidayOffPeak_)
        holidayOffPeak_->update();
    if (holidayPeak_)
        holidayPeak_->update();
}

} // namespace QuantExt
