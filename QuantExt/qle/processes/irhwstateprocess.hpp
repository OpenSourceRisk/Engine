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

/*! \file irhwstateprocess.hpp
    \brief ir hw model state process
    \ingroup processes
*/

#ifndef quantext_irhw_stateprocess_hpp
#define quantext_irhw_stateprocess_hpp

#include <qle/models/hwmodel.hpp>

#include <ql/processes/eulerdiscretization.hpp>
#include <ql/stochasticprocess.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Ir HW State Process
/*! \ingroup processes
 */
class IrHwStateProcess : public StochasticProcess {
public:
    IrHwStateProcess(const QuantLib::ext::shared_ptr<IrHwParametrization>& parametrization, const IrModel::Measure measure,
                     const HwModel::Discretization discretization = HwModel::Discretization::Euler,
                     const bool evaluateBankAccount = true)
        : StochasticProcess(discretization == HwModel::Discretization::Euler ? QuantLib::ext::make_shared<EulerDiscretization>()
                                                                             : nullptr),
          parametrization_(parametrization), measure_(measure), discretization_(discretization),
          evaluateBankAccount_(evaluateBankAccount) {
        QL_REQUIRE(measure_ == IrModel::Measure::BA, "IrHwStateProcess only supports measure BA");
        QL_REQUIRE(discretization_ == HwModel::Discretization::Euler,
                   "IrHwStateProcess only supports dicsretization Euler");
    }
    Size size() const override {
        return parametrization_->n() +
               (evaluateBankAccount_ && measure_ == IrModel::Measure::BA ? parametrization_->n() : 0);
    }
    Size factors() const override {
        return parametrization_->m() + (evaluateBankAccount_ && measure_ == IrModel::Measure::BA &&
                                                discretization_ == HwModel::Discretization::Exact
                                            ? parametrization_->m()
                                            : 0);
    }
    Array initialValues() const override;
    Array drift(Time t, const Array& s) const override;
    Matrix diffusion(Time t, const Array& s) const override;

private:
    QuantLib::ext::shared_ptr<IrHwParametrization> parametrization_;
    IrModel::Measure measure_;
    HwModel::Discretization discretization_;
    bool evaluateBankAccount_;
};

} // namespace QuantExt

#endif
