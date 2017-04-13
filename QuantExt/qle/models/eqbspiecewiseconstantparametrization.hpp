/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file eqbspiecewiseconstantparametrization.hpp
    \brief piecewise constant model parametrization
    \ingroup models
*/

#ifndef quantext_piecewiseconstant_eqbs_parametrization_hpp
#define quantext_piecewiseconstant_eqbs_parametrization_hpp

#include <qle/models/eqbsparametrization.hpp>
#include <qle/models/piecewiseconstanthelper.hpp>

namespace QuantExt {
//! EQ Black Scholes constant parametrization
/*! EQ Black Scholes parametrization with piecewise
    constant volatility
    \ingroup models
*/
class EqBsPiecewiseConstantParametrization : public EqBsParametrization, private PiecewiseConstantHelper1 {
public:
    /*! The currency refers to the equity currency, the spots
        are as of today (i.e. the discounted spot) */
    EqBsPiecewiseConstantParametrization(const Currency& currency, const std::string& eqName,
                                         const Handle<Quote>& eqSpotToday, const Handle<Quote>& fxSpotToday,
                                         const Array& times, const Array& sigma,
                                         const Handle<YieldTermStructure>& eqIrCurveToday,
                                         const Handle<YieldTermStructure>& eqDivYieldCurveToday);
    /*! The term structure is needed in addition because it
        it's day counter and reference date is needed to
        convert dates to times. It should be the term structure
        of the domestic IR component in the cross asset model,
        since this is defining the model's date-time conversion
        in more general terms. */
    EqBsPiecewiseConstantParametrization(const Currency& currency, const std::string& eqName,
                                         const Handle<Quote>& eqSpotToday, const Handle<Quote>& fxSpotToday,
                                         const std::vector<Date>& dates, const Array& sigma,
                                         const Handle<YieldTermStructure>& domesticTermStructure,
                                         const Handle<YieldTermStructure>& eqIrCurveToday,
                                         const Handle<YieldTermStructure>& eqDivYieldCurveToday);
    Real variance(const Time t) const;
    Real sigma(const Time t) const;
    const Array& parameterTimes(const Size) const;
    const boost::shared_ptr<Parameter> parameter(const Size) const;
    void update() const;

protected:
    Real direct(const Size i, const Real x) const;
    Real inverse(const Size i, const Real y) const;

private:
    void initialize(const Array& sigma);
};

// inline

inline Real EqBsPiecewiseConstantParametrization::direct(const Size, const Real x) const {
    return PiecewiseConstantHelper1::direct(x);
}

inline Real EqBsPiecewiseConstantParametrization::inverse(const Size, const Real y) const {
    return PiecewiseConstantHelper1::inverse(y);
}

inline Real EqBsPiecewiseConstantParametrization::variance(const Time t) const {
    return PiecewiseConstantHelper1::int_y_sqr(t);
}

inline Real EqBsPiecewiseConstantParametrization::sigma(const Time t) const { return PiecewiseConstantHelper1::y(t); }

inline const Array& EqBsPiecewiseConstantParametrization::parameterTimes(const Size i) const {
    QL_REQUIRE(i == 0, "parameter " << i << " does not exist, only have 0");
    return PiecewiseConstantHelper1::t_;
}

inline const boost::shared_ptr<Parameter> EqBsPiecewiseConstantParametrization::parameter(const Size i) const {
    QL_REQUIRE(i == 0, "parameter " << i << " does not exist, only have 0");
    return PiecewiseConstantHelper1::y_;
}

inline void EqBsPiecewiseConstantParametrization::update() const { PiecewiseConstantHelper1::update(); }

} // namespace QuantExt

#endif
