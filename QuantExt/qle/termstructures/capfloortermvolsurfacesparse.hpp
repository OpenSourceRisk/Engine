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

/*! \file capfloortermvolsurface.hpp
    \brief Cap/floor smile volatility surface
*/

#ifndef quantext_cap_floor_term_vol_surface_sparse_hpp
#define quantext_cap_floor_term_vol_surface_sparse_hpp

#include <qle/interpolators/optioninterpolator2d.hpp>
#include <ql/math/interpolations/bicubicsplineinterpolation.hpp>
#include <ql/math/interpolations/bilinearinterpolation.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/utilities/dataformatters.hpp>
#include <ql/patterns/lazyobject.hpp>
#include <ql/quote.hpp>
#include <qle/termstructures/capfloortermvolsurface.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <vector>

namespace QuantExt {

//! Cap/floor smile volatility surface sparse
/*! This class provides the volatility for a given cap/floor interpolating
    a volatility surface whose elements are the market term volatilities
    of a set of caps/floors.
*/
template <class InterpolatorStrike, class InterpolatorExpiry>
class CapFloorTermVolSurfaceSparse : public CapFloorTermVolSurface {
    
public:
    //! fixed reference date, fixed market data
    CapFloorTermVolSurfaceSparse(const QuantLib::Date& referenceDate, const QuantLib::Calendar& calendar, const QuantLib::BusinessDayConvention& bdc,
        const QuantLib::DayCounter& dc, const std::vector<QuantLib::Period>& tenors, const std::vector<QuantLib::Real>& strikes,
        const std::vector<QuantLib::Volatility>& volatilities, bool lowerStrikeConstExtrap = true,
        bool upperStrikeConstExtrap = true, bool timeFlatExtrapolation = false);

    //! floating reference date, fixed market data
    CapFloorTermVolSurfaceSparse(QuantLib::Natural settlementDays, const QuantLib::Calendar& calendar, const QuantLib::BusinessDayConvention& bdc,
        const QuantLib::DayCounter& dc, const std::vector<QuantLib::Period>& tenors, const std::vector<QuantLib::Real>& strikes,
        const std::vector<QuantLib::Volatility>& volatilities, bool lowerStrikeConstExtrap = true,
        bool upperStrikeConstExtrap = true, bool timeFlatExtrapolation = false);

    //! \name TermStructure interface
    //@{
    QuantLib::Date maxDate() const override;
    //@}
    //! \name VolatilityTermStructure interface
    //@{
    QuantLib::Real minStrike() const override;
    QuantLib::Real maxStrike() const override;
    //@}
    //! \name LazyObject interface
    //@{
    void performCalculations() const override;
    //@}

protected:
    QuantLib::Volatility volatilityImpl(Time t, Rate strike) const override;

private:
    void initialiseStrikesTenors();
    mutable QuantLib::ext::shared_ptr<OptionInterpolator2d<InterpolatorStrike, InterpolatorExpiry>> optionInterpolator_;
    std::vector<QuantLib::Period> allTenors_;
    std::vector<QuantLib::Real> allStrikes_;
    std::vector<QuantLib::Volatility> allVols_;
    bool lowerStrikeConstExtrap_;
    bool upperStrikeConstExtrap_;
};

// inline definitions

template <class IS, class IE>
CapFloorTermVolSurfaceSparse<IS, IE>::CapFloorTermVolSurfaceSparse(const QuantLib::Date& referenceDate, const QuantLib::Calendar& calendar,
    const QuantLib::BusinessDayConvention& bdc, const QuantLib::DayCounter& dc, const std::vector<QuantLib::Period>& tenors, 
    const std::vector<QuantLib::Real>& strikes, const std::vector<QuantLib::Volatility>& volatilities, bool lowerStrikeConstExtrap,
    bool upperStrikeConstExtrap, bool timeFlatExtrapolation)
    : CapFloorTermVolSurface(referenceDate, calendar, bdc, dc), allTenors_(tenors), allStrikes_(strikes), allVols_(volatilities),
    lowerStrikeConstExtrap_(lowerStrikeConstExtrap), upperStrikeConstExtrap_(upperStrikeConstExtrap) {

    initialiseStrikesTenors();
}

template <class IS, class IE>
CapFloorTermVolSurfaceSparse<IS, IE>::CapFloorTermVolSurfaceSparse(QuantLib::Natural settlementDays, const QuantLib::Calendar& calendar, 
    const QuantLib::BusinessDayConvention& bdc, const QuantLib::DayCounter& dc, const std::vector<QuantLib::Period>& tenors, 
    const std::vector<QuantLib::Real>& strikes, const std::vector<QuantLib::Volatility>& volatilities, bool lowerStrikeConstExtrap,
    bool upperStrikeConstExtrap, bool timeFlatExtrapolation)
    : CapFloorTermVolSurface(settlementDays, calendar, bdc, dc), allTenors_(tenors), allStrikes_(strikes), allVols_(volatilities),
    lowerStrikeConstExtrap_(lowerStrikeConstExtrap), upperStrikeConstExtrap_(upperStrikeConstExtrap) {

    initialiseStrikesTenors();
}

template <class IS, class IE>
void CapFloorTermVolSurfaceSparse<IS, IE>::initialiseStrikesTenors() {
    
    for (auto t : allTenors_) {
        // add to the unique list of option tenors
        auto fnd = find(optionTenors_.begin(), optionTenors_.end(), t);
        if (fnd == optionTenors_.end()) {
            optionTenors_.push_back(t);
        }
    }
    // sort the tenors
    std::sort(optionTenors_.begin(), optionTenors_.end());

    for (auto s : allStrikes_) {
        std::vector<Real>::iterator fnd =
            find_if(strikes_.begin(), strikes_.end(), CloseEnoughComparator(s));
        if (fnd == strikes_.end()) {
            strikes_.push_back(s);
        }
    }
    // sort the strikes
    std::sort(strikes_.begin(), strikes_.end());

    // create the option interpolator
    performCalculations();
}

template <class IS, class IE>
QuantLib::Date CapFloorTermVolSurfaceSparse<IS, IE>::maxDate() const {
    return optionDateFromTenor(optionTenors().back());
}

template <class IS, class IE>
QuantLib::Real CapFloorTermVolSurfaceSparse<IS, IE>::minStrike() const { return strikes().front(); }

template <class IS, class IE>
QuantLib::Real CapFloorTermVolSurfaceSparse<IS, IE>::maxStrike() const { return strikes().back(); }

template <class IS, class IE>
QuantLib::Volatility CapFloorTermVolSurfaceSparse<IS, IE>::volatilityImpl(Time t, Rate strike) const {
    return optionInterpolator_->getValue(t, strike);
}

template <class IS, class IE>
void CapFloorTermVolSurfaceSparse<IS, IE>::performCalculations() const {
    optionInterpolator_ = QuantLib::ext::make_shared<OptionInterpolator2d<IS, IE>>(referenceDate(), calendar(), businessDayConvention(), dayCounter(), 
        allTenors_, allStrikes_, allVols_, lowerStrikeConstExtrap_, upperStrikeConstExtrap_);
}

} // namespace QuantExt

#endif
