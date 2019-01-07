/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file qle/termstructures/yoyoptionletvolatilitysurface.hpp
    \brief YoY Inflation volatility surface - extends QuantLib YoYOptionletVolatilitySurface
           to include a volatility type and displacement
    \ingroup termstructures
*/

#ifndef quantext_yoy_optionlet_volatility_surface_hpp
#define quantext_yoy_optionlet_volatility_surface_hpp

#include <ql/termstructures/volatility/inflation/yoyinflationoptionletvolatilitystructure.hpp>
#include <ql/termstructures/volatility/volatilitytype.hpp>


namespace QuantExt {
using namespace QuantLib;

//! YoY Inflation volatility surface
/*! \ingroup termstructures */
class YoYOptionletVolatilitySurface : public QuantLib::TermStructure {
public:

    //! \name Constructor
    //! calculate the reference date based on the global evaluation date
    YoYOptionletVolatilitySurface(boost::shared_ptr<QuantLib::YoYOptionletVolatilitySurface> referenceVolSurface,
        VolatilityType volType = ShiftedLognormal, Real displacement = 0.0) :
        TermStructure(), referenceVolSurface_(referenceVolSurface), volType_(volType), displacement_(displacement) {}
    
    Volatility volatility(const Date& maturityDate,
        Rate strike,
        const Period &obsLag = Period(-1, Days),
        bool extrapolate = false) const;
    Volatility volatility(const Period& optionTenor,
        Rate strike,
        const Period &obsLag = Period(-1, Days),
        bool extrapolate = false) const;

    virtual Volatility totalVariance(const Date& exerciseDate,
        Rate strike,
        const Period &obsLag = Period(-1, Days),
        bool extrapolate = false) const;
    virtual Volatility totalVariance(const Period& optionTenor,
        Rate strike,
        const Period &obsLag = Period(-1, Days),
        bool extrapolate = false) const;

    virtual Period observationLag() const { return referenceVolSurface_->observationLag(); }
    virtual Frequency frequency() const { return referenceVolSurface_->frequency(); }
    virtual bool indexIsInterpolated() const { return referenceVolSurface_->indexIsInterpolated(); }
    virtual Date baseDate() const { return referenceVolSurface_->baseDate(); }
    virtual Time timeFromBase(const Date &date, const Period& obsLag = Period(-1, Days)) const {
        return referenceVolSurface_->timeFromBase(date, obsLag); }

    virtual Date maxDate() const { return referenceVolSurface_->maxDate(); }
    virtual Real minStrike() const { return referenceVolSurface_->minStrike(); }
    virtual Real maxStrike() const { return referenceVolSurface_->maxStrike(); }
    virtual VolatilityType volatilityType() const;
    virtual Real displacement() const;
    virtual Volatility baseLevel() const { return referenceVolSurface_->baseLevel(); }

    boost::shared_ptr<QuantLib::YoYOptionletVolatilitySurface> yoyVolSurface() const { return referenceVolSurface_; }

protected:
    boost::shared_ptr<QuantLib::YoYOptionletVolatilitySurface> referenceVolSurface_;
    VolatilityType volType_;
    Real displacement_;
    Rate minStrike_, maxStrike_;
};

inline Volatility YoYOptionletVolatilitySurface::volatility(const Date& maturityDate, Rate strike,
    const Period &obsLag, bool extrapolate) const {
    return referenceVolSurface_->volatility(maturityDate, strike, obsLag, extrapolate);
}

inline Volatility YoYOptionletVolatilitySurface::volatility(const Period& optionTenor,
    Rate strike, const Period &obsLag, bool extrapolate) const {
    return referenceVolSurface_->volatility(optionTenor, strike, obsLag, extrapolate);
}

inline Volatility YoYOptionletVolatilitySurface::totalVariance(const Date& exerciseDate,
    Rate strike, const Period &obsLag, bool extrapolate) const {
    return referenceVolSurface_->totalVariance(exerciseDate, strike, obsLag, extrapolate);
}

inline Volatility YoYOptionletVolatilitySurface::totalVariance(const Period& optionTenor,
    Rate strike, const Period &obsLag, bool extrapolate) const {
    return referenceVolSurface_->totalVariance(optionTenor, strike, obsLag, extrapolate);
}

inline VolatilityType YoYOptionletVolatilitySurface::volatilityType() const {
    return volType_;
}

inline Real YoYOptionletVolatilitySurface::displacement() const {
    return displacement_;
}

} // namespace QuantExt

#endif
