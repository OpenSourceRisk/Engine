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

/*! \file qle/models/representativeswaption.hpp
    \brief representative swaption matcher
    \ingroup models
*/

#pragma once

#include <qle/models/lgm.hpp>
#include <qle/models/lgmimpliedyieldtermstructure.hpp>

#include <ql/cashflows/floatingratecoupon.hpp>
#include <ql/indexes/swapindex.hpp>
#include <ql/instruments/swaption.hpp>

namespace QuantExt {

using namespace QuantLib;

/*! Given an exotic underlying find a standard swap matching the underlying using the representative swaption method in
   a LGM model.

   The swaption that is returned does not have a pricing engine attached, the underlying swap has a
   discounting swap engine attached (using the given discount curve) though and the ibor index of the underlying swap is
   using the forwarding curve from the given swap index.

   The LGM model uses to find the representative swaption
   - uses a constant volatility and reversion as specified in the ctor
   - if flatRate is null, uses the given dicount curve and forwarding curves from the underlying's ibor indices
   - if flatRate is not null, uses flat discount and forwarding curves using the rate level given by flatRate

   For the methodology, see Andersen, Piterbarg, Interest Rate Modelling, ch. 19.4.

   The underlying may only contain simple cahsflows, fixed coupons and standard ibor coupons (i.e. without cap/floor or
   in arrears fixings). */

class RepresentativeSwaptionMatcher {
public:
    enum class InclusionCriterion { AccrualStartGeqExercise, PayDateGtExercise };
    RepresentativeSwaptionMatcher(const std::vector<Leg>& underlying, const std::vector<bool>& isPayer,
                                  const QuantLib::ext::shared_ptr<SwapIndex>& standardSwapIndexBase,
                                  const bool useUnderlyingIborIndex, const Handle<YieldTermStructure>& discountCurve,
                                  const Real reversion, const Real volatility = 0.0050,
                                  const Real flatRate = Null<Real>());

    /*! find representative swaption for all specified underlying cashflows
        a null pointer is returned if there are no live cashflows found */
    QuantLib::ext::shared_ptr<Swaption>
    representativeSwaption(Date exerciseDate,
                           const InclusionCriterion criterion = InclusionCriterion::AccrualStartGeqExercise);

private:
    QuantLib::Date valueDate(const QuantLib::Date& fixingDate, const QuantLib::ext::shared_ptr<QuantLib::FloatingRateCoupon>& cpn) const;
    
    const std::vector<Leg> underlying_;
    const std::vector<bool> isPayer_;
    const QuantLib::ext::shared_ptr<SwapIndex> swapIndexBase_;
    const bool useUnderlyingIborIndex_;
    const Handle<YieldTermStructure> discountCurve_;
    const Real reversion_, volatility_, flatRate_;
    //
    QuantLib::ext::shared_ptr<LGM> model_;
    Leg modelLinkedUnderlying_;
    std::vector<bool> modelLinkedUnderlyingIsPayer_;

    std::map<std::string, QuantLib::ext::shared_ptr<LgmImpliedYtsFwdFwdCorrected>> modelForwardCurves_;
    QuantLib::ext::shared_ptr<LgmImpliedYtsFwdFwdCorrected> modelDiscountCurve_, modelSwapIndexForwardCurve_,
        modelSwapIndexDiscountCurve_;
    QuantLib::ext::shared_ptr<SwapIndex> swapIndexBaseFinal_, modelSwapIndexBase_;
};

} // namespace QuantExt
