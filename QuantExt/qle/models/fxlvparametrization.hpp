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

/*! \file fxlvparametrization.hpp
    \brief FX LV parametrization
    \ingroup models
*/

#pragma once

#include <ql/handle.hpp>
#include <ql/termstructures/volatility/equityfx/localvoltermstructure.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <qle/models/parametrization.hpp>

namespace QuantExt {
using namespace QuantLib;

//! FX LV parametrization
class FxLvParametrization : public Parametrization {
public:
    /*! The currency refers to the foreign currency, the spot
        is as of today (i.e. the discounted spot) */
    FxLvParametrization(const Currency& foreignCurrency, const Handle<Quote>& fxSpotToday,
                        const QuantLib::ext::shared_ptr<QuantLib::LocalVolTermStructure>& lv);

    /*! is supposed to be positive */
    Real sigma(const Time t, const Real s) const;
    const Handle<Quote> fxSpotToday() const;

    Size numberOfParameters() const override { return 1; }

private:
    Handle<Quote> fxSpotToday_;
    QuantLib::ext::shared_ptr<LocalVolTermStructure> lv_;
};

// inline

inline Real FxLvParametrization::sigma(const Time t, const Real s) const { return lv_->localVol(t, s); }

inline const Handle<Quote> FxLvParametrization::fxSpotToday() const { return fxSpotToday_; }

} // namespace QuantExt
