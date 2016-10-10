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

/*! \file fxbspiecewiseconstantparametrization.hpp
    \brief piecewise constant model parametrization
    \ingroup models
*/

#ifndef quantext_piecewiseconstant_fxbs_parametrization_hpp
#define quantext_piecewiseconstant_fxbs_parametrization_hpp

#include <qle/models/fxbsparametrization.hpp>
#include <qle/models/piecewiseconstanthelper.hpp>

namespace QuantExt {
//! FX Black Scholes constant parametrization
/*! FX Black Scholes parametrization with piecewise
    constant volatility
    \ingroup models
*/
class FxBsPiecewiseConstantParametrization : public FxBsParametrization, private PiecewiseConstantHelper1 {
public:
    /*! The currency refers to the foreign currency, the spot
        is as of today (i.e. the discounted spot) */
    FxBsPiecewiseConstantParametrization(const Currency& currency, const Handle<Quote>& fxSpotToday, const Array& times,
                                         const Array& sigma);
    /*! The term structure is needed in addition because it
        it's day counter and reference date is needed to
        convert dates to times. It should be the term structure
        of the domestic IR component in the cross asset model,
        since this is defining the model's date-time conversion
        in more general terms. */
    FxBsPiecewiseConstantParametrization(const Currency& currency, const Handle<Quote>& fxSpotToday,
                                         const std::vector<Date>& dates, const Array& sigma,
                                         const Handle<YieldTermStructure>& domesticTermStructure);
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

inline Real FxBsPiecewiseConstantParametrization::direct(const Size, const Real x) const {
    return PiecewiseConstantHelper1::direct(x);
}

inline Real FxBsPiecewiseConstantParametrization::inverse(const Size, const Real y) const {
    return PiecewiseConstantHelper1::inverse(y);
}

inline Real FxBsPiecewiseConstantParametrization::variance(const Time t) const {
    return PiecewiseConstantHelper1::int_y_sqr(t);
}

inline Real FxBsPiecewiseConstantParametrization::sigma(const Time t) const { return PiecewiseConstantHelper1::y(t); }

inline const Array& FxBsPiecewiseConstantParametrization::parameterTimes(const Size i) const {
    QL_REQUIRE(i == 0, "parameter " << i << " does not exist, only have 0");
    return PiecewiseConstantHelper1::t_;
}

inline const boost::shared_ptr<Parameter> FxBsPiecewiseConstantParametrization::parameter(const Size i) const {
    QL_REQUIRE(i == 0, "parameter " << i << " does not exist, only have 0");
    return PiecewiseConstantHelper1::y_;
}

inline void FxBsPiecewiseConstantParametrization::update() const { PiecewiseConstantHelper1::update(); }

} // namespace QuantExt

#endif
