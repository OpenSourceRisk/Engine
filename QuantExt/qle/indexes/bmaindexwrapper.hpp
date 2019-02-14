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
\brief wrapper class for bmaindex, for the purpose of providing ibroindex inheritance.
\ingroup indexes
*/

#ifndef quantext_bma_index_wrapper_hpp
#define quantext_bma_index_wrapper_hpp

#include <ql/indexes/bmaindex.hpp>
#include <ql/indexes/iborindex.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Wrapper that adapts the quantlib BMAIndex into a class inheriting from IborIndex
/*!
\ingroup indexes
*/
class BMAIndexWrapper : public IborIndex {
public:
    // TODO: fix the day count convention
    // TODO: fix the end of month
    BMAIndexWrapper(const boost::shared_ptr<QuantLib::BMAIndex>& bma)
        : IborIndex(bma->name(), bma->tenor(), bma->fixingDays(), bma->currency(), bma->fixingCalendar(),
                    ModifiedFollowing, false, bma->dayCounter(), bma->forwardingTermStructure()),
          bma_(bma) {}

    BMAIndexWrapper(const boost::shared_ptr<QuantLib::BMAIndex>& bma, const Handle<YieldTermStructure>& h)
        : IborIndex(bma->name(), bma->tenor(), bma->fixingDays(), bma->currency(), bma->fixingCalendar(),
                    ModifiedFollowing, false, bma->dayCounter(), h),
          bma_(new BMAIndex(h)) {}

    // overwrite all the virtual methods
    std::string name() const { return "BMA"; }
    bool isValidFixingDate(const Date& date) const { return bma_->isValidFixingDate(date); }
    Handle<YieldTermStructure> forwardingTermStructure() const { return bma_->forwardingTermStructure(); }
    Date maturityDate(const Date& valueDate) const { return bma_->maturityDate(valueDate); }
    Schedule fixingSchedule(const Date& start, const Date& end) { return bma_->fixingSchedule(start, end); }
    Rate forecastFixing(const Date& fixingDate) const {
        QL_REQUIRE(!termStructure_.empty(), "null term structure set to this instance of " << name());
        Date start = fixingCalendar().advance(fixingDate, 1, Days);
        Date end = maturityDate(start);
        return termStructure_->forwardRate(start, end, dayCounter_, Simple);
    }
    boost::shared_ptr<IborIndex> clone(const Handle<YieldTermStructure>& h) const {
        return boost::shared_ptr<BMAIndexWrapper>(new BMAIndexWrapper(bma(), h));
    }

    // do we need these?
    boost::shared_ptr<QuantLib::BMAIndex> bma() const { return bma_; }

    operator QuantLib::BMAIndex&() { return *bma_; }
    operator QuantLib::BMAIndex*() { return &*bma_; }

private:
    boost::shared_ptr<QuantLib::BMAIndex> bma_;
};
} // namespace QuantExt
// namespace QuantExt

#endif
