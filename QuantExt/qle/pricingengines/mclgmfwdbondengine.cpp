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

void McLgmFwdBondEngine::setMember() const {

    // assemble date logic and interim results for both payoff and amc calculator
    Date npvDate = contractCurve_->referenceDate();

    Date maturityDate = arguments_.fwdMaturityDate;

    Date bondSettlementDate = arguments_.underlying->settlementDate(maturityDate);

    Real cmpPayment = arguments_.compensationPayment;
    if (cmpPayment == Null<Real>()) {
        cmpPayment = 0.0;
    }

    Date cmpPaymentDate = arguments_.compensationPaymentDate;
    if (cmpPaymentDate == Null<Date>()) {
        cmpPaymentDate = npvDate;
    }

    // set interim results for both payoff and amc calculator
    cmpPayment_ = cmpPaymentDate >= npvDate ? cmpPayment : 0.0;

    // the case of dirty strike corresponds here to an accrual of 0.0. This will be convenient in the code.
    accruedAmount_ = arguments_.settlementDirty
                         ? 0.0
                         : arguments_.underlying->accruedAmount(bondSettlementDate) *
                               arguments_.underlying->notional(bondSettlementDate) / 100.0 * arguments_.bondNotional;

    if (!arguments_.isPhysicallySettled)
        incomeCurveDate_ = bondSettlementDate;
    else
        incomeCurveDate_ = arguments_.fwdSettlementDate;

    contractCurveDate_ = arguments_.fwdSettlementDate;
    cmpPaymentDate_ = cmpPaymentDate >= npvDate ? cmpPaymentDate : maturityDate;
}

void McLgmFwdBondEngine::calculate() const {

    setMember();

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

    // no option
    exercise_ = nullptr;

    McMultiLegBaseEngine::calculate();

    // take result from base engine, akin to calculateBondNpv in DiscountingForwardBondEngine
    results_.underlyingSpotValue = resultUnderlyingNpv_ * arguments_.bondNotional;

    // calculation of contract value, akin to calculateForwardContractPresentValue in DiscountingForwardBondEngine
    results_.value = payOff();

    // get the interim amcCalculator from the base class
    auto multiLegBaseAmcCalculator = boost::dynamic_pointer_cast<MultiLegBaseAmcCalculator>(amcCalculator());

    // cast to fwdAMC to gain access to the overwritten simulate path method
    ext::shared_ptr<FwdBondAmcCalculator> fwdBondCalc =
        boost::make_shared<FwdBondAmcCalculator>(*multiLegBaseAmcCalculator);
    fwdBondCalc->addEngine(*this);
    ext::shared_ptr<AmcCalculator> amcCalc = fwdBondCalc;

    results_.additionalResults["amcCalculator"] = amcCalc;

} // McLgmSwaptionEngine::calculate

double McLgmFwdBondEngine::payOff() const {

    // engine ignores credit curve and uses the spread on discount only ...
    // ignores T-lock feature

    double forwardBondValue = results_.underlyingSpotValue / incomeCurve_->discount(incomeCurveDate_);

    // vanilla forward bond calculation
    double forwardContractForwardValue = (*arguments_.payoff)(forwardBondValue - accruedAmount_);

    // builder ensures we calculate with a clean price or we divide by one
    forwardContractForwardValue /= conversionFactor();

    // include compensation payments
    double forwardContractPresentValue =
        forwardContractForwardValue * contractCurve_->discount(contractCurveDate_) -
        cmpPayment_ * contractCurve_->discount(cmpPaymentDate_);

    return forwardContractPresentValue;
}

