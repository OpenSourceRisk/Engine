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

/*! \file blackmonotonevarvoltermstructure.hpp
    \brief Black volatility surface that monotonises the variance in an existing surface
    \ingroup termstructures
*/

#ifndef quantext_black_monotone_var_vol_termstructure_hpp
#define quantext_black_monotone_var_vol_termstructure_hpp

#include <ql/math/array.hpp>
#include <ql/math/interpolations/backwardflatinterpolation.hpp>
#include <ql/math/rounding.hpp>
#include <ql/termstructures/volatility/equityfx/blackvoltermstructure.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Black volatility surface that monotonises the variance in an existing surface
/*! This class is used when monotonic variance is required

            \ingroup termstructures
*/
class BlackMonotoneVarVolTermStructure : public BlackVolTermStructure {
public:
    //! Constructor takes a BlackVolTermStructure and an array of time points which required monotonic variance
    /*! This will work with both a floating and fixed reference date underlying surface,
        since we are reimplementing the reference date and update methods */
    BlackMonotoneVarVolTermStructure(const Handle<BlackVolTermStructure>& vol, const std::vector<Time>& timePoints)
        : BlackVolTermStructure(vol->businessDayConvention(), vol->dayCounter()), vol_(vol), timePoints_(timePoints) {
        registerWith(vol_);
    }

    //! return the underlying vol surface
    const Handle<BlackVolTermStructure>& underlyingVol() const { return vol_; }

    //! \name TermStructure interface
    //@{
    const Date& referenceDate() const override { return vol_->referenceDate(); }
    Date maxDate() const override { return vol_->maxDate(); }
    Natural settlementDays() const override { return vol_->settlementDays(); }
    Calendar calendar() const override { return vol_->calendar(); }
    //! \name Observer interface
    //@{
    void update() override {
        monoVars_.clear();
        notifyObservers();
    }
    //@}
    //! \name VolatilityTermStructure interface
    //@{
    Real minStrike() const override { return vol_->minStrike(); }
    Real maxStrike() const override { return vol_->maxStrike(); }
    //@}
    //! \name Visitability
    //@{
    virtual void accept(AcyclicVisitor&) override;
    //@}
protected:
    virtual Real blackVarianceImpl(Time t, Real strike) const override { return getMonotoneVar(t, strike); }
    virtual Volatility blackVolImpl(Time t, Real strike) const override { return std::sqrt(getMonotoneVar(t, strike) / t); }

    void setMonotoneVar(const Real& strike) const {
        QL_REQUIRE(timePoints_.size() > 0, "timePoints cannot be empty");
        std::vector<Real> vars(timePoints_.size());
        vars[0] = vol_->blackVariance(timePoints_[0], strike);
        for (Size i = 1; i < timePoints_.size(); i++) {
            Real var = vol_->blackVariance(timePoints_[i], strike);
            var = std::max(var, vars[i - 1]);
            vars[i] = var;
        }
        monoVars_[strike] = vars;
    }

    Real getMonotoneVar(const Time& t, const Real& strike) const {
        if (monoVars_.find(strike) == monoVars_.end())
            setMonotoneVar(strike);
        BackwardFlatInterpolation interpolation(timePoints_.begin(), timePoints_.end(), monoVars_[strike].begin());
        return interpolation(t);
    }

private:
    Handle<BlackVolTermStructure> vol_;
    std::vector<Time> timePoints_;

    class closeDouble {
    public:
        bool operator()(const double& left, const double& right) const {
            return left < right && !QuantLib::close_enough(left, right);
        }
    };

    mutable std::map<Real, std::vector<Real>, closeDouble> monoVars_;
};

// inline definitions
inline void BlackMonotoneVarVolTermStructure::accept(AcyclicVisitor& v) {
    Visitor<BlackMonotoneVarVolTermStructure>* v1 = dynamic_cast<Visitor<BlackMonotoneVarVolTermStructure>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        BlackMonotoneVarVolTermStructure::accept(v);
}
} // namespace QuantExt

#endif
