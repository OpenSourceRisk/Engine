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

/*
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

#include <qle/pricingengines/blackbondoptionengine.hpp>
#include <ql/cashflows/cashflows.hpp>
#include <ql/pricingengines/blackformula.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <ql/exercise.hpp>
#include <ql/quote.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>

namespace QuantExt {

    //! no vol structures implemented yet besides constant volatility
    BlackBondOptionEngine::BlackBondOptionEngine(
        const Handle<SwaptionVolatilityStructure>& yieldVolStructure,
        const Handle<YieldTermStructure>& discountCurve)
    :volatility_(yieldVolStructure), discountCurve_(discountCurve) {
        registerWith(volatility_);
        registerWith(discountCurve_);
    }

    Real BlackBondOptionEngine::spotIncome() const {
        //! settle date of embedded option assumed same as that of bond
        Date settlement = arguments_.settlementDate;
        Leg cf = arguments_.cashflows;
        Date optionMaturity = arguments_.putCallSchedule[0]->date();

        /* the following assumes
        1. cashflows are in ascending order !
        2. income = coupons paid between settlementDate() and put/call date
        */
        Real income = 0.0;
        for (Size i = 0; i < cf.size() - 1; ++i) {
            if (!cf[i]->hasOccurred(settlement, false)) {
                if (cf[i]->hasOccurred(optionMaturity, false)) {
                    income += cf[i]->amount() *
                        discountCurve_->discount(cf[i]->date());
                }
                else {
                    break;
                }
            }
        }
        return income / discountCurve_->discount(settlement);
    }


    Volatility BlackBondOptionEngine::forwardPriceVolatility()
        const {
        Date bondMaturity = arguments_.redemptionDate;
        Date exerciseDate = arguments_.callabilityDates[0];
        Leg fixedLeg = arguments_.cashflows;

        // value of bond cash flows at option maturity
        Real fwdNpv = CashFlows::npv(fixedLeg,
            **discountCurve_,
            false, exerciseDate);

        DayCounter dayCounter = arguments_.paymentDayCounter;
        Frequency frequency = arguments_.frequency;

        // adjust if zero coupon bond (see also bond.cpp)
        if (frequency == NoFrequency || frequency == Once)
            frequency = Annual;

        Rate fwdYtm = CashFlows::yield(fixedLeg,
            fwdNpv,
            dayCounter,
            Compounded,
            frequency,
            false, exerciseDate);

        InterestRate fwdRate(fwdYtm,
            dayCounter,
            Compounded,
            frequency);

        Time fwdDur = CashFlows::duration(fixedLeg,
            fwdRate,
            Duration::Modified,
            false, exerciseDate);

        Real cashStrike = arguments_.callabilityPrices[0];
        dayCounter = volatility_->dayCounter();
        Date referenceDate = volatility_->referenceDate();
        Time exerciseTime = dayCounter.yearFraction(referenceDate,
            exerciseDate);
        Time maturityTime = dayCounter.yearFraction(referenceDate,
            bondMaturity);
        Volatility yieldVol = volatility_->volatility(exerciseTime,
            maturityTime - exerciseTime,
            cashStrike);
        Volatility fwdPriceVol = yieldVol*fwdDur*fwdYtm;
        return fwdPriceVol;
    }


    void BlackBondOptionEngine::calculate() const {
        // validate args for Black engine
        QL_REQUIRE(arguments_.putCallSchedule.size() == 1,
            "Must have exactly one call/put date to use Black Engine");

        Date settle = arguments_.settlementDate;
        Date exerciseDate = arguments_.callabilityDates[0];
        QL_REQUIRE(exerciseDate >= settle,
            "must have exercise Date >= settlement Date");

        Leg fixedLeg = arguments_.cashflows;

        Real value = CashFlows::npv(fixedLeg,
            **discountCurve_,
            false, settle);

        Real npv = CashFlows::npv(fixedLeg,
            **discountCurve_,
            false, discountCurve_->referenceDate());

        Real discount = discountCurve_->discount(exerciseDate);

        Real fwdCashPrice = (value - spotIncome()) / discount;

        Real cashStrike = arguments_.callabilityPrices[0];

        Option::Type type = (arguments_.putCallSchedule[0]->type() ==
            Callability::Call ? Option::Call : Option::Put);

        Volatility priceVol = forwardPriceVolatility();

        Time exerciseTime = volatility_->dayCounter().yearFraction(
            volatility_->referenceDate(),
            exerciseDate);

        Real embeddedOptionValue =
            blackFormula(type,
                cashStrike,
                fwdCashPrice,
                priceVol*std::sqrt(exerciseTime),
                discount);

        if (arguments_.bondInPrice) {
            if (type == Option::Call) {
                results_.value = npv - embeddedOptionValue;
                results_.settlementValue = value - embeddedOptionValue;
            }
            else {
                results_.value = npv + embeddedOptionValue;
                results_.settlementValue = value + embeddedOptionValue;
            }
        } else {
            results_.value = embeddedOptionValue;
            results_.settlementValue = embeddedOptionValue;
        }
    }

} // namespace QuantExt
