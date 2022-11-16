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

/*! \file commodityschwartzstateprocess.hpp
    \brief COM state process for the one-factor Schwartz model
    \ingroup processes
*/

#ifndef quantext_com_schwartz_stateprocess_hpp
#define quantext_com_schwartz_stateprocess_hpp

#include <ql/stochasticprocess.hpp>
#include <qle/models/commodityschwartzparametrization.hpp>
#include <qle/models/commodityschwartzmodel.hpp>

namespace QuantExt {
using namespace QuantLib;

//! COM Schwartz model one-factor state process
/*! \ingroup processes
 */
class CommoditySchwartzStateProcess : public StochasticProcess1D {
public:
    CommoditySchwartzStateProcess(const boost::shared_ptr<CommoditySchwartzParametrization>& parametrization,
                                  const CommoditySchwartzModel::Discretization discretization);
    //! \name StochasticProcess interface
    //@{
    Real x0() const override;
    Real drift(Time t, Real x) const override;
    Real diffusion(Time t, Real x) const override;
    Real expectation(Time t0, Real x0, Time dt) const override;
    Real stdDeviation(Time t0, Real x0, Time dt) const override;
    Real variance(Time t0, Real x0, Time dt) const override;
    //@}
private:
    const boost::shared_ptr<CommoditySchwartzParametrization> p_;
};

// inline

inline Real CommoditySchwartzStateProcess::x0() const { return 0.0; }


} // namespace QuantExt

#endif
