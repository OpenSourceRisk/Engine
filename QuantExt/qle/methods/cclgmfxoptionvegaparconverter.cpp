/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

#include <qle/methods/cclgmfxoptionvegaparconverter.hpp>

#include <ql/option.hpp>
#include <ql/pricingengines/blackformula.hpp>

namespace QuantExt {

CcLgmFxOptionVegaParConverter::CcLgmFxOptionVegaParConverter(const QuantLib::ext::shared_ptr<CrossAssetModel>& model,
                                                             const Size foreignCurrency,
                                                             const std::vector<Period>& optionTerms)
    : model_(model), foreignCurrency_(foreignCurrency), optionTerms_(optionTerms) {

    dpardzero_ = Matrix(optionTerms_.size(), optionTerms_.size(), 0.0);

    optionTimes_.resize(optionTerms_.size());
    baseImpliedVols_.resize(optionTerms.size());

    std::transform(optionTerms_.begin(), optionTerms_.end(), optionTimes_.begin(), [this](const Period& p) {
        return model_->irlgm1f(0)->termStructure()->timeFromReference(
            model_->irlgm1f(0)->termStructure()->referenceDate() + p);
    });

    std::vector<Real> discounts;
    std::vector<Real> forwards;
    std::vector<QuantLib::ext::shared_ptr<StrikedTypePayoff>> payoffs;

    for (auto const& t : optionTimes_) {
        discounts.push_back(model_->irlgm1f(0)->termStructure()->discount(t));
        forwards.push_back(model_->fxbs(foreignCurrency_)->fxSpotToday()->value() *
                           model_->irlgm1f(foreignCurrency_ + 1)->termStructure()->discount(t) /
                           model_->irlgm1f(0)->termStructure()->discount(t));
        payoffs.push_back(QuantLib::ext::make_shared<PlainVanillaPayoff>(Option::Type::Call, forwards.back()));
    }

    AnalyticCcLgmFxOptionEngine engine(model_, foreignCurrency_);

    constexpr Real shift = 1E-4;

    for (Size i = 0; i < optionTimes_.size(); ++i) {
        Real baseVol = blackFormulaImpliedStdDev(
                           Option::Type::Call, forwards[i], forwards[i],
                           engine.value(0.0, optionTimes_[i], payoffs[i], discounts[i], forwards[i]), discounts[i]) /
                       std::sqrt(optionTimes_[i]);
        baseImpliedVols_[i] = baseVol;
        for (Size j = 0; j <= i; ++j) {
            engine.setSigmaShift(i == 0 ? 0.0 : optionTimes_[j - 1], optionTimes_[j], shift);
            Real bumpedVol =
                blackFormulaImpliedStdDev(Option::Type::Call, forwards[i], forwards[i],
                                          engine.value(0.0, optionTimes_[i], payoffs[i], discounts[i], forwards[i]),
                                          discounts[i]) /
                std::sqrt(optionTimes_[i]);
            dpardzero_(i, j) = (bumpedVol - baseVol) / shift;
            engine.resetSigmaShift();
        }
    }

    dzerodpar_ = inverse(dpardzero_);
}

const std::vector<Real>& CcLgmFxOptionVegaParConverter::optionTimes() const { return optionTimes_; }
const Matrix& CcLgmFxOptionVegaParConverter::dpardzero() const { return dpardzero_; }
const Matrix& CcLgmFxOptionVegaParConverter::dzerodpar() const { return dzerodpar_; }
const std::vector<Real>& CcLgmFxOptionVegaParConverter::baseImpliedVols() const { return baseImpliedVols_; }

} // namespace QuantExt
