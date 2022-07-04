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
#include <ql/termstructures/volatility/inflation/cpivolatilitystructure.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Interpolated zero inflation volatility structure
/*!
  The surface provides interpolated CPI Black volatilities.
  Volatility data is passed in as a vector of vector of quote Handles.
  When performCalculations is called, current quote values are copied to a matrix and the interpolator is updated.
 */
template <class Interpolator2D>
class InterpolatedCPIVolatilitySurface : public QuantLib::CPIVolatilitySurface, public LazyObject {
public:
    InterpolatedCPIVolatilitySurface(const std::vector<Period>& optionTenors, const std::vector<Real>& strikes,
                                     const std::vector<std::vector<Handle<Quote> > > quotes,
                                     const boost::shared_ptr<QuantLib::ZeroInflationIndex>& index,
                                     const Natural settlementDays, const Calendar& cal, BusinessDayConvention bdc,
                                     const DayCounter& dc, const Period& observationLag,
                                     const Interpolator2D& interpolator2d = Interpolator2D());

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

private:
    // Same logic as in CPIVolatilitySurface::volatility(const Date& ....) to compute the relevant time
    // that is used for calling voaltilityImpl resp. the interpolator. It would be better to move this up
    // in the hierarchy to CPIVolatilitySurface as this is needed in both InterpolatedCPIVolatilitySurface
    // and StrippedCPIVolatilitySurface.
    QuantLib::Time inflationTime(const QuantLib::Date& date) const {
        QuantLib::Real t;
        if (indexIsInterpolated_)
            t = timeFromReference(date - observationLag_);
        else {
            std::pair<QuantLib::Date, QuantLib::Date> dd = inflationPeriod(date - observationLag_, frequency_);
            t = timeFromReference(dd.first);
        }
        return t;
    }

    mutable std::vector<QuantLib::Period> optionTenors_;
    mutable std::vector<QuantLib::Time> optionTimes_;
    mutable std::vector<QuantLib::Rate> strikes_;
    mutable std::vector<std::vector<Handle<Quote> > > quotes_;
    boost::shared_ptr<QuantLib::ZeroInflationIndex> index_;
    mutable Matrix volData_;
    mutable QuantLib::Interpolation2D vols_;
    Interpolator2D interpolator2d_;
};

template <class Interpolator2D>
InterpolatedCPIVolatilitySurface<Interpolator2D>::InterpolatedCPIVolatilitySurface(
    const std::vector<Period>& optionTenors, const std::vector<Real>& strikes,
    const std::vector<std::vector<Handle<Quote> > > quotes,
    const boost::shared_ptr<QuantLib::ZeroInflationIndex>& index, const Natural settlementDays, const Calendar& cal,
    BusinessDayConvention bdc, const DayCounter& dc, const Period& observationLag, const Interpolator2D& interpolator2d)
    : CPIVolatilitySurface(settlementDays, cal, bdc, dc, observationLag, index->frequency(), index->interpolated()),
      optionTenors_(optionTenors), strikes_(strikes), quotes_(quotes), index_(index), interpolator2d_(interpolator2d) {
    for (Size i = 0; i < optionTenors_.size(); ++i) {
        QL_REQUIRE(quotes_[i].size() == strikes_.size(), "quotes row " << i << " length does not match strikes size");
        for (Size j = 0; j < strikes_.size(); ++j)
            registerWith(quotes_[i][j]);
    }
}

template <class Interpolator2D> void InterpolatedCPIVolatilitySurface<Interpolator2D>::performCalculations() const {
    volData_ = QuantLib::Matrix(strikes_.size(), optionTenors_.size(), QuantLib::Null<QuantLib::Real>());
    QL_REQUIRE(quotes_.size() == optionTenors_.size(), "quotes rows does not match option tenors size");
    optionTimes_.clear();
    for (Size i = 0; i < optionTenors_.size(); ++i) {
        QuantLib::Date d = optionDateFromTenor(optionTenors_[i]);
        optionTimes_.push_back(inflationTime(d));
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

} // namespace QuantExt

#endif
