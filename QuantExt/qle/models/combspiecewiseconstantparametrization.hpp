/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

/*! \file combspiecewiseconstantparametrization.hpp
    \brief piecewise constant model parametrization
    \ingroup models
*/

#ifndef quantext_piecewiseconstant_combs_parametrization_hpp
#define quantext_piecewiseconstant_combs_parametrization_hpp

#include <qle/models/combsparametrization.hpp>
#include <qle/models/piecewiseconstanthelper.hpp>

namespace QuantExt {
//! COM Black Scholes constant parametrization
/*! COM Black Scholes parametrization with piecewise
    constant volatility
    \ingroup models
*/
class ComBsPiecewiseConstantParametrization : public ComBsParametrization, private PiecewiseConstantHelper1 {
public:
    /*! The currency refers to the commodity currency, the spots
        are as of today (i.e. the discounted spot) */
    ComBsPiecewiseConstantParametrization(const Currency& currency, const std::string& name,
                                         const Handle<CommodityIndex>& index, const Handle<Quote>& fxSpotToday,
                                         const Array& times, const Array& sigma);
    Real variance(const Time t) const override;
    Real sigma(const Time t) const override;
    const Array& parameterTimes(const Size) const override;
    const boost::shared_ptr<Parameter> parameter(const Size) const override;
    void update() const override;

protected:
    Real direct(const Size i, const Real x) const override;
    Real inverse(const Size i, const Real y) const override;

private:
    void initialize(const Array& sigma);
};

// inline

inline Real ComBsPiecewiseConstantParametrization::direct(const Size, const Real x) const {
    return PiecewiseConstantHelper1::direct(x);
}

inline Real ComBsPiecewiseConstantParametrization::inverse(const Size, const Real y) const {
    return PiecewiseConstantHelper1::inverse(y);
}

inline Real ComBsPiecewiseConstantParametrization::variance(const Time t) const {
    return PiecewiseConstantHelper1::int_y_sqr(t);
}

inline Real ComBsPiecewiseConstantParametrization::sigma(const Time t) const { return PiecewiseConstantHelper1::y(t); }

inline const Array& ComBsPiecewiseConstantParametrization::parameterTimes(const Size i) const {
    QL_REQUIRE(i == 0, "parameter " << i << " does not exist, only have 0");
    return PiecewiseConstantHelper1::t_;
}

inline const boost::shared_ptr<Parameter> ComBsPiecewiseConstantParametrization::parameter(const Size i) const {
    QL_REQUIRE(i == 0, "parameter " << i << " does not exist, only have 0");
    return PiecewiseConstantHelper1::y_;
}

inline void ComBsPiecewiseConstantParametrization::update() const { PiecewiseConstantHelper1::update(); }

} // namespace QuantExt

#endif
