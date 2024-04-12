/*
Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file bmaindexwrapper.hpp
\brief wrapper class for bmaindex, for the purpose of providing iborindex inheritance.
\ingroup indexes
*/

#ifndef quantext_bma_index_wrapper_hpp
#define quantext_bma_index_wrapper_hpp

#include <ql/indexes/bmaindex.hpp>
#include <ql/indexes/iborindex.hpp>

namespace QuantExt {
using namespace QuantLib;

/*! Wrapper that adapts the quantlib BMAIndex into a class inheriting from IborIndex
    The purpose of this is twofold:
    1) we can use Market::iborIndex() to retrieve a BMA index
    2) we can set up an IborCoupon using this index wrapper to approximate an AveragedBMACoupon
       at places where a pricer only supports an IborCoupon, e.g. for cap/floors or swaptions on
       BMA underlyings
    To make 2) work we tweak the implementations of isValidFixingDate(), maturityDate() and pastFixing()
    to make sure an Ibor coupon on this index class will behave gracefully. */
/*!
\ingroup indexes
*/
class BMAIndexWrapper : public IborIndex {
public:
    // TODO: fix the day count convention
    // TODO: fix the end of month
    BMAIndexWrapper(const QuantLib::ext::shared_ptr<QuantLib::BMAIndex>& bma)
        : IborIndex(bma->name(), bma->tenor(), bma->fixingDays(), bma->currency(), bma->fixingCalendar(),
                    ModifiedFollowing, false, bma->dayCounter(), bma->forwardingTermStructure()),
          bma_(bma) {}

    BMAIndexWrapper(const QuantLib::ext::shared_ptr<QuantLib::BMAIndex>& bma, const Handle<YieldTermStructure>& h)
        : IborIndex(bma->name(), bma->tenor(), bma->fixingDays(), bma->currency(), bma->fixingCalendar(),
                    ModifiedFollowing, false, bma->dayCounter(), h),
          bma_(new BMAIndex(h)) {}

    // overwrite all the virtual methods
    std::string name() const override { return bma_->name(); }
    bool isValidFixingDate(const Date& date) const override {
        // this is not the original BMA behaviour!
        return fixingCalendar().isBusinessDay(date);
    }
    Handle<YieldTermStructure> forwardingTermStructure() const { return bma_->forwardingTermStructure(); }
    Date maturityDate(const Date& valueDate) const override {
        Date d = bma_->maturityDate(valueDate);
        // make sure that d > valueDate to avoid problems in IborCoupon, this is not the original
        // BMAIndex behaviour!
        return std::max<Date>(d, valueDate + 1);
    }
    Schedule fixingSchedule(const Date& start, const Date& end) { return bma_->fixingSchedule(start, end); }
    Rate forecastFixing(const Date& fixingDate) const override {
        QL_REQUIRE(!termStructure_.empty(), "null term structure set to this instance of " << name());
        Date start = fixingCalendar().advance(fixingDate, 1, Days);
        Date end = maturityDate(start);
        return termStructure_->forwardRate(start, end, dayCounter_, Simple);
    }
    // return the last valid BMA fixing date before or on the given fixingDate
    Date adjustedFixingDate(const Date& fixingDate) const {
        Date tmp = fixingDate;
        while (!bma_->isValidFixingDate(tmp) && tmp > Date::minDate())
            tmp--;
        return tmp;
    }
    Rate pastFixing(const Date& fixingDate) const override {
        // we allow for fixing dates that are not valid BMA fixing dates, so we need to make sure that we
        // read a past fixing from a valid BMA fixing date
        return bma_->fixing(adjustedFixingDate(fixingDate));
    }
    QuantLib::ext::shared_ptr<IborIndex> clone(const Handle<YieldTermStructure>& h) const override {
        return QuantLib::ext::shared_ptr<BMAIndexWrapper>(new BMAIndexWrapper(bma(), h));
    }

    // do we need these?
    QuantLib::ext::shared_ptr<QuantLib::BMAIndex> bma() const { return bma_; }

    operator QuantLib::BMAIndex &() { return *bma_; }
    operator QuantLib::BMAIndex *() { return &*bma_; }

private:
    QuantLib::ext::shared_ptr<QuantLib::BMAIndex> bma_;
};
} // namespace QuantExt

#endif
