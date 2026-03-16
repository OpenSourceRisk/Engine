/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*! \file cmscaphelper.hpp
    \brief Cms Option helper class
*/

#ifndef quantext_cms_calibration_helper_h
#define quantext_cms_calibration_helper_h

#include <list>
#include <ql/indexes/swapindex.hpp>
#include <ql/instruments/capfloor.hpp>
#include <ql/models/calibrationhelper.hpp>
#include <ql/patterns/lazyobject.hpp>
#include <ql/quote.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <qle/cashflows/lognormalcmsspreadpricer.hpp>

namespace QuantExt {
using namespace QuantLib;
class CmsCapHelper : public LazyObject, public CalibrationHelper {
public:
    CmsCapHelper(Date asof, QuantLib::ext::shared_ptr<SwapIndex>& index1, QuantLib::ext::shared_ptr<SwapIndex>& index2,
                 const Handle<YieldTermStructure>& yts, const Handle<Quote>& price, const Handle<Quote>& correlation,
                 const Period& length, const Period& forwardStart, const Period& spotDays, const Period& cmsTenor,
                 Natural fixingDays, const Calendar& calendar, const DayCounter& daycounter,
                 const BusinessDayConvention& convention, QuantLib::ext::shared_ptr<FloatingRateCouponPricer>& pricer,
                 QuantLib::ext::shared_ptr<QuantLib::CmsCouponPricer>& cmsPricer)
        : asof_(asof), index1_(index1), index2_(index2), discountCurve_(yts), marketValue_(price->value()),
          correlation_(correlation), length_(length), forwardStart_(forwardStart), spotDays_(spotDays),
          cmsTenor_(cmsTenor), fixingDays_(fixingDays), calendar_(calendar), dayCounter_(daycounter),
          convention_(convention), pricer_(pricer), cmsPricer_(cmsPricer) {

        registerWith(correlation_);
    }

    void performCalculations() const override;

    //! returns the actual price of the instrument (from volatility)
    QuantLib::Real marketValue() const { return marketValue_; }

    //! returns the price of the instrument according to the model
    QuantLib::Real modelValue() const;

    //! returns the error resulting from the model valuation
    QuantLib::Real calibrationError() override { return marketValue() - modelValue(); }

protected:
    Date asof_;
    QuantLib::ext::shared_ptr<SwapIndex> index1_;
    QuantLib::ext::shared_ptr<SwapIndex> index2_;
    Handle<YieldTermStructure> discountCurve_;
    Real marketValue_;
    Handle<Quote> correlation_;
    Period length_;

    Period forwardStart_;
    Period spotDays_;
    Period cmsTenor_;
    Natural fixingDays_;
    Calendar calendar_;
    DayCounter dayCounter_;
    BusinessDayConvention convention_;

    QuantLib::ext::shared_ptr<FloatingRateCouponPricer> pricer_;
    QuantLib::ext::shared_ptr<QuantLib::CmsCouponPricer> cmsPricer_;

private:
    mutable QuantLib::ext::shared_ptr<QuantLib::Swap> cap_;
};

} // namespace QuantExt

#endif
