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

#include <qle/pricingengines/mclgmbondengine.hpp>

namespace QuantExt {

void McLgmBondEngine::calculate() const {


    leg_ = { arguments_.cashflows };

    // todo: base currency of CAM or NPV currency of underlying bond ???
    currency_ = std::vector<Currency>(leg_.size(), model_->irlgm1f(0)->currency());

    // fixed for  bonds ...
    payer_.resize(true);

    //no option
    exercise_ = nullptr;

    McMultiLegBaseEngine::calculate();

    results_.value = resultValue_;

    // get the interim amcCalculator from the base class
    auto multiLegBaseAmcCalculator = QuantLib::ext::dynamic_pointer_cast<MultiLegBaseAmcCalculator>(amcCalculator());

    // cast to bondAMC to gain access to the overwritten simulate path method
    ext::shared_ptr<BondAmcCalculator> bondCalc =
        QuantLib::ext::make_shared<BondAmcCalculator>(*multiLegBaseAmcCalculator);
    bondCalc->addEngine(*this);
    ext::shared_ptr<AmcCalculator> amcCalc = bondCalc;

    results_.additionalResults["amcCalculator"] = amcCalc;

} // McLgmBondEngine::calculate

std::vector<QuantExt::RandomVariable> McLgmBondEngine::BondAmcCalculator::simulatePath(
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

        Size ind = std::distance(exerciseXvaTimes_.begin(), exerciseXvaTimes_.find(t));
        QL_REQUIRE(ind < exerciseXvaTimes_.size(), "FwdBondAmcCalculator::simulatePath(): internal error, xva time "
                                                       << t << " not found in exerciseXvaTimes vector.");

        RandomVariable bondRV = regModelUndDirty_[regModelIndex][ind].apply(initialState_, effPaths, xvaTimes_);

        // numeraire multiplication (incl and excl security spread), required as the base engine uses the numeraire
        // incl. the spread
        auto numeraire_inclSpread = engine_->lgmVectorised_[0].numeraire(
            t, paths[ind][engine_->model_->pIdx(CrossAssetModel::AssetType::IR, 0)], engine_->discountCurves_[0]);

        auto numeraire_exclSpread = engine_->lgmVectorised_[0].numeraire(
            t, paths[ind][engine_->model_->pIdx(CrossAssetModel::AssetType::IR, 0)], engine_->referenceCurve_);

        result[++counter] = bondRV * numeraire_inclSpread / numeraire_exclSpread;

    }
    return result;
}
} // namespace QuantExt
