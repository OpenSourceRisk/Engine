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

/*! \file qle/pricingengines/analyticxassetlgmeqoptionengine.hpp
    \brief analytic cross-asset lgm eq option engine
    \ingroup engines
*/

#ifndef quantext_xassetlgm_eqoptionengine_hpp
#define quantext_xassetlgm_eqoptionengine_hpp

#include <ql/instruments/vanillaoption.hpp>
#include <qle/models/crossassetmodel.hpp>

namespace QuantExt {

//! Analytic cross-asset lgm equity option engine
/*!
        This class prices an equity option analytically using the dynamics of
        a CrossAssetModel. The formula is black-like, with the variance of the
        underlying equity being dependent upon the dynamics of related interest
        and FX rates within the CrossAssetModel universe.
        See the book "Modern Derivatives Pricing and Credit Exposure Analysis"
        by Lichters, Stamm and Gallagher.

        \ingroup engines
*/
class AnalyticXAssetLgmEquityOptionEngine : public VanillaOption::engine {
public:
    AnalyticXAssetLgmEquityOptionEngine(const boost::shared_ptr<CrossAssetModel>& model, const Size equityIdx,
                                        const Size ccyIdx);
    void calculate() const;

    /*! the actual option price calculation, exposed to public,
      since it is useful to directly use the core computation
      sometimes */
    Real value(const Time t0, const Time t, const boost::shared_ptr<StrikedTypePayoff> payoff,
               const Real domesticDiscount, const Real eqForward) const;

private:
    const boost::shared_ptr<CrossAssetModel> model_;
    const Size eqIdx_, ccyIdx_;
};

} // namespace QuantExt

#endif
