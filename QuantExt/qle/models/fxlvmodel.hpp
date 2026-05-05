/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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

/*! \file fxlvmodel.hpp
    \brief fx lv model
    \ingroup models
*/

#pragma once

#include <qle/models/fxlvparametrization.hpp>
#include <qle/models/fxmodel.hpp>

namespace QuantExt {

class FxLvModel : public FxModel {
public:
    explicit FxLvModel(const QuantLib::ext::shared_ptr<FxLvParametrization>& parametrization,
                       const Discretization disc = Discretization::PC);

    const QuantLib::ext::shared_ptr<Parametrization> parametrizationBase() const override { return parametrization_; }

    Handle<Quote> fxSpotToday() const override { return parametrization_->fxSpotToday(); }
    Size n() const override { return 1; }
    Size m() const override { return 1; }
    Size n_aux() const override { return 0; }
    Size m_aux() const override { return 0; }

    double volatility(const Time t, const Array& s) const override;

    Array marginalStep(const Time t0, const Array& x0, const Time dt, const Array& dw, const Real r_dom,
                       const Real r_for, const std::optional<Discretization> disc = std::nullopt) const override;

private:
    QuantLib::ext::shared_ptr<FxLvParametrization> parametrization_;
    Discretization disc_;
};

} // namespace QuantExt
