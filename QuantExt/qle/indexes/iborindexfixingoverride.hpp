/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

/*! \file qle/indexes/iborindexfixingoverride.hpp
    \brief ibor index wrapper with fixings
    \ingroup indexes
*/

#pragma once

#include <ql/indexes/iborindex.hpp>

namespace QuantExt {

//! wrapper for ibor index wit individiual trade level fixings
class IborIndexWithFixingOverride : public QuantLib::IborIndex {
public:
    IborIndexWithFixingOverride(const QuantLib::ext::shared_ptr<QuantLib::IborIndex>& index,
                                const std::map<QuantLib::Date, double>& fixingOverrides)
        : IborIndexWithFixingOverride(index->familyName(), index->tenor(), index->fixingDays(), index->currency(),
                                      index->fixingCalendar(), index->businessDayConvention(), index->endOfMonth(),
                                      index->dayCounter(), index->forwardingTermStructure(), fixingOverrides) {}

    IborIndexWithFixingOverride(const std::string& familyName, const QuantLib::Period& tenor,
                                QuantLib::Natural settlementDays, const QuantLib::Currency& currency,
                                const QuantLib::Calendar& fixingCalendar, QuantLib::BusinessDayConvention convention,
                                bool endOfMonth, const QuantLib::DayCounter& dayCounter,
                                QuantLib::Handle<QuantLib::YieldTermStructure> h,
                                const std::map<QuantLib::Date, double>& fixingOverrides)
        : QuantLib::IborIndex(familyName, tenor, settlementDays, currency, fixingCalendar, convention, endOfMonth,
                              dayCounter, h),
          fixingOverrides_(fixingOverrides) {}
    //! \name InterestRateIndex interface
    //@{

    QuantLib::ext::shared_ptr<QuantLib::IborIndex>
    clone(const QuantLib::Handle<QuantLib::YieldTermStructure>& forwarding) const override {
        return ext::make_shared<IborIndexWithFixingOverride>(familyName(), tenor(), fixingDays(), currency(),
                                                             fixingCalendar(), businessDayConvention(), endOfMonth(),
                                                             dayCounter(), forwarding, fixingOverrides_);
    }
    // @}
protected:
    QuantLib::Rate pastFixing(const QuantLib::Date& fixingDate) const override {
        auto histFixing = fixingOverrides_.find(fixingDate);
        if (histFixing != fixingOverrides_.end()) {
            return histFixing->second;
        } else {
            return QuantLib::IborIndex::pastFixing(fixingDate);
        }
    }

private:
    std::map<QuantLib::Date, double> fixingOverrides_;
};

class OvernightIndexWithFixingOverride : public QuantLib::OvernightIndex {
public:
    OvernightIndexWithFixingOverride(const QuantLib::ext::shared_ptr<QuantLib::OvernightIndex>& index,
                                     const std::map<QuantLib::Date, double>& fixingOverrides)
        : OvernightIndexWithFixingOverride(index->familyName(), index->fixingDays(), index->currency(),
                                           index->fixingCalendar(), index->dayCounter(),
                                           index->forwardingTermStructure(), fixingOverrides) {}

    OvernightIndexWithFixingOverride(const std::string& familyName, QuantLib::Natural settlementDays,
                                     const QuantLib::Currency& currency, const QuantLib::Calendar& fixingCalendar,
                                     const QuantLib::DayCounter& dayCounter,
                                     const QuantLib::Handle<QuantLib::YieldTermStructure>& h,
                                     const std::map<QuantLib::Date, double>& fixingOverrides)
        : QuantLib::OvernightIndex(familyName, settlementDays, currency, fixingCalendar, dayCounter, h),
          fixingOverrides_(fixingOverrides) {}

    QuantLib::ext::shared_ptr<QuantLib::IborIndex>
    clone(const QuantLib::Handle<QuantLib::YieldTermStructure>& forwarding) const override {
        return ext::make_shared<OvernightIndexWithFixingOverride>(
            familyName(), fixingDays(), currency(), fixingCalendar(), dayCounter(), forwarding, fixingOverrides_);
    }

protected:
    QuantLib::Rate pastFixing(const QuantLib::Date& fixingDate) const override {
        auto histFixing = fixingOverrides_.find(fixingDate);
        if (histFixing != fixingOverrides_.end()) {
            return histFixing->second;
        } else {
            return QuantLib::IborIndex::pastFixing(fixingDate);
        }
    }

private:
    std::map<QuantLib::Date, double> fixingOverrides_;
};
} // namespace QuantExt