std::vector<QuantExt::RandomVariable> McLgmFwdBondEngine::FwdBondAmcCalculator::simulatePath(
    const std::vector<QuantLib::Real>& pathTimes, const std::vector<std::vector<QuantExt::RandomVariable>>& paths,
    const std::vector<size_t>& relevantPathIndex, const std::vector<size_t>& relevantTimeIndex) {

    QL_REQUIRE(!paths.empty(), "FwdBondAmcCalculator::simulatePath(): no future path times, this is not allowed.");
    QL_REQUIRE(pathTimes.size() == paths.size(), "FwdBondAmcCalculator::simulatePath(): inconsistent pathTimes size ("
                                                     << pathTimes.size() << ") and paths size (" << paths.size()
                                                     << ") - internal error.");
    QL_REQUIRE(relevantPathIndex.size() >= xvaTimes_.size(),
               "MultiLegBaseAmcCalculator::simulatePath() relevant path indexes ("
                   << relevantPathIndex.size() << ") >= xvaTimes (" << xvaTimes_.size()
                   << ") required - internal error.");

    // convert dates to times
    double maturityTime = engine_->time(engine_->arguments_.fwdMaturityDate);
    double incomeCurveTime = engine_->time(engine_->incomeCurveDate_);
    double contractCurveTime = engine_->time(engine_->contractCurveDate_);
    double cmpPaymentTime = engine_->time(engine_->cmpPaymentDate_);

    boost::shared_ptr<ForwardBondTypePayoff> fwdBndPayOff =
        boost::dynamic_pointer_cast<ForwardBondTypePayoff>(engine_->arguments_.payoff);
    QL_REQUIRE(fwdBndPayOff, "not a ForwardBondTypePayoff");

    // bool stickyCloseOutRun = false;
    std::size_t regModelIndex = 0;

    for (size_t i = 0; i < relevantPathIndex.size(); ++i) {
        if (relevantPathIndex[i] != relevantTimeIndex[i]) {
            // stickyCloseOutRun = true;
            regModelIndex = 1;
            break;
        }
    }

    // init result vector
    Size samples = paths.front().front().size();
    std::vector<RandomVariable> result(xvaTimes_.size() + 1, RandomVariable(paths.front().front().size(), 0.0));

    /* put together the relevant simulation times on the input paths and check for consistency with xva times,
    also put together the effective paths by filtering on relevant simulation times and model indices */
    std::vector<std::vector<const RandomVariable*>> effPaths(
        xvaTimes_.size(), std::vector<const RandomVariable*>(externalModelIndices_.size()));

    Size timeIndex = 0;
    for (Size i = 0; i < xvaTimes_.size(); ++i) {
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

        // strip off values after expiry of forward contract
        if (t >= maturityTime) {
            result[++counter] = RandomVariable(samples, 0.0);
            continue;
        }
        Size ind = std::distance(exerciseXvaTimes_.begin(), exerciseXvaTimes_.find(t));
        QL_REQUIRE(ind < exerciseXvaTimes_.size(), "FwdBondAmcCalculator::simulatePath(): internal error, xva time "
                                                       << t << " not found in exerciseXvaTimes vector.");

        RandomVariable underylingSpotRV =
            regModelUndDirty_[regModelIndex][ind].apply(initialState_, effPaths, xvaTimes_) *
            RandomVariable(samples, engine_->arguments_.bondNotional);

        // numeraire multiplication (incl and excl security spread), required as the base engine uses the numeraire
        // incl. the spread
        auto numeraire_inclSpread = engine_->lgmVectorised_[0].numeraire(
            t, paths[ind][engine_->model_->pIdx(CrossAssetModel::AssetType::IR, 0)], engine_->discountCurves_[0]);

        auto numeraire_exclSpread = engine_->lgmVectorised_[0].numeraire(
            t, paths[ind][engine_->model_->pIdx(CrossAssetModel::AssetType::IR, 0)], engine_->referenceCurve_);

        RandomVariable spotNumeraireAdjusted = underylingSpotRV * numeraire_inclSpread / numeraire_exclSpread;

        // compounding
        auto disc_income = engine_->lgmVectorised_[0].discountBond(
            0.0, incomeCurveTime, paths[ind][engine_->model_->pIdx(CrossAssetModel::AssetType::IR, 0)],
            engine_->incomeCurve_);

        RandomVariable forwardBondValue = spotNumeraireAdjusted / disc_income;

        // vanilla forward bond calculation : differentiate between long/short
        RandomVariable forwardContractForwardValue;
        if (fwdBndPayOff->forwardType() == Position::Type::Long)
            forwardContractForwardValue = (forwardBondValue - RandomVariable(samples, engine_->accruedAmount_)) -
                                          RandomVariable(samples, fwdBndPayOff->strike());
        else
            forwardContractForwardValue = RandomVariable(samples, fwdBndPayOff->strike()) -
                                          (forwardBondValue - RandomVariable(samples, engine_->accruedAmount_));

        // builder ensures we have a clean price or we divide by one.
        forwardContractForwardValue /= RandomVariable(samples, engine_->conversionFactor());

        // Present value and compensation payment...
        auto disc_contract = engine_->lgmVectorised_[0].discountBond(
            0.0, contractCurveTime, paths[ind][engine_->model_->pIdx(CrossAssetModel::AssetType::IR, 0)],
            engine_->contractCurve_);

        auto disc_cmpPayment = engine_->lgmVectorised_[0].discountBond(
            0.0, cmpPaymentTime, paths[ind][engine_->model_->pIdx(CrossAssetModel::AssetType::IR, 0)],
            engine_->contractCurve_);

        // forwardContractPresentValue
        result[++counter] = forwardContractForwardValue * disc_contract -
                            (RandomVariable(samples, engine_->cmpPayment_) * disc_cmpPayment);
    }
    result.resize(relevantPathIndex.size() + 1, RandomVariable(samples, 0.0));
    return result;
}

} // namespace QuantExt
