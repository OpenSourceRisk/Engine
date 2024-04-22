/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

/*! \file fxbsmodel.hpp
    \brief fx black scholes model
    \ingroup models
*/

#pragma once

#include <qle/models/fxbsparametrization.hpp>
#include <qle/models/fxmodel.hpp>

#include <iostream>

namespace QuantExt {

class FxBsModel : public FxModel {
public:
    explicit FxBsModel(const QuantLib::ext::shared_ptr<FxBsParametrization>& parametrization);

    const QuantLib::ext::shared_ptr<Parametrization> parametrizationBase() const override { return parametrization_; }

    Handle<Quote> fxSpotToday() const override { return parametrization_->fxSpotToday(); }
    Size n() const override { return 1; }
    Size m() const override { return 1; }

    Array eulerStep(const Time t0, const Array& x0, const Time dt, const Array& dw, const Real r_dom,
                    const Real r_for) const override;

private:
    QuantLib::ext::shared_ptr<FxBsParametrization> parametrization_;
};

inline FxBsModel::FxBsModel(const QuantLib::ext::shared_ptr<FxBsParametrization>& parametrization)
    : parametrization_(parametrization) {
    QL_REQUIRE(parametrization != nullptr, "FxBsModel: parametrization is null");
}

inline Array FxBsModel::eulerStep(const Time t0, const Array& x0, const Time dt, const Array& dw, const Real r_dom,
                                  const Real r_for) const {
    Real sigma = parametrization_->sigma(t0);
    return x0 + (r_dom - r_for - 0.5 * sigma * sigma) * dt + sigma * std::sqrt(dt) * dw[0];
}

} // namespace QuantExt
