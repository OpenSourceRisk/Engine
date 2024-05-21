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

#include <ql/exercise.hpp>
#include <qle/cashflows/scaledcoupon.hpp>
#include <qle/pricingengines/mclgmfwdbondengine.hpp>

namespace QuantExt {

void McLgmFwdBondEngine::calculate() const {

    // discounting taking care of in the builder file ...

    // truncate legs akin to fwd bond method...
    Leg truncatedLeg;
    for (auto& cf : arguments_.underlying->cashflows()) {
        if (!cf->hasOccurred(arguments_.fwdMaturityDate))
            truncatedLeg.push_back(cf);
    }
    leg_ = {truncatedLeg};

    // todo: base currency of CAM or NPV currency of underlying bond ???
    currency_ = std::vector<Currency>(leg_.size(), model_->irlgm1f(0)->currency());

    // fixed for underlying bonds ...
    payer_.resize(true);

    // new argument to scale the outputs ...
    // not sure yet, why we need this for fwd bonds and not in bonds
    multiplier_ = arguments_.bondNotional;

    // generally: no option
    exercise_ = nullptr;

    McMultiLegBaseEngine::calculate();

    // take result from base engine, akin to calculateBondNpv in DiscountingForwardBondEngine
    results_.underlyingSpotValue = resultUnderlyingNpv_;

    // calculation of contract value, akin to calculateForwardContractPresentValue in DiscountingForwardBondEngine
    results_.value = payOff(0.0, resultUnderlyingNpv_);

    // get the interim amcCalculator from the base class
    auto multiLegBaseAmcCalculator = boost::dynamic_pointer_cast<MultiLegBaseAmcCalculator>(amcCalculator());

    // cast to fwdAMC to gain access to the overwritten simulate path method
    ext::shared_ptr<FwdBondAmcCalculator> fwdBondCalc =
        boost::make_shared<FwdBondAmcCalculator>(*multiLegBaseAmcCalculator);
    fwdBondCalc->addEngine(*this);
    ext::shared_ptr<AmcCalculator> amcCalc = fwdBondCalc;

    results_.additionalResults["amcCalculator"] = amcCalc;

} // McLgmSwaptionEngine::calculate

double McLgmFwdBondEngine::payOff(double time, double underlyingSpot) const {

    // engine ignores credit curve and uses the spread on discount only ...

    // strip off values after expiry of forward contract
    double maturityTime = McLgmFwdBondEngine::time(arguments_.fwdMaturityDate);
    if (time >= maturityTime)
        return 0.0;

    // retrieve correct numeraire - however numeraire value is one
    double numeraire_income = model_->numeraire(0, 0.0, 0.0, incomeCurve_);
    double numeraire_contract = model_->numeraire(0, 0.0, 0.0, discountContractCurve_);

    // the case of dirty strike corresponds here to an accrual of 0.0. This will be convenient in the code.
    Date bondSettlementDate = arguments_.underlying->settlementDate(arguments_.fwdMaturityDate);
    Real accruedAmount = arguments_.settlementDirty ? 0.0
                                                    : arguments_.underlying->accruedAmount(bondSettlementDate) *
                                                          arguments_.underlying->notional(bondSettlementDate) / 100.0 *
                                                          arguments_.bondNotional;

    // cash settlement case
    auto forwardBondValue = underlyingSpot / incomeCurve_->discount(bondSettlementDate) / numeraire_income;

    // vanilla forward bond calculation
    auto forwardContractForwardValue = (*arguments_.payoff)(forwardBondValue - accruedAmount);

    auto forwardContractPresentValue = forwardContractForwardValue *
                                       discountContractCurve_->discount(arguments_.fwdSettlementDate) *
                                       numeraire_contract;

    return forwardContractPresentValue;
}

std::vector<QuantExt::RandomVariable> McLgmFwdBondEngine::FwdBondAmcCalculator::simulatePath(
    const std::vector<QuantLib::Real>& pathTimes, std::vector<std::vector<QuantExt::RandomVariable>>& paths,
    const std::vector<size_t>& relevantPathIndex, const std::vector<size_t>& relevantTimeIndex) {

    // init result vector
    Size samples = paths.front().front().size();
    std::vector<RandomVariable> result(xvaTimes_.size() + 1, RandomVariable(paths.front().front().size(), 0.0));

    /* put together the relevant simulation times on the input paths and check for consistency with xva times,
    also put together the effective paths by filtering on relevant simulation times and model indices */
    std::vector<std::vector<const RandomVariable*>> effPaths(
        xvaTimes_.size(), std::vector<const RandomVariable*>(externalModelIndices_.size()));

    Size timeIndex = 0;
    for (Size i = 0; i < relevantPathIndex.size(); ++i) {
        size_t pathIdx = relevantPathIndex[i];
        for (Size j = 0; j < externalModelIndices_.size(); ++j) {
            effPaths[timeIndex][j] = &paths[pathIdx][externalModelIndices_[j]];
        }
        ++timeIndex;
    }

    // simulate the path: result at first time index is simply the reference date npv
    result[0] = RandomVariable(samples, engine_->results_.value);

    Size counter = 0;
    for (auto t : xvaTimes_) {

        Size ind = std::distance(exerciseXvaTimes_.begin(), exerciseXvaTimes_.find(t));
        QL_REQUIRE(ind < exerciseXvaTimes_.size(),
                   "MultiLegBaseAmcCalculator::simulatePath(): internal error, xva time "
                       << t << " not found in exerciseXvaTimes vector.");

        auto underylingSpotRV = regModelUndDirty_[ind].apply(initialState_, effPaths, xvaTimes_);

        double underlyingSpot =
            expectation(underylingSpotRV).at(0) * engine_->model_->numeraire(0, 0.0, 0.0, engine_->discountCurves_[0]);

        result[++counter] = RandomVariable(samples, engine_->payOff(t, underlyingSpot));
    }
    return result;
}

} // namespace QuantExt
