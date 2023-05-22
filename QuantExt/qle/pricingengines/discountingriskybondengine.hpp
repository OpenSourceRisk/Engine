/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

/*! \file qle/pricingengines/discountingriskybondengine.hpp
    \brief Risky Bond Engine
    \ingroup engines
*/

#ifndef quantext_discounting_riskybond_engine_hpp
#define quantext_discounting_riskybond_engine_hpp

#include <ql/instruments/bond.hpp>
#include <ql/termstructures/defaulttermstructure.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/time/period.hpp>
#include <qle/instruments/cashflowresults.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Discounting Risky Bond Engine
/*! WARNING: Only covers Vanilla coupon bonds (floating and fixed rate), and
    Zero Bonds (one cashflow, a redemption at maturity).

    This class implements pricing of Risky Bonds by discounting the future
    nominal cash flows using the respective yield curves, and probability of
    survival.
    The nominal recovered in case of default is calculated as recovery rate times
    the integral of probability of default until maturity date. For coupon bonds
    the coupon periods are taken as the time step for integration, for a zero
    bond the time step period provided is used.

    \ingroup engines

*/
class DiscountingRiskyBondEngine : public QuantLib::Bond::engine {
public:

    struct BondNPVCalculationResults {
	// always provided in calculateNpv()
        Real npv;
        Real compoundFactorSettlement;
        Real cashflowsBeforeSettlementValue;
	// only provided in calculateNpv() when additionalResults = true
        std::vector<CashFlowResults> cashflowResults;
    };

    DiscountingRiskyBondEngine(const Handle<YieldTermStructure>& discountCurve,
                               const Handle<DefaultProbabilityTermStructure>& defaultCurve,
                               const Handle<Quote>& recoveryRate, const Handle<Quote>& securitySpread,
                               Period timestepPeriod, boost::optional<bool> includeSettlementDateFlows = boost::none);

    //! alternative constructor (does not require default curve or recovery rate)
    DiscountingRiskyBondEngine(const Handle<YieldTermStructure>& discountCurve, const Handle<Quote>& securitySpread,
                               Period timestepPeriod, boost::optional<bool> includeSettlementDateFlows = boost::none);

    void calculate() const override;

    /*! Calculate the npv, compoundFactorSettlement, cashflowsBeforeSettlementValue and the additional CashflowResults
        as of the npvDate including cashflows eligible w.r.t. the given settlement date
        - If conditionalOnSurvival is set to true, the npv is computed conditional on the survival until the npvDate,
          otherwise the npv is including the default probability between today and the npvDate
        - If an incomeCurve is given, this is used to compound the npv from today to the npvDate, otherwise the curve
          built in the engine as discount curve + security Spread is used. */
    BondNPVCalculationResults calculateNpv(const Date& npvDate, const Date& settlementDate, const Leg& cashflows,
                                           boost::optional<bool> includeSettlementDateFlows = boost::none,
                                           const Handle<YieldTermStructure>& incomeCurve = Handle<YieldTermStructure>(),
                                           const bool conditionalOnSurvival = true,
                                           const bool additionalResults = true) const;

    // inspectors
    Handle<YieldTermStructure> discountCurve() const { return discountCurve_; };
    Handle<DefaultProbabilityTermStructure> defaultCurve() const { return defaultCurve_; };
    Handle<Quote> recoveryRate() const { return recoveryRate_; };
    Handle<Quote> securitySpread() const { return securitySpread_; };

protected:
    Handle<YieldTermStructure> discountCurve_;
    mutable Handle<DefaultProbabilityTermStructure> defaultCurve_;
    mutable Handle<Quote> recoveryRate_;
    Handle<Quote> securitySpread_;
    Period timestepPeriod_;
    boost::optional<bool> includeSettlementDateFlows_;
};
} // namespace QuantExt

#endif
