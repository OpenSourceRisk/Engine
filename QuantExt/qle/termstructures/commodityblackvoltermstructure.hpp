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

/*! \file qle/termstructures/commodityblackvoltermstructure.hpp
    \brief Base Class for Commodity Volatility Termstructures
    \ingroup termstructures
*/

#pragma once

#include <ql/termstructures/voltermstructure.hpp>

namespace QuantExt {
class CommodityFutureBlackVolatilityTermStructure : public QuantLib::VolatilityTermStructure {
public:
    /*! \warning term structures initialized by means of this
                constructor must manage their own reference date
                by overriding the referenceDate() method.
    */
    CommodityFutureBlackVolatilityTermStructure(BusinessDayConvention bdc = Following,
                                                const DayCounter& dc = DayCounter())
        : VolatilityTermStructure(bdc, dc) {}

    //! initialize with a fixed reference date
    CommodityFutureBlackVolatilityTermStructure(const Date& referenceDate, const Calendar& cal = Calendar(),
                                                BusinessDayConvention bdc = Following,
                                                const DayCounter& dc = DayCounter())
        : VolatilityTermStructure(refDate, cal, bdc, dc) {}
    //! calculate the reference date based on the global evaluation date
    CommodityFutureBlackVolatilityTermStructure(Natural settlementDays, const Calendar& cal,
                                                BusinessDayConvention bdc = Following,
                                                const DayCounter& dc = DayCounter())
        : VolatilityTermStructure(settlDays, cal, bdc, dc) {}
    //@}
    //! \name Visitability
    //@{
    void accept(AcyclicVisitor&) override;

    //! spot volatility
    Volatility blackVol(const Date& contractExpiry, Real strike,
                        bool extrapolate = false) const;
    //! spot volatility
    Volatility blackVol(Time contractExpiry, Real strike, bool extrapolate = false) const;
    //! spot variance
    Real blackVariance(const Date& contractExpiry, Real strike, const Date& optionExpiry,
                       bool extrapolate = false) const;
    //! spot variance
    Real blackVariance(Time contractExpiry, Real strike, Time optionExpiry, bool extrapolate = false) const;

protected:
    /*! \name Calculations

        These methods must be implemented in derived classes to perform
        the actual volatility calculations. When they are called,
        range check has already been performed; therefore, they must
        assume that extrapolation is required.
    */
    //@{
    //! Black variance calculation
    virtual Real blackVarianceImpl(Time contractExpiry, Real strike, Time optionExpiry) const;
    //! Black volatility calculation
    virtual Volatility blackVolImpl(Time contractExpiry, Real strike) const = 0;
    //@}
};

// inline definitions

inline Volatility CommodityFutureBlackVolatilityTermStructure::blackVol(const Date& contractExpiry, Real strike,
                                                                        bool extrapolate) const {
    checkRange(d, extrapolate);
    checkStrike(strike, extrapolate);
    Time t = timeFromReference(d);
    return blackVolImpl(t, strike);
}

inline Volatility CommodityFutureBlackVolatilityTermStructure::blackVol(Time contractExpiry, Real strike,
                                                                        bool extrapolate) const {
    checkRange(t, extrapolate);
    checkStrike(strike, extrapolate);
    return blackVolImpl(t, strike);
}

inline Real CommodityFutureBlackVolatilityTermStructure::blackVariance(const Date& contractExpiry, Real strike,
                                                                       const Date& optionExpiry,
                                                                       bool extrapolate) const {
    checkRange(d, contractExpiry);
    checkStrike(strike, extrapolate);
    Time contractExpiryTime = timeFromReference(contractExpiry);
    Time optionExpiryTime = timeFromReference(optionExpiry);
    return blackVarianceImpl(contractExpiryTime, strike, optionExpiryTime);
}

inline Real CommodityFutureBlackVolatilityTermStructure::blackVariance(Time contractExpiry, Real strike,
                                                                       Time optionExpiry, bool extrapolate) const {
    checkRange(t, extrapolate);
    checkStrike(strike, extrapolate);
    return blackVarianceImpl(contractExpiry, strike, optionExpiry);
}

inline void CommodityFutureBlackVolatilityTermStructure::accept(AcyclicVisitor& v) {
    auto* v1 = dynamic_cast<Visitor<BlackVolTermStructure>*>(&v);
    if (v1 != nullptr)
        v1->visit(*this);
    else
        QL_FAIL("not a Black-volatility term structure visitor");
}

inline Real CommodityFutureBlackVolatilityTermStructure::blackVarianceImpl(Time contractExpiry, Real strike,
                                                                           Time optionExpiry) const {
    Volatility vol = blackVolImpl(contractExpiry, strike);
    return vol * vol * optionExpiry;
}

}


