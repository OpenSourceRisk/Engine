/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2015 Quaternion Risk Management

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

/*! \file analyticlgmswaptionengine.hpp
    \brief analytic engine for european swaptions in the LGM model
*/

#ifndef quantext_analytic_lgm_swaption_engine_hpp
#define quantext_analytic_lgm_swaption_engine_hpp

#include <ql/instruments/swaption.hpp>
#include <ql/pricingengines/genericmodelengine.hpp>
#include <qle/models/xassetmodel.hpp>

namespace QuantExt {

//! Analytic LGM swaption engine for european exercise
/*! \ingroup swaptionengines

    All fixed coupons with start date greater or equal to the respective
    option expiry are considered to be
    part of the exercise into right.

    Reference: Methodology for callable swaps and bermudan "exercise into"
    swaptions, http://www.researchgate.net/publication/273720052,
    section 3.3

    \warning Cash settled swaptions are not supported

    The basis between the given discounting curve (or - if not given - the
    model curve) and the forwarding curve attached to the underlying swap's
    ibor index is taken into account by a static correction spread for
    the underlying's fixed leg.

    Note: Eventually we might provide this engine based on an own LGM model
    class and an adaptor to the XAsset model, but for the time being it
    seems easier to just refer to the XAsset model and select the appropriate
    LGM component via the currency index.
*/

class AnalyticLgmSwaptionEngine
    : public GenericModelEngine<XAssetModel, Swaption::arguments,
                                Swaption::results> {

    AnalyticLgmSwaptionEngine(const boost::shared_ptr<XAssetModel> &model,
                              const Size ccy,
                              const Handle<YieldTermStructure> &discountCurve =
                                  Handle<YieldTermStructure>());

    void calculate() const;

  private:
    Real yStarHelper(const Real y) const;
    const Size ccy_;
    const Handle<YieldTermStructure> discountCurve_;
    mutable Real H0_, D0_, zetaex_;
    mutable std::vector<Real> S_, Hj_, Dj_;
    mutable Size j1_, k1_;
};

} // namespace QuantExt

#endif
