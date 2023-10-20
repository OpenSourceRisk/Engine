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

#include <ql/math/interpolations/bicubicsplineinterpolation.hpp>
#include <ql/math/interpolations/bilinearinterpolation.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/utilities/dataformatters.hpp>
#include <qle/termstructures/capfloortermvolsurface.hpp>

using namespace QuantLib;
using std::ostream;

namespace QuantExt {

// floating reference date, floating market data
CapFloorTermVolSurfaceExact::CapFloorTermVolSurfaceExact(Natural settlementDays, const Calendar& calendar,
                                               BusinessDayConvention bdc, const std::vector<Period>& optionTenors,
                                               const std::vector<Rate>& strikes,
                                               const std::vector<std::vector<Handle<Quote> > >& vols,
                                               const DayCounter& dc, InterpolationMethod interpolationMethod)
    : CapFloorTermVolSurface(settlementDays, calendar, bdc, dc, optionTenors, strikes), nOptionTenors_(optionTenors.size()),
      optionDates_(nOptionTenors_), optionTimes_(nOptionTenors_), nStrikes_(strikes.size()), volHandles_(vols), 
      vols_(vols.size(), vols[0].size()), interpolationMethod_(interpolationMethod) {
    checkInputs();
    initializeOptionDatesAndTimes();
    for (Size i = 0; i < nOptionTenors_; ++i)
        QL_REQUIRE(volHandles_[i].size() == nStrikes_, io::ordinal(i + 1)
                                                           << " row of vol handles has size " << volHandles_[i].size()
                                                           << " instead of " << nStrikes_);
    registerWithMarketData();
    for (Size i = 0; i < vols_.rows(); ++i)
        for (Size j = 0; j < vols_.columns(); ++j)
            vols_[i][j] = volHandles_[i][j]->value();
    interpolate();
}

// fixed reference date, floating market data
CapFloorTermVolSurfaceExact::CapFloorTermVolSurfaceExact(const Date& settlementDate, const Calendar& calendar,
                                               BusinessDayConvention bdc, const std::vector<Period>& optionTenors,
                                               const std::vector<Rate>& strikes,
                                               const std::vector<std::vector<Handle<Quote> > >& vols,
                                               const DayCounter& dc, InterpolationMethod interpolationMethod)
    : CapFloorTermVolSurface(settlementDate, calendar, bdc, dc, optionTenors, strikes), nOptionTenors_(optionTenors.size()),
      optionDates_(nOptionTenors_), optionTimes_(nOptionTenors_), nStrikes_(strikes.size()), volHandles_(vols), 
      vols_(vols.size(), vols[0].size()), interpolationMethod_(interpolationMethod) {
    checkInputs();
    initializeOptionDatesAndTimes();
    for (Size i = 0; i < nOptionTenors_; ++i)
        QL_REQUIRE(volHandles_[i].size() == nStrikes_, io::ordinal(i + 1)
                                                           << " row of vol handles has size " << volHandles_[i].size()
                                                           << " instead of " << nStrikes_);
    registerWithMarketData();
    for (Size i = 0; i < vols_.rows(); ++i)
        for (Size j = 0; j < vols_.columns(); ++j)
            vols_[i][j] = volHandles_[i][j]->value();
    interpolate();
}

// fixed reference date, fixed market data
CapFloorTermVolSurfaceExact::CapFloorTermVolSurfaceExact(const Date& settlementDate, const Calendar& calendar,
                                               BusinessDayConvention bdc, const std::vector<Period>& optionTenors,
                                               const std::vector<Rate>& strikes, const Matrix& vols,
                                               const DayCounter& dc, InterpolationMethod interpolationMethod)
    : CapFloorTermVolSurface(settlementDate, calendar, bdc, dc, optionTenors, strikes), nOptionTenors_(optionTenors.size()),
      optionDates_(nOptionTenors_), optionTimes_(nOptionTenors_), nStrikes_(strikes.size()), volHandles_(vols.rows()), vols_(vols),
      interpolationMethod_(interpolationMethod) {
    checkInputs();
    initializeOptionDatesAndTimes();
    // fill dummy handles to allow generic handle-based computations later
    for (Size i = 0; i < nOptionTenors_; ++i) {
        volHandles_[i].resize(nStrikes_);
        for (Size j = 0; j < nStrikes_; ++j)
            volHandles_[i][j] = Handle<Quote>(ext::shared_ptr<Quote>(new SimpleQuote(vols_[i][j])));
    }
    interpolate();
}

// floating reference date, fixed market data
CapFloorTermVolSurfaceExact::CapFloorTermVolSurfaceExact(Natural settlementDays, const Calendar& calendar,
                                               BusinessDayConvention bdc, const std::vector<Period>& optionTenors,
                                               const std::vector<Rate>& strikes, const Matrix& vols,
                                               const DayCounter& dc, InterpolationMethod interpolationMethod)
    : CapFloorTermVolSurface(settlementDays, calendar, bdc, dc, optionTenors, strikes), nOptionTenors_(optionTenors.size()),
      optionDates_(nOptionTenors_), optionTimes_(nOptionTenors_),
      nStrikes_(strikes.size()), volHandles_(vols.rows()), vols_(vols),
      interpolationMethod_(interpolationMethod) {
    checkInputs();
    initializeOptionDatesAndTimes();
    // fill dummy handles to allow generic handle-based computations later
    for (Size i = 0; i < nOptionTenors_; ++i) {
        volHandles_[i].resize(nStrikes_);
        for (Size j = 0; j < nStrikes_; ++j)
            volHandles_[i][j] = Handle<Quote>(ext::shared_ptr<Quote>(new SimpleQuote(vols_[i][j])));
    }
    interpolate();
}

void CapFloorTermVolSurfaceExact::checkInputs() const {

    QL_REQUIRE(!optionTenors_.empty(), "empty option tenor vector");
    QL_REQUIRE(nOptionTenors_ == vols_.rows(), "mismatch between number of option tenors ("
                                                   << nOptionTenors_ << ") and number of volatility rows ("
                                                   << vols_.rows() << ")");
    QL_REQUIRE(optionTenors_[0] > 0 * Days, "negative first option tenor: " << optionTenors_[0]);
    for (Size i = 1; i < nOptionTenors_; ++i)
        QL_REQUIRE(optionTenors_[i] > optionTenors_[i - 1],
                   "non increasing option tenor: " << io::ordinal(i) << " is " << optionTenors_[i - 1] << ", "
                                                   << io::ordinal(i + 1) << " is " << optionTenors_[i]);

    QL_REQUIRE(nStrikes_ == vols_.columns(),
               "mismatch between strikes(" << strikes_.size() << ") and vol columns (" << vols_.columns() << ")");
    for (Size j = 1; j < nStrikes_; ++j)
        QL_REQUIRE(strikes_[j - 1] < strikes_[j],
                   "non increasing strikes: " << io::ordinal(j) << " is " << io::rate(strikes_[j - 1]) << ", "
                                              << io::ordinal(j + 1) << " is " << io::rate(strikes_[j]));
}

void CapFloorTermVolSurfaceExact::registerWithMarketData() {
    for (Size i = 0; i < nOptionTenors_; ++i)
        for (Size j = 0; j < nStrikes_; ++j)
            registerWith(volHandles_[i][j]);
}

void CapFloorTermVolSurfaceExact::interpolate() {
    if (interpolationMethod_ == BicubicSpline)
        interpolation_ =
            QuantLib::BicubicSpline(strikes_.begin(), strikes_.end(), optionTimes_.begin(), optionTimes_.end(), vols_);
    else if (interpolationMethod_ == Bilinear)
        interpolation_ =
            BilinearInterpolation(strikes_.begin(), strikes_.end(), optionTimes_.begin(), optionTimes_.end(), vols_);
    else {
        QL_FAIL("Invalid InterpolationMethod");
    }
}

void CapFloorTermVolSurfaceExact::update() {
    // recalculate dates if necessary...
    if (moving_) {
        Date d = Settings::instance().evaluationDate();
        if (evaluationDate_ != d) {
            evaluationDate_ = d;
            initializeOptionDatesAndTimes();
        }
    }
    CapFloorTermVolSurface::update();    
}

void CapFloorTermVolSurfaceExact::initializeOptionDatesAndTimes() const {
    for (Size i = 0; i < nOptionTenors_; ++i) {
        optionDates_[i] = optionDateFromTenor(optionTenors_[i]);
        optionTimes_[i] = timeFromReference(optionDates_[i]);
    }
}

void CapFloorTermVolSurfaceExact::performCalculations() const {
    // check if date recalculation must be called here

    for (Size i = 0; i < nOptionTenors_; ++i)
        for (Size j = 0; j < nStrikes_; ++j)
            vols_[i][j] = volHandles_[i][j]->value();

    interpolation_.update();
}

ostream& operator<<(ostream& out, CapFloorTermVolSurfaceExact::InterpolationMethod method) {
    switch (method) {
    case CapFloorTermVolSurfaceExact::BicubicSpline:
        return out << "BicubicSpline";
    case CapFloorTermVolSurfaceExact::Bilinear:
        return out << "Bilinear";
    default:
        QL_FAIL("Unknown CapFloorTermVolSurface::InterpolationMethod (" << Integer(method) << ")");
    }
}

} // namespace QuantExt
