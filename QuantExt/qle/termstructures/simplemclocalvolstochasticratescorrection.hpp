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

/*! \file simplemclocalvolstochasticratescorrection.hpp
    \brief simple, monte-carlo based stochastic rates correction for local vol surface
*/

#pragma once

#include <qle/models/irmodel.hpp>
#include <qle/models/fxmodel.hpp>

#include <ql/termstructures/volatility/equityfx/localvoltermstructure.hpp>
#include <ql/handle.hpp>

namespace QuantExt {

class SimpleMcLocalVolStochasticRatesCorrection : public QuantLib::LocalVolTermStructure, public QuantLib::LazyObject {
public:
    /*! S should be linked to this local vol ts to facilitate the bootstrap of the correction */
    SimpleMcLocalVolStochasticRatesCorrection(
        QuantLib::Handle<QuantLib::LocalVolTermStructure> source, QuantLib::ext::shared_ptr<IrModel> r,
        QuantLib::ext::shared_ptr<IrModel> q, QuantLib::ext::shared_ptr<FxModel> S,
        std::function<QuantLib::Array(QuantLib::Real, QuantLib::Real)> dwGenerator);

private:
    void performCalculations() const override;
    void update() override;
    QuantLib::Volatility localVolImpl(Time t, Real strike) const override final;

    QuantLib::Handle<QuantLib::LocalVolTermStructure> source_;
    QuantLib::ext::shared_ptr<IrModel> r_;
    QuantLib::ext::shared_ptr<IrModel> q_;
    QuantLib::ext::shared_ptr<FxModel> S_;
    std::function<QuantLib::Array(QuantLib::Real, QuantLib::Real)> dwGenerator_;
};

} // namespace QuantExt
