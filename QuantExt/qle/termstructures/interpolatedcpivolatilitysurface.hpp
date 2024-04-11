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

/*! \file qle/termstructures/interpolatedcpivolatilitysurface.hpp
    \brief zero inflation volatility structure interpolated on a expiry/strike matrix of quotes
 */

#ifndef quantext_interpolated_cpi_volatility_structure_hpp
#define quantext_interpolated_cpi_volatility_structure_hpp

#include <ql/indexes/inflationindex.hpp>
#include <ql/math/interpolations/interpolation2d.hpp>
#include <ql/math/matrix.hpp>
#include <ql/quote.hpp>
#include <ql/termstructures/volatility/inflation/constantcpivolatility.hpp>
#include <qle/termstructures/inflation/cpivolatilitystructure.hpp>
#include <qle/utilities/inflation.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Interpolated zero inflation volatility structure
/*!
  The surface provides interpolated CPI Black volatilities.
  Volatility data is passed in as a vector of vector of quote Handles.
  When performCalculations is called, current quote values are copied to a matrix and the interpolator is updated.
 */
template <class Interpolator2D>
class InterpolatedCPIVolatilitySurface : public QuantExt::CPIVolatilitySurface, public LazyObject {
public:
    InterpolatedCPIVolatilitySurface(const std::vector<Period>& optionTenors, const std::vector<Real>& strikes,
                                     const std::vector<std::vector<Handle<Quote>>> quotes,
                                     const QuantLib::ext::shared_ptr<QuantLib::ZeroInflationIndex>& index,
                                     const bool quotedInstrumentsAreInterpolated,
                                     const Natural settlementDays, const Calendar& cal, BusinessDayConvention bdc,
                                     const DayCounter& dc, const Period& observationLag,
                                     const Date& capFloorStartDate = Date(),
                                     const Interpolator2D& interpolator2d = Interpolator2D(),
                                     const QuantLib::VolatilityType volType = QuantLib::ShiftedLognormal,
                                     const double displacement = 0.0);


    QL_DEPRECATED
    InterpolatedCPIVolatilitySurface(const std::vector<Period>& optionTenors, const std::vector<Real>& strikes,
                                     const std::vector<std::vector<Handle<Quote>>> quotes,
                                     const QuantLib::ext::shared_ptr<QuantLib::ZeroInflationIndex>& index,
                                     const Natural settlementDays, const Calendar& cal, BusinessDayConvention bdc,
                                     const DayCounter& dc, const Period& observationLag,
                                     const Date& capFloorStartDate = Date(),
                                     const Interpolator2D& interpolator2d = Interpolator2D(),
                                     const QuantLib::VolatilityType volType = QuantLib::ShiftedLognormal,
                                     const double displacement = 0.0);

    //! \name LazyObject interface
    //@{
    void performCalculations() const override;
    void update() override {
        LazyObject::update();
        CPIVolatilitySurface::update();
    }
    //@}

    //! \name Limits
    //@{
    //! the minimum strike for which the term structure can return vols
    QuantLib::Real minStrike() const override;
    //! the maximum strike for which the term structure can return vols
    QuantLib::Real maxStrike() const override;
    //! maximum date for which the term structure can return vols
    QuantLib::Date maxDate() const override;
    //@}

    //! \name Inspectors
    //@{
    const std::vector<QuantLib::Real>& strikes() { return strikes_; }
    const std::vector<QuantLib::Period>& optionTenors() { return optionTenors_; }
    const std::vector<std::vector<Handle<Quote> > >& quotes() { return quotes_; }
    const QuantLib::Matrix& volData() { return volData_; }
    //@}

    QuantLib::Volatility volatilityImpl(QuantLib::Time length, QuantLib::Rate strike) const override;

    QuantLib::Real atmStrike(const QuantLib::Date& maturity,
                             const QuantLib::Period& obsLag = QuantLib::Period(-1, QuantLib::Days)) const override;

private:
    
    mutable std::vector<QuantLib::Period> optionTenors_;
    mutable std::vector<QuantLib::Time> optionTimes_;
    mutable std::vector<QuantLib::Rate> strikes_;
    mutable std::vector<std::vector<Handle<Quote> > > quotes_;
    QuantLib::ext::shared_ptr<QuantLib::ZeroInflationIndex> index_;
    mutable Matrix volData_;
    mutable QuantLib::Interpolation2D vols_;
    Interpolator2D interpolator2d_;
    
};

