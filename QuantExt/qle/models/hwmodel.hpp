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

/*! \file hwmodel.hpp
    \brief hull white n Factor model class
    \ingroup models
*/

#pragma once

#include <qle/models/hwparametrization.hpp>
#include <qle/models/irmodel.hpp>

#include <ql/math/comparison.hpp>

namespace QuantExt {

class HwModel : public IrModel {
public:
    enum class Discretization { Euler, Exact };

    HwModel(const QuantLib::ext::shared_ptr<IrHwParametrization>& parametrization, const Measure measure = Measure::BA,
            const Discretization discretization = Discretization::Euler, const bool evaluateBankAccount = true);

    // IrModel interface

    Measure measure() const override { return measure_; }

    const QuantLib::ext::shared_ptr<Parametrization> parametrizationBase() const override { return parametrization_; }

    Handle<YieldTermStructure> termStructure() const override { return parametrization_->termStructure(); }

    Size n() const override;
    Size m() const override;
    Size n_aux() const override;
    Size m_aux() const override;

    QuantLib::ext::shared_ptr<StochasticProcess> stateProcess() const override { return stateProcess_; }

    QuantLib::Real discountBond(const QuantLib::Time t, const QuantLib::Time T, const QuantLib::Array& x,
                                const QuantLib::Handle<QuantLib::YieldTermStructure>& discountCurve =
                                    Handle<YieldTermStructure>()) const override;

    QuantLib::Real
    numeraire(const QuantLib::Time t, const QuantLib::Array& x,
              const QuantLib::Handle<QuantLib::YieldTermStructure>& discountCurve = Handle<YieldTermStructure>(),
              const QuantLib::Array& aux = Array()) const override;

    QuantLib::Real shortRate(const QuantLib::Time t, const QuantLib::Array& x,
                             const QuantLib::Handle<QuantLib::YieldTermStructure>& discountCurve =
                                 Handle<YieldTermStructure>()) const override;

    // HwModel specific methods

    const QuantLib::ext::shared_ptr<IrHwParametrization> parametrization() const { return parametrization_; }

    /*! observer and linked calibrated model interface */
    void update() override;
    void generateArguments() override;

private:
    QuantLib::ext::shared_ptr<IrHwParametrization> parametrization_;
    Measure measure_;
    Discretization discretization_;
    QuantLib::ext::shared_ptr<StochasticProcess> stateProcess_;
    bool evaluateBankAccount_;
};

} // namespace QuantExt
