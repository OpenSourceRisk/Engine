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

#include <qle/methods/lgmswaptionvegaparconverter.hpp>

#include <ql/option.hpp>
#include <ql/pricingengines/blackformula.hpp>
#include <ql/models/shortrate/calibrationhelpers/swaptionhelper.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/indexes/iborindex.hpp>

namespace QuantExt {

LgmSwaptionVegaParConverter::LgmSwaptionVegaParConverter(const QuantLib::ext::shared_ptr<LinearGaussMarkovModel>& model,
                                                         const std::vector<QuantLib::Period>& optionTerms,
                                                         const std::vector<QuantLib::Period>& underlyingTerms,
                                                         const QuantExt::ext::shared_ptr<SwapIndex>& index)
    : model_(model), optionTerms_(optionTerms), underlyingTerms_(underlyingTerms), index_(index) {

    dpardzero_ = Matrix(optionTerms_.size(), optionTerms_.size(), 0.0);

    optionTimes_.resize(optionTerms_.size());
    baseImpliedVols_.resize(optionTerms_.size());

    std::transform(optionTerms_.begin(), optionTerms_.end(), optionTimes_.begin(), [this](const Period& p) {
        return model_->termStructure()->timeFromReference(model_->termStructure()->referenceDate() + p);
    });

    QL_REQUIRE(optionTerms_.size() == underlyingTerms_.size(),
               "LgmSwaptionVegaParConverter: number of option terms ("
                   << optionTerms_.size() << ") does not match underlying terms (" << underlyingTerms_.size() << ")");

    auto engine = QuantLib::ext::make_shared<AnalyticLgmSwaptionEngine>(model_, Handle<YieldTermStructure>());

    std::vector<QuantLib::ext::shared_ptr<SwaptionHelper>> helpers;

    for (std::size_t i = 0; i < optionTerms_.size(); ++i) {
        helpers.push_back(QuantLib::ext::make_shared<QuantLib::SwaptionHelper>(
            optionTerms_[i], underlyingTerms_[i], Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.0050)),
            index_->iborIndex(), index_->fixedLegTenor(), index_->dayCounter(), index_->iborIndex()->dayCounter(),
            model->termStructure(), BlackCalibrationHelper::CalibrationErrorType::RelativePriceError, Null<Real>(), 1.0,
            VolatilityType::Normal, 0.0));
        helpers.back()->setPricingEngine(engine);
    }

    constexpr Real shift = 1E-7;

    for (std::size_t i = 0; i < optionTerms_.size(); ++i) {
        Real baseVol = helpers[i]->impliedVolatility(helpers[i]->modelValue(), 1E-6, 100, 0.0, 0.03);
        engine->setZetaShift(optionTimes_[i], shift);
        Real bumpedVol = helpers[i]->impliedVolatility(helpers[i]->modelValue(), 1E-6, 100, 0.0, 0.03);
        dpardzero_(i, i) = (bumpedVol - baseVol) / shift;
        engine->resetZetaShift();
        baseImpliedVols_[i] = baseVol;
    }

    dzerodpar_ = inverse(dpardzero_);
}

const std::vector<Real>& LgmSwaptionVegaParConverter::optionTimes() const { return optionTimes_; }
const Matrix& LgmSwaptionVegaParConverter::dpardzero() const { return dpardzero_; }
const Matrix& LgmSwaptionVegaParConverter::dzerodpar() const { return dzerodpar_; }
const std::vector<Real>& LgmSwaptionVegaParConverter::baseImpliedVols() const { return baseImpliedVols_; }

} // namespace QuantExt
