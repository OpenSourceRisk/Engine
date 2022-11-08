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

/*! \file combsparametrization.hpp
    \brief Commodity Black Scholes parametrization
    \ingroup models
*/

#ifndef quantext_combs_parametrization_hpp
#define quantext_combs_parametrization_hpp

#include <ql/handle.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <qle/indexes/commodityindex.hpp>
#include <qle/models/parametrization.hpp>

namespace QuantExt {
using namespace QuantLib;

//! COM Black Scholes parametrizations
/*! Base class for COM Black Scholes parametrizations
    \ingroup models
*/
class ComBsParametrization : public Parametrization {
public:
    /*! The currency refers to the commodity currency,
        the fx spots is as of today
        (i.e. the discounted spot) */
    ComBsParametrization(const Currency& comCcy, const std::string& comName,
                         const Handle<CommodityIndex>& comIndex,
                         const Handle<Quote>& fxSpotToday);
    /*! must satisfy variance(0) = 0.0, variance'(t) >= 0 */
    virtual Real variance(const Time t) const = 0;
    /*! is supposed to be positive */
    virtual Real sigma(const Time t) const;
    virtual Real stdDeviation(const Time t) const;
    const Handle<Quote> fxSpotToday() const;

    Size numberOfParameters() const override { return 1; }

private:
    const Handle<CommodityIndex> comIndex_;
    const Handle<Quote> fxSpotToday_;
    std::string comName_;
};

// inline

inline Real ComBsParametrization::sigma(const Time t) const {
    return std::sqrt((variance(tr(t)) - variance(tl(t))) / h_);
}

inline Real ComBsParametrization::stdDeviation(const Time t) const { return std::sqrt(variance(t)); }

inline const Handle<Quote> ComBsParametrization::fxSpotToday() const { return fxSpotToday_; }

} // namespace QuantExt

#endif
