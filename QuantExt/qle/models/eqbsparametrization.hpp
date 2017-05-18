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

/*! \file eqbsparametrization.hpp
    \brief EQ Black Scholes parametrization
    \ingroup models
*/

#ifndef quantext_eqbs_parametrization_hpp
#define quantext_eqbs_parametrization_hpp

#include <ql/handle.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <qle/models/parametrization.hpp>

using namespace QuantLib;

namespace QuantExt {

//! EQ Black Scholes parametrizations
/*! Base class for EQ Black Scholes parametrizations
    \ingroup models
*/
class EqBsParametrization : public Parametrization {
public:
    /*! The currency refers to the equity currency,
        the equity and fx spots are as of today
        (i.e. the discounted spot) */
    EqBsParametrization(const Currency& eqCcy, const std::string& eqName, const Handle<Quote>& equitySpotToday,
                        const Handle<Quote>& fxSpotToday, const Handle<YieldTermStructure>& equityIrCurveToday,
                        const Handle<YieldTermStructure>& equityDivYieldCurveToday);
    /*! must satisfy variance(0) = 0.0, variance'(t) >= 0 */
    virtual Real variance(const Time t) const = 0;
    /*! is supposed to be positive */
    virtual Real sigma(const Time t) const;
    virtual Real stdDeviation(const Time t) const;
    const Handle<Quote> eqSpotToday() const;
    const Handle<Quote> fxSpotToday() const;
    const Handle<YieldTermStructure> equityIrCurveToday() const;
    const Handle<YieldTermStructure> equityDivYieldCurveToday() const;
    const std::string& eqName() const { return eqName_; }

private:
    const Handle<Quote> eqSpotToday_, fxSpotToday_;
    const Handle<YieldTermStructure> eqRateCurveToday_, eqDivYieldCurveToday_;
    std::string eqName_;
};

// inline

inline Real EqBsParametrization::sigma(const Time t) const {
    return std::sqrt((variance(tr(t)) - variance(tl(t))) / h_);
}

inline Real EqBsParametrization::stdDeviation(const Time t) const { return std::sqrt(variance(t)); }

inline const Handle<Quote> EqBsParametrization::eqSpotToday() const { return eqSpotToday_; }

inline const Handle<Quote> EqBsParametrization::fxSpotToday() const { return fxSpotToday_; }

inline const Handle<YieldTermStructure> EqBsParametrization::equityIrCurveToday() const { return eqRateCurveToday_; }

inline const Handle<YieldTermStructure> EqBsParametrization::equityDivYieldCurveToday() const {
    return eqDivYieldCurveToday_;
}

} // namespace QuantExt

#endif
