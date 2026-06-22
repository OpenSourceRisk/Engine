/*
 Copyright (C) 2026 AcadiaSoft Inc
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

/*! \file qle/termstructures/pricetermstructure.hpp
    \brief Term structure of prices
*/

#pragma once

#include <qle/termstructures/intradayshapetermstructure.hpp>
#include <qle/termstructures/pricetermstructure.hpp>

namespace QuantExt {

//! Commodity Price term structure
/*! A commodity price term structure which can handle intraday power
 by applying an optional intraday shape factors to an underlying daily price curve.
    \ingroup termstructures
*/
class IntradayPowerPriceTermStructure : public QuantExt::PriceTermStructure {
public:
    //! \name Constructors
    //@{
    IntradayPowerPriceTermStructure(const QuantLib::Handle<PriceTermStructure>& underlying,
                                    const QuantLib::ext::shared_ptr<IntradayShapeTermstructure>& shape = nullptr);
    //@}

    //! \name Prices
    //@{
    QuantLib::Real price(QuantLib::Time t, bool extrapolate = false) const override;
    QuantLib::Real price(const QuantLib::Date& d, bool extrapolate = false) const override;
    QuantLib::Real price(const QuantLib::Date& d, int deliveryStartTime, int deliveryEndTime, bool isDSTextraHour,
                         bool extrapolate = false) const;
    //@}

    //! \name Observer interface
    //@{
    void update() override;
    //@}
    QuantLib::Date maxDate() const override { return underlying_->maxDate(); }
    QuantLib::Time maxTime() const override { return underlying_->maxTime(); }
    //! The minimum time for which the curve can return values
    QuantLib::Time minTime() const override { return underlying_->minTime(); }

    //! The currency in which prices are expressed
    const QuantLib::Currency& currency() const override { return underlying_->currency(); }

    //! The pillar dates for the PriceTermStructure
    std::vector<QuantLib::Date> pillarDates() const override { return underlying_->pillarDates(); }

    const QuantLib::Handle<PriceTermStructure>& averageDayPriceCurve() const { return underlying_; }
    const QuantLib::ext::shared_ptr<IntradayShapeTermstructure>& intradayShape() const { return shape_; }

protected:
    //@{
    //! Price calculation
    QuantLib::Real priceImpl(QuantLib::Time) const override {
        QL_FAIL("priceImpl(Time) not implemented for IntradayPowerPriceTermStructure");
    }
    //@}

private:
    QuantLib::Handle<PriceTermStructure> underlying_;
    QuantLib::ext::shared_ptr<IntradayShapeTermstructure> shape_;
};

} // namespace QuantExt
