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

/*! \file capfloortermvolsurface.hpp
    \brief Cap/floor smile volatility surface
*/

#ifndef quantext_cap_floor_term_vol_surface_hpp
#define quantext_cap_floor_term_vol_surface_hpp

#include <ql/math/interpolations/interpolation2d.hpp>
#include <ql/patterns/lazyobject.hpp>
#include <ql/quote.hpp>
#include <ql/termstructures/volatility/capfloor/capfloortermvolatilitystructure.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <vector>

namespace QuantExt {
using namespace QuantLib;

//! Cap/floor term-volatility surface
/*! This is a base class and defines the interface of capfloor term
    surface which will be derived from this one.
*/
class CapFloorTermVolSurface : public LazyObject, public CapFloorTermVolatilityStructure {
public:   
    //! default constructor
    CapFloorTermVolSurface(QuantLib::BusinessDayConvention bdc,
            const QuantLib::DayCounter& dc = QuantLib::DayCounter(),
            std::vector<QuantLib::Period> optionTenors = {},
            std::vector<QuantLib::Rate> strikes = {})
        : CapFloorTermVolatilityStructure(bdc, dc), optionTenors_(optionTenors), strikes_(strikes) {};

    //! initialize with a fixed reference date
    CapFloorTermVolSurface(const QuantLib::Date& referenceDate,
            const QuantLib::Calendar& cal,
            BusinessDayConvention bdc,
            const DayCounter& dc = QuantLib::DayCounter(),
            std::vector<QuantLib::Period> optionTenors = {},
            std::vector<QuantLib::Rate> strikes = {})
        : CapFloorTermVolatilityStructure(referenceDate, cal, bdc, dc), optionTenors_(optionTenors), strikes_(strikes) {};

    //! calculate the reference date based on the global evaluation date
    CapFloorTermVolSurface(QuantLib::Natural settlementDays,
            const QuantLib::Calendar& cal,
            QuantLib::BusinessDayConvention bdc,
            const QuantLib::DayCounter& dc = QuantLib::DayCounter(),
            std::vector<QuantLib::Period> optionTenors = {},
            std::vector<QuantLib::Rate> strikes = {})
        : CapFloorTermVolatilityStructure(settlementDays, cal, bdc, dc), optionTenors_(optionTenors), strikes_(strikes) {};

    const std::vector<QuantLib::Period>& optionTenors() const { return optionTenors_; }
    const std::vector<QuantLib::Rate>& strikes() const { return strikes_; }

    //! \name LazyObject interface
    //@{
    void update() {
        CapFloorTermVolatilityStructure::update();
        LazyObject::update();
    }
    void performCalculations() const {};
    //@}

protected:
    std::vector<QuantLib::Period> optionTenors_;
    std::vector<QuantLib::Rate> strikes_;
};

//! Cap/floor smile volatility surface
/*! This class provides the volatility for a given cap/floor interpolating
    a volatility surface whose elements are the market term volatilities
    of a set of caps/floors with given length and given strike.

    This is a copy of the QL CapFloorTermVolSurface but gives the option to use
    BiLinear instead of BiCubic Spline interpolation.
    Default is BiCubic Spline for backwards compatibility with QuantLib
*/
class CapFloorTermVolSurfaceExact : public CapFloorTermVolSurface {
public:
    enum InterpolationMethod { BicubicSpline, Bilinear };

    //! floating reference date, floating market data
    CapFloorTermVolSurfaceExact(Natural settlementDays, const Calendar& calendar, BusinessDayConvention bdc,
                           const std::vector<Period>& optionTenors, const std::vector<Rate>& strikes,
                           const std::vector<std::vector<Handle<Quote> > >&, const DayCounter& dc = Actual365Fixed(),
                           InterpolationMethod interpolationMethod = BicubicSpline);
    //! fixed reference date, floating market data
    CapFloorTermVolSurfaceExact(const Date& settlementDate, const Calendar& calendar, BusinessDayConvention bdc,
                           const std::vector<Period>& optionTenors, const std::vector<Rate>& strikes,
                           const std::vector<std::vector<Handle<Quote> > >&, const DayCounter& dc = Actual365Fixed(),
                           InterpolationMethod interpolationMethod = BicubicSpline);
    //! fixed reference date, fixed market data
    CapFloorTermVolSurfaceExact(const Date& settlementDate, const Calendar& calendar, BusinessDayConvention bdc,
                           const std::vector<Period>& optionTenors, const std::vector<Rate>& strikes,
                           const Matrix& volatilities, const DayCounter& dc = Actual365Fixed(),
                           InterpolationMethod interpolationMethod = BicubicSpline);
    //! floating reference date, fixed market data
    CapFloorTermVolSurfaceExact(Natural settlementDays, const Calendar& calendar, BusinessDayConvention bdc,
                           const std::vector<Period>& optionTenors, const std::vector<Rate>& strikes,
                           const Matrix& volatilities, const DayCounter& dc = Actual365Fixed(),
                           InterpolationMethod interpolationMethod = BicubicSpline);
    //! \name TermStructure interface
    //@{
    Date maxDate() const;
    //@}
    //! \name VolatilityTermStructure interface
    //@{
    Real minStrike() const;
    Real maxStrike() const;
    //@}
    //! \name LazyObject interface
    //@{
    void update();
    void performCalculations() const;
    //@}
    //! \name some inspectors
    //@{
    const std::vector<Date>& optionDates() const;
    const std::vector<Time>& optionTimes() const;
    InterpolationMethod interpolationMethod() const;
    //@}
protected:
    Volatility volatilityImpl(Time t, Rate strike) const;

private:
    void checkInputs() const;
    void initializeOptionDatesAndTimes() const;
    void registerWithMarketData();
    void interpolate();

    Size nOptionTenors_;
    mutable std::vector<Date> optionDates_;
    mutable std::vector<Time> optionTimes_;
    Date evaluationDate_;

    Size nStrikes_;

    std::vector<std::vector<Handle<Quote> > > volHandles_;
    mutable Matrix vols_;

    // make it not mutable if possible
    InterpolationMethod interpolationMethod_;
    mutable Interpolation2D interpolation_;
};

//! In order to convert CapFloorTermVolSurface::InterpolationMethod to string
std::ostream& operator<<(std::ostream& out, CapFloorTermVolSurfaceExact::InterpolationMethod method);

// inline definitions

inline Date CapFloorTermVolSurfaceExact::maxDate() const {
    calculate();
    return optionDateFromTenor(optionTenors_.back());
}

inline Real CapFloorTermVolSurfaceExact::minStrike() const { return strikes_.front(); }

inline Real CapFloorTermVolSurfaceExact::maxStrike() const { return strikes_.back(); }

inline Volatility CapFloorTermVolSurfaceExact::volatilityImpl(Time t, Rate strike) const {
    calculate();
    return interpolation_(strike, t, true);
}

inline const std::vector<Date>& CapFloorTermVolSurfaceExact::optionDates() const {
    // what if quotes are not available?
    calculate();
    return optionDates_;
}

inline const std::vector<Time>& CapFloorTermVolSurfaceExact::optionTimes() const {
    // what if quotes are not available?
    calculate();
    return optionTimes_;
}

inline CapFloorTermVolSurfaceExact::InterpolationMethod CapFloorTermVolSurfaceExact::interpolationMethod() const {
    return interpolationMethod_;
}

} // namespace QuantExt

#endif
