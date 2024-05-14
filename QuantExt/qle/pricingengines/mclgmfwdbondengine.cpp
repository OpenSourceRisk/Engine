/*
 Copyright (C) 2024 Quaternion Risk Management Ltd
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

#include <qle/pricingengines/mclgmfwdbondengine.hpp>
#include <qle/cashflows/scaledcoupon.hpp>
#include <ql/exercise.hpp>

namespace QuantExt {

void McLgmFwdBondEngine::calculate() const {

    //discounting taking care of in the builder file ...

    //truncate legs akin to fwd bond method...
    Leg truncatedLeg;
    for (auto& cf : arguments_.underlying->cashflows()) {
        if (!cf->hasOccurred(arguments_.fwdMaturityDate))
            truncatedLeg.push_back(cf);
    }
    leg_ = { truncatedLeg };

    // todo: base currency of CAM or NPV currency of underlying bond ???
    currency_ = std::vector<Currency>(leg_.size(), model_->irlgm1f(0)->currency());

    // fixed for underlying bonds ...
    payer_.resize(true);

    //new argument to scale the outputs ...
    //not sure why we need this for fwd bonds and not in bonds
    multiplier_ = arguments_.bondNotional;

    //generally: no option, but we can switch off the underlying npv in the output
    //real option values in not beeing calculated as payOff function overwrites resultValue
    //exercise_ = ext::make_shared<EuropeanExercise>(arguments_.fwdMaturityDate);
    exercise_ = nullptr;

    McMultiLegBaseEngine::calculate();

    results_.underlyingSpotValue = resultUnderlyingNpv_;

    results_.value = resultValue_;

    results_.additionalResults["amcCalculator"] = amcCalculator();

} // McLgmSwaptionEngine::calculate


Real McLgmFwdBondEngine::payOff(RandomVariable pathValueOption) const{

    //pathValueOption not used here, but in the base class implementation...
    //engine ignores credit curve and uses the spread on discount only ...

    // the case of dirty strike corresponds here to an accrual of 0.0. This will be convenient in the code.
    Date bondSettlementDate = arguments_.underlying->settlementDate(arguments_.fwdMaturityDate);
    Real accruedAmount = arguments_.settlementDirty ? 0.0
                               : arguments_.underlying->accruedAmount(bondSettlementDate) * arguments_.underlying->notional(bondSettlementDate) / 100.0 *
                                     arguments_.bondNotional;

    //cash settlement case
    //TODO : do we need the numeraire, here?
    double forwardBondValue = resultUnderlyingNpv_ / (incomeCurve_->discount(bondSettlementDate));

    //vanilla forward bond calculation
    double forwardContractForwardValue = (*arguments_.payoff)(forwardBondValue - accruedAmount);

    //strike logic
    Date npvDate = discountCurves_[0]->referenceDate();
    Date cmpPaymentDate = arguments_.compensationPaymentDate;
    Real cmpPayment = arguments_.compensationPayment;

    if (cmpPayment == Null<Real>())
        cmpPayment = 0.0;

    if (cmpPaymentDate == Null<Date>())
        cmpPaymentDate = npvDate;

    Date cmpPaymentDate_use = cmpPaymentDate >= npvDate ? cmpPaymentDate : arguments_.fwdMaturityDate;
    cmpPayment = cmpPaymentDate >= npvDate ? cmpPayment : 0.0;

    std::cout << std::endl;
    std::cout << std::endl;
    std::cout << "discountContractCurve " << discountContractCurve_->discount(arguments_.fwdSettlementDate) << std::endl;
    std::cout << "discountCurve         " << discountCurves_[0]->discount(arguments_.fwdSettlementDate) << std::endl;
    std::cout << "incomeCurve           " << incomeCurve_->discount(arguments_.fwdSettlementDate) << std::endl;
    std::cout << std::endl;
    std::cout << std::endl;

    //finally ... the forwardContractPresentValue
    double forwardContractPresentValue = forwardContractForwardValue * (discountContractCurve_->discount(arguments_.fwdSettlementDate)) -
        cmpPayment * (discountContractCurve_->discount(cmpPaymentDate_use));

    //TODO : do we need the numeraire, here? It is one, given the argument list.
    return forwardContractPresentValue;// * model_->numeraire(0, 0.0, 0.0, discountCurves_[0]);
}

} // namespace QuantExt
