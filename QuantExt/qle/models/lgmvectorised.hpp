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

#include <qle/cashflows/subperiodscoupon.hpp>
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
    RandomVariable compoundedOnRate(const QuantLib::ext::shared_ptr<OvernightIndex>& index,
                                    const std::vector<Date>& fixingDates, const std::vector<Date>& valueDates,
                                    const std::vector<Real>& dt, const Natural rateCutoff, const bool includeSpread,
                                    const Real spread, const Real gearing, const Period lookback, Real cap, Real floor,
                                    const bool localCapFloor, const bool nakedOption,
                                    const std::vector<Time>& simTime,
                                    const std::vector<Size>& simIdx,
                                    const std::function<const RandomVariable*(Size)>& x) const;

    /* Exact if no cap/floors are present and t <= first value date.
       Approximations are applied for t > first value date or when cap / floors are present. */
    RandomVariable averagedOnRate(const QuantLib::ext::shared_ptr<OvernightIndex>& index, const std::vector<Date>& fixingDates,
                                  const std::vector<Date>& valueDates, const std::vector<Real>& dt,
                                  const Natural rateCutoff, const bool includeSpread, const Real spread,
                                  const Real gearing, const Period lookback, Real cap, Real floor,
                                  const bool localCapFloor, const bool nakedOption, const Time t,
                                  const RandomVariable& x) const;
    RandomVariable averagedOnRate(const QuantLib::ext::shared_ptr<OvernightIndex>& index, const std::vector<Date>& fixingDates,
                                  const std::vector<Date>& valueDates, const std::vector<Real>& dt,
                                  const Natural rateCutoff, const bool includeSpread, const Real spread,
                                  const Real gearing, const Period lookback, Real cap, Real floor,
                                  const bool localCapFloor, const bool nakedOption,
                                  const std::vector<Time>& simTime,
                                  const std::vector<Size>& simIdx,
                                  const std::function<const RandomVariable*(Size)>& x) const;

    /* Exact if no cap/floors are present and t <= first value date.
       Approximations are applied for t > first value date or when cap / floors are present. */
    RandomVariable averagedBmaRate(const QuantLib::ext::shared_ptr<BMAIndex>& index, const std::vector<Date>& fixingDates,
                                   const Date& accrualStartDate, const Date& accrualEndDate, const bool includeSpread,
                                   const Real spread, const Real gearing, Real cap, Real floor, const bool nakedOption,
                                   const Time t, const RandomVariable& x) const;

    /* Exact. Requires observation time t <= fixingDate */
    RandomVariable subPeriodsRate(const QuantLib::ext::shared_ptr<InterestRateIndex>& index,
                                  const std::vector<Date>& fixingDates, const Time t, const RandomVariable& x,
                                  const std::vector<Time>& accrualFractions,
                                  const SubPeriodsCoupon1::Type type, const bool includeSpread,
                                  const Spread spread, const Real gearing,
                                  const Time accrualPeriod) const;

   /*! Analytical pricing of a range accrual coupon in the LGM1F model.
       Each observation is priced as a digital caplet/floorlet using the closed-form formula
       for digital options on zero bonds with delayed payment (see ORE documentation 5.1.25).
       Requires the conditioning time t to be no later than the value date S_i of every future
       observation, i.e. t <= S_i. This is necessary because the formula conditions on z(t) = x
       and integrates alpha^2 over [t, S_i]; for S_i < t the fixing would depend on the path of x
       before t, which is not available in the 1D backward solver. A violation triggers QL_REQUIRE.
       \param index the underlying Ibor index for the range observations
       \param fixingDate the fixing date of the coupon rate
       \param observationDates the observation dates within the accrual period
       \param lowerTrigger the lower range bound
       \param upperTrigger the upper range bound
       \param gearing the coupon gearing
       \param spread the coupon spread
       \param payTime the payment time (year fraction)
       \param t the current observation/simulation time
       \param x the LGM state variable
       \param fixedRate if not Null<Real>(), the coupon pays fixedRate * (n/N) instead of
                        the floating formula gearing * Libor * (n/N) + spread
   */
    RandomVariable rangeAccrualRate(const QuantLib::ext::shared_ptr<IborIndex>& index,
                                    const Date& fixingDate,
                                    const std::vector<Date>& observationDates,
                                    const Real lowerTrigger, const Real upperTrigger,
                                    const Real gearing, const Spread spread,
                                    const Time payTime,
                                    const Time t, const RandomVariable& x,
                                    const Real fixedRate = Null<Real>()) const;

private:
    QuantLib::ext::shared_ptr<IrLgm1fParametrization> p_;
};

} // namespace QuantExt
