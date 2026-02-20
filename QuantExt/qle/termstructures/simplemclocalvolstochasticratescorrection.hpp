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

#include <qle/models/fxmodel.hpp>
#include <qle/models/irmodel.hpp>

#include <ql/handle.hpp>
#include <ql/math/interpolations/interpolation2d.hpp>
#include <ql/termstructures/volatility/equityfx/blackvoltermstructure.hpp>
#include <ql/termstructures/volatility/equityfx/localvoltermstructure.hpp>
#include <ql/timegrid.hpp>

namespace QuantExt {

class SimpleMcLocalVolStochasticRatesCorrection : public QuantLib::LocalVolTermStructure, public QuantLib::LazyObject {
public:
    /*! - the input model S should be linked to this local vol ts to facilitate the bootstrap of the correction
        - min, max strike are given in log moneyness log K / F */
    SimpleMcLocalVolStochasticRatesCorrection(
        QuantLib::Handle<QuantLib::LocalVolTermStructure> source, QuantLib::ext::shared_ptr<IrModel> r,
        QuantLib::ext::shared_ptr<IrModel> q, QuantLib::ext::shared_ptr<FxModel> S,
        QuantLib::Handle<QuantLib::YieldTermStructure> r0, QuantLib::Handle<QuantLib::YieldTermStructure> q0,
        Matrix correlation_r_q_S, Real maxTime = 10.0, Real minStrike = -2.5, Real maxStrike = 2.5,
        Size timeStepsPerYear = 24, Size nStrikes = 100, Size nPaths = 10000, Real d2CdK2Threshold = 0.01);

    Date maxDate() const override { return source_->maxDate(); }
    Real minStrike() const override { return source_->minStrike(); }
    Real maxStrike() const override { return source_->maxStrike(); }
    Calendar calendar() const override { return source_->calendar(); }
    DayCounter dayCounter() const override { return source_->dayCounter(); }
    Natural settlementDays() const override { return source_->settlementDays(); }
    const Date& referenceDate() const override { return source_->referenceDate(); }

private:
    void performCalculations() const override;
    void update() override;
    QuantLib::Volatility localVolImpl(Time t, Real strike) const override final;

    QuantLib::Handle<QuantLib::LocalVolTermStructure> source_;
    QuantLib::ext::shared_ptr<IrModel> r_;
    QuantLib::ext::shared_ptr<IrModel> q_;
    QuantLib::ext::shared_ptr<FxModel> S_;
    QuantLib::Handle<QuantLib::YieldTermStructure> r0_;
    QuantLib::Handle<QuantLib::YieldTermStructure> q0_;
    Matrix correlation_r_q_S_;
    Real maxTime_;
    Real minStrike_;
    Real maxStrike_;
    Size timeStepsPerYear_;
    Size nStrikes_;
    Size nPaths_;
    Real d2CdK2Threshold_;

    mutable Matrix correctionData_;
    mutable TimeGrid timeGrid_;
    mutable std::vector<Real> logStrikes_;
    mutable std::unique_ptr<Interpolation2D> correction_;
    mutable bool applyCorrection_ = true;
};

} // namespace QuantExt
