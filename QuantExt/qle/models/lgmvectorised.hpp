/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
 All rights reserved.
*/

/*! \file models/lgmvectorised.hpp
    \brief vectorised lgm model calculations
    \ingroup models
*/

#pragma once


#include <qle/math/randomvariable.hpp>
#include <qle/models/irlgm1fparametrization.hpp>

#include <ql/indexes/interestrateindex.hpp>

namespace QuantExt {

using namespace QuantLib;
using namespace QuantExt;

class LgmVectorised {
public:
    LgmVectorised(const boost::shared_ptr<IrLgm1fParametrization>& p) : p_(p) {}

    RandomVariable numeraire(const Time t, const RandomVariable& x,
                             const Handle<YieldTermStructure> discountCurve = Handle<YieldTermStructure>()) const;

    RandomVariable discountBond(const Time t, const Time T, const RandomVariable& x,
                                Handle<YieldTermStructure> discountCurve = Handle<YieldTermStructure>()) const;

    RandomVariable
    reducedDiscountBond(const Time t, const Time T, const RandomVariable& x,
                        const Handle<YieldTermStructure> discountCurve = Handle<YieldTermStructure>()) const;

    RandomVariable fixing(const boost::shared_ptr<InterestRateIndex>& index, const Date& fixingDate, const Time t,
                          const RandomVariable& x) const;

private:
    const boost::shared_ptr<IrLgm1fParametrization> p_;
};

} // namespace QuantExt
