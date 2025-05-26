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
                               arguments_.underlying->notional(bondSettlementDate) / 100.0;

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

    // take result from base engine, this depends on the pathvaluedirty only (i.e. no regression model -> no
    // compounding) the raw (untouched) pathvaluedirty is the underlying bond value at disounted to t0
    double forwardBondValue = resultUnderlyingNpv_ / incomeCurve_->discount(incomeCurveDate_);

    // vanilla forward bond calculation
    double forwardContractForwardValue = (*arguments_.payoff)((forwardBondValue - accruedAmount_) * arguments_.bondNotional);

    // builder ensures we calculate with a clean price or we divide by one
    forwardContractForwardValue /= conversionFactor();

    // include compensation payments
    double forwardContractPresentValue = forwardContractForwardValue * contractCurve_->discount(contractCurveDate_) -
                                         cmpPayment_ * contractCurve_->discount(cmpPaymentDate_);

    results_.value = forwardContractPresentValue;

    // get the interim amcCalculator from the base class
    auto multiLegBaseAmcCalculator = QuantLib::ext::dynamic_pointer_cast<MultiLegBaseAmcCalculator>(amcCalculator());

    // cast to fwdAMC to gain access to the overwritten simulate path method
    ext::shared_ptr<FwdBondAmcCalculator> fwdBondCalc =
        QuantLib::ext::make_shared<FwdBondAmcCalculator>(*multiLegBaseAmcCalculator);
    fwdBondCalc->addEngine(*this);
    ext::shared_ptr<AmcCalculator> amcCalc = fwdBondCalc;

    results_.additionalResults["amcCalculator"] = amcCalc;

} // McLgmSwaptionEngine::calculate

RandomVariable
McLgmFwdBondEngine::overwritePathValueUndDirty(double t, const RandomVariable& pathValueUndDirty,
                                               const std::set<Real>& exerciseXvaTimes,
                                               const std::vector<std::vector<QuantExt::RandomVariable>>& paths) const {

    double fwdMaturity = time(arguments_.fwdMaturityDate);
    if (t < fwdMaturity) {

        Size ind = std::distance(exerciseXvaTimes.begin(), exerciseXvaTimes.find(t));
        Size samples = paths.front().front().size();

        // numeraire adjustment {ref + spread} (t) / ois (t) ... ois below with return ...
        auto numeraire_bonddiscount = lgmVectorised_[0].numeraire(
            t, paths[ind][model_->pIdx(CrossAssetModel::AssetType::IR, 0)], discountCurves_[0]);

        auto numeraire_contract = lgmVectorised_[0].numeraire(
            t, paths[ind][model_->pIdx(CrossAssetModel::AssetType::IR, 0)], contractCurve_);

        // compounding with income curve from t to fwd_maturity T
        double compoundingTime = time(incomeCurveDate_);
        auto incomeCompounding = lgmVectorised_[0].discountBond(
            t, compoundingTime, paths[ind][model_->pIdx(CrossAssetModel::AssetType::IR, 0)], incomeCurve_);

        RandomVariable forwardBondValue = pathValueUndDirty * numeraire_bonddiscount / incomeCompounding;

        // vanilla forward bond calculation at fwdMaturity : differentiate between long/short
        ext::shared_ptr<ForwardBondTypePayoff> fwdBndPayOff =
            ext::dynamic_pointer_cast<ForwardBondTypePayoff>(arguments_.payoff);
        QL_REQUIRE(fwdBndPayOff, "not a ForwardBondTypePayoff");

        RandomVariable strikePayment = RandomVariable(samples, fwdBndPayOff->strike() / arguments_.bondNotional);

        RandomVariable forwardContractForwardValueRV;
        if (fwdBndPayOff->forwardType() == Position::Type::Long)
            forwardContractForwardValueRV =
                (forwardBondValue - RandomVariable(samples, accruedAmount_)) - strikePayment;
        else
            forwardContractForwardValueRV =
                strikePayment - (forwardBondValue - RandomVariable(samples, accruedAmount_));

        return forwardContractForwardValueRV / numeraire_contract;

    } else
        return pathValueUndDirty;
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
    double contractCurveTime = engine_->time(engine_->contractCurveDate_);
    double cmpPaymentTime = engine_->time(engine_->cmpPaymentDate_);

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

        RandomVariable forwardContractForwardValue =
            regModelUndDirty_[regModelIndex][ind].apply(initialState_, effPaths, xvaTimes_);

        // builder ensures we have a clean price or we divide by one.
        forwardContractForwardValue /= RandomVariable(samples, engine_->conversionFactor());

        // Present value and compensation payment...
        auto disc_contract = engine_->lgmVectorised_[0].discountBond(
            t, contractCurveTime, paths[ind][engine_->model_->pIdx(CrossAssetModel::AssetType::IR, 0)],
            engine_->contractCurve_);

        auto disc_cmpPayment = engine_->lgmVectorised_[0].discountBond(
            t, cmpPaymentTime, paths[ind][engine_->model_->pIdx(CrossAssetModel::AssetType::IR, 0)],
            engine_->contractCurve_);

        auto forwardContractPresentValue = forwardContractForwardValue * disc_contract -
                            (RandomVariable(samples, engine_->cmpPayment_) * disc_cmpPayment);

        result[++counter] = forwardContractPresentValue * RandomVariable(samples, engine_->arguments_.bondNotional);

    }
    result.resize(relevantPathIndex.size() + 1, RandomVariable(samples, 0.0));
    return result;
}

} // namespace QuantExt
