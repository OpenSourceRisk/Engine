/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

/*! \file qle/indexes/offpeakpowerindex.hpp
    \brief commodity future index for off peak power prices.
    \ingroup indexes
*/

#ifndef quantext_off_peak_power_index_hpp
#define quantext_off_peak_power_index_hpp

#include <qle/indexes/commodityindex.hpp>

namespace QuantExt {

//! Off peak power index
/*! A commodity index to represent daily off-peak power prices.

    In general, when used in derivatives the off-peak power value for a given date will be:
    1. the average of Locational Marginal Prices (LMPs) over the off-peak hours, generally 8, on peak calendar 
       business days
    2. the average of LMPs over all hours on peak calendar holidays

    There are generally two types of daily futures in the power markets:
    1. those that average the LMPs over the peak hours, generally 16, on every calendar day
    2. those that average the LMPs over the off-peak hours, generally 8, on every calendar day

    This off peak power index uses the prices of both of these daily future contracts to construct the index that is 
    used in derivatives that reference off-peak power prices. The off-peak future is used directly on peak calendar 
    business days. On peak calendar holidays, the weighted average of the daily off-peak future price and daily peak 
    future price is used where the weights are the number of off-peak hours and peak hours respectively divided by 24.

    \ingroup indexes
*/
class OffPeakPowerIndex : public CommodityFuturesIndex {
public:
    //! Constructor
    OffPeakPowerIndex(const std::string& underlyingName,
        const QuantLib::Date& expiryDate,
        const QuantLib::ext::shared_ptr<CommodityFuturesIndex>& offPeakIndex,
        const QuantLib::ext::shared_ptr<CommodityFuturesIndex>& peakIndex,
        QuantLib::Real offPeakHours,
        const QuantLib::Calendar& peakCalendar,
        const Handle<QuantExt::PriceTermStructure>& priceCurve = Handle<QuantExt::PriceTermStructure>());

    //! \name Inspectors
    //@{
    const QuantLib::ext::shared_ptr<CommodityFuturesIndex>& offPeakIndex() const;
    const QuantLib::ext::shared_ptr<CommodityFuturesIndex>& peakIndex() const;
    QuantLib::Real offPeakHours() const;
    const QuantLib::Calendar& peakCalendar() const;
    //@}

    //! Implement the base clone.
    QuantLib::ext::shared_ptr<CommodityIndex> clone(const QuantLib::Date& expiryDate,
        const boost::optional<QuantLib::Handle<PriceTermStructure>>& ts = boost::none) const override;

protected:
    //! \name CommodityIndex interface
    //@{
    Real pastFixing(const Date& fixingDate) const override;
    //@}

private:
    QuantLib::ext::shared_ptr<CommodityFuturesIndex> offPeakIndex_;
    QuantLib::ext::shared_ptr<CommodityFuturesIndex> peakIndex_;
    QuantLib::Real offPeakHours_;
    QuantLib::Calendar peakCalendar_;

};

}

#endif