template <class Interpolator2D>
InterpolatedCPIVolatilitySurface<Interpolator2D>::InterpolatedCPIVolatilitySurface(
    const std::vector<Period>& optionTenors, const std::vector<Real>& strikes,
    const std::vector<std::vector<Handle<Quote>>> quotes, const QuantLib::ext::shared_ptr<QuantLib::ZeroInflationIndex>& index,
    const bool quotedInstrumentsOberserveInterpolated, const Natural settlementDays, const Calendar& cal, BusinessDayConvention bdc, const DayCounter& dc,
    const Period& observationLag, const Date& capFloorStartDate, const Interpolator2D& interpolator2d,
    const QuantLib::VolatilityType volType, const double displacement)
    : CPIVolatilitySurface(settlementDays, cal, bdc, dc, observationLag, index->frequency(),
                           quotedInstrumentsOberserveInterpolated,
                           capFloorStartDate, volType, displacement),
      optionTenors_(optionTenors), strikes_(strikes), quotes_(quotes), index_(index), interpolator2d_(interpolator2d) {
    for (Size i = 0; i < optionTenors_.size(); ++i) {
        QL_REQUIRE(quotes_[i].size() == strikes_.size(), "quotes row " << i << " length does not match strikes size");
        for (Size j = 0; j < strikes_.size(); ++j)
            registerWith(quotes_[i][j]);
    }
}


QL_DEPRECATED_DISABLE_WARNING
template <class Interpolator2D>
InterpolatedCPIVolatilitySurface<Interpolator2D>::InterpolatedCPIVolatilitySurface(
    const std::vector<Period>& optionTenors, const std::vector<Real>& strikes,
    const std::vector<std::vector<Handle<Quote>>> quotes, const QuantLib::ext::shared_ptr<QuantLib::ZeroInflationIndex>& index,
    const Natural settlementDays, const Calendar& cal, BusinessDayConvention bdc, const DayCounter& dc,
    const Period& observationLag, const Date& capFloorStartDate, const Interpolator2D& interpolator2d,
    const QuantLib::VolatilityType volType, const double displacement)
    : CPIVolatilitySurface(settlementDays, cal, bdc, dc, observationLag, index->frequency(), index->interpolated(),
                           capFloorStartDate, volType, displacement),
      optionTenors_(optionTenors), strikes_(strikes), quotes_(quotes), index_(index), interpolator2d_(interpolator2d) {
    for (Size i = 0; i < optionTenors_.size(); ++i) {
        QL_REQUIRE(quotes_[i].size() == strikes_.size(), "quotes row " << i << " length does not match strikes size");
        for (Size j = 0; j < strikes_.size(); ++j)
            registerWith(quotes_[i][j]);
    }
}
QL_DEPRECATED_ENABLE_WARNING   

template <class Interpolator2D> void InterpolatedCPIVolatilitySurface<Interpolator2D>::performCalculations() const {
    volData_ = QuantLib::Matrix(strikes_.size(), optionTenors_.size(), QuantLib::Null<QuantLib::Real>());
    QL_REQUIRE(quotes_.size() == optionTenors_.size(), "quotes rows does not match option tenors size");
    optionTimes_.clear();
    for (Size i = 0; i < optionTenors_.size(); ++i) {
        QuantLib::Date d = optionDateFromTenor(optionTenors_[i]);
        // Save the vols at their fixing times and not maturity
        optionTimes_.push_back(fixingTime(d));
        for (QuantLib::Size j = 0; j < strikes_.size(); j++)
            volData_[j][i] = quotes_[i][j]->value();
    }

    vols_ = interpolator2d_.interpolate(optionTimes_.begin(), optionTimes_.end(), strikes_.begin(), strikes_.end(),
                                        volData_);
    // FIXME ?
    vols_.enableExtrapolation();
    vols_.update();
}

template <class Interpolator2D> QuantLib::Real InterpolatedCPIVolatilitySurface<Interpolator2D>::minStrike() const {
    return strikes_.front() - QL_EPSILON;
}

template <class Interpolator2D> QuantLib::Real InterpolatedCPIVolatilitySurface<Interpolator2D>::maxStrike() const {
    return strikes_.back() + QL_EPSILON;
}

template <class Interpolator2D> QuantLib::Date InterpolatedCPIVolatilitySurface<Interpolator2D>::maxDate() const {
    QuantLib::Date today = QuantLib::Settings::instance().evaluationDate();
    return today + optionTenors_.back();
}

template <class Interpolator2D>
QuantLib::Volatility InterpolatedCPIVolatilitySurface<Interpolator2D>::volatilityImpl(QuantLib::Time length,
                                                                                      QuantLib::Rate strike) const {
    calculate();
    return vols_(length, strike);
}

template <class Interpolator2D>
QuantLib::Real InterpolatedCPIVolatilitySurface<Interpolator2D>::atmStrike(const QuantLib::Date& maturity,
                                                                           const QuantLib::Period& obsLag) const {
    QuantLib::Period lag = obsLag == -1 * QuantLib::Days ? observationLag() : obsLag;
    QuantLib::Date fixingDate = ZeroInflation::fixingDate(maturity, lag, frequency(), indexIsInterpolated());
    double forwardCPI = ZeroInflation::cpiFixing(index_, maturity, lag, indexIsInterpolated());
    double baseCPI = ZeroInflation::cpiFixing(
        index_, capFloorStartDate(), observationLag(), indexIsInterpolated());
    double atm = forwardCPI / baseCPI;
    double ttm =
        QuantLib::inflationYearFraction(frequency(), indexIsInterpolated(), dayCounter(), baseDate(), fixingDate);
    return std::pow(atm, 1.0 / ttm) - 1.0;
}

} // namespace QuantExt

#endif
