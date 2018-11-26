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

/*! \file fxbsparametrization.hpp
    \brief FX Black Scholes parametrization
    \ingroup models
*/

#ifndef quantext_fxbs_parametrization_hpp
#define quantext_fxbs_parametrization_hpp

#include <ql/handle.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <qle/models/parametrization.hpp>

using namespace QuantLib;

namespace QuantExt {

//! FX Black Scholes parametrizations
/*! Base class for FX Black Scholes parametrizations
    \ingroup models
*/
class FxBsParametrization : public Parametrization {
public:
    /*! The currency refers to the foreign currency, the spot
        is as of today (i.e. the discounted spot) */
    FxBsParametrization(const Currency& foreignCurrency, const Handle<Quote>& fxSpotToday);
    /*! must satisfy variance(0) = 0.0, variance'(t) >= 0 */
    virtual Real variance(const Time t) const = 0;
    /*! is supposed to be positive */
    virtual Real sigma(const Time t) const;
    virtual Real stdDeviation(const Time t) const;
    const Handle<Quote> fxSpotToday() const;

private:
    const Handle<Quote> fxSpotToday_;
};

// inline

inline Real FxBsParametrization::sigma(const Time t) const {
    return std::sqrt((variance(tr(t)) - variance(tl(t))) / h_);
}

inline Real FxBsParametrization::stdDeviation(const Time t) const { return std::sqrt(variance(t)); }

inline const Handle<Quote> FxBsParametrization::fxSpotToday() const { return fxSpotToday_; }

} // namespace QuantExt

#endif
