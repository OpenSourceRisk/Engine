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

/*! \file models/lgmvectorised.hpp
    \brief vectorised lgm model calculations
    \ingroup models
*/

#pragma once

#include <qle/math/randomvariable.hpp>
#include <qle/models/irlgm1fparametrization.hpp>

#include <ql/indexes/bmaindex.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/option.hpp>

namespace QuantExt {

using namespace QuantLib;

class LgmVectorised {
public:
    LgmVectorised() = default;
    LgmVectorised(const QuantLib::ext::shared_ptr<IrLgm1fParametrization>& p) : p_(p) {}

    QuantLib::ext::shared_ptr<IrLgm1fParametrization> parametrization() const { return p_; }

    RandomVariable numeraire(const Time t, const RandomVariable& x,
                             const Handle<YieldTermStructure>& discountCurve = Handle<YieldTermStructure>()) const;

    RandomVariable discountBond(const Time t, const Time T, const RandomVariable& x,
                                const Handle<YieldTermStructure>& discountCurve = Handle<YieldTermStructure>()) const;

    RandomVariable
    reducedDiscountBond(const Time t, const Time T, const RandomVariable& x,
                        const Handle<YieldTermStructure>& discountCurve = Handle<YieldTermStructure>()) const;

    RandomVariable discountBondOption(Option::Type type, const Real K, const Time t, const Time S, const Time T,
                                      const RandomVariable& x, const Handle<YieldTermStructure>& discountCurve) const;

    /* Handles IborIndex and SwapIndex. Requires observation time t <= fixingDate */
    RandomVariable fixing(const QuantLib::ext::shared_ptr<InterestRateIndex>& index, const Date& fixingDate, const Time t,
                          const RandomVariable& x) const;

    /* Exact if no cap/floors are present and t <= first value date.
       Approximations are applied for t > first value date or when cap / floors are present. */
    RandomVariable compoundedOnRate(const QuantLib::ext::shared_ptr<OvernightIndex>& index,
                                    const std::vector<Date>& fixingDates, const std::vector<Date>& valueDates,
                                    const std::vector<Real>& dt, const Natural rateCutoff, const bool includeSpread,
                                    const Real spread, const Real gearing, const Period lookback, Real cap, Real floor,
                                    const bool localCapFloor, const bool nakedOption, const Time t,
                                    const RandomVariable& x) const;

    /* Exact if no cap/floors are present and t <= first value date.
       Approximations are applied for t > first value date or when cap / floors are present. */
    RandomVariable averagedOnRate(const QuantLib::ext::shared_ptr<OvernightIndex>& index, const std::vector<Date>& fixingDates,
                                  const std::vector<Date>& valueDates, const std::vector<Real>& dt,
                                  const Natural rateCutoff, const bool includeSpread, const Real spread,
                                  const Real gearing, const Period lookback, Real cap, Real floor,
                                  const bool localCapFloor, const bool nakedOption, const Time t,
                                  const RandomVariable& x) const;

    /* Exact if no cap/floors are present and t <= first value date.
       Approximations are applied for t > first value date or when cap / floors are present. */
    RandomVariable averagedBmaRate(const QuantLib::ext::shared_ptr<BMAIndex>& index, const std::vector<Date>& fixingDates,
                                   const Date& accrualStartDate, const Date& accrualEndDate, const bool includeSpread,
                                   const Real spread, const Real gearing, Real cap, Real floor, const bool nakedOption,
                                   const Time t, const RandomVariable& x) const;

    /* Approximation via plain Ibor coupon with fixing date = first fixing date and the fixing() method above. */
    RandomVariable subPeriodsRate(const QuantLib::ext::shared_ptr<InterestRateIndex>& index,
                                  const std::vector<Date>& fixingDates, const Time t, const RandomVariable& x) const;

private:
    QuantLib::ext::shared_ptr<IrLgm1fParametrization> p_;
};

} // namespace QuantExt
