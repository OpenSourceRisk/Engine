/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

/*! \file lgm.hpp
    \brief lgm model class
*/

#ifndef quantext_lgm_model_hpp
#define quantext_lgm_model_hpp

#include <qle/models/xassetmodel.hpp>

namespace QuantExt {

class Lgm : public XAssetModel {

  public:
    Lgm(const boost::shared_ptr<IrLgm1fParametrization> &parametrization);

    boost::shared_ptr<StochasticProcess1D> stateProcess() const;
    boost::shared_ptr<IrLgm1fParametrization> parametrization() const;

    Real numeraire(const Time t, const Real x) const;
    Real discountBond(const Time t, const Time T, const Real x) const;
    Real reducedDiscountBond(const Time t, const Time T, const Real x) const;
    Real discountBondOption(Option::Type type, const Real K, const Time t,
                            const Time S, const Time T) const;

    void calibrateIrVolatilitiesIterative();

  private:
    boost::shared_ptr<StochasticProcess1D> stateProcess_;
};

// inline

inline boost::shared_ptr<StochasticProcess1D> Lgm::stateProcess() const {
    return stateProcess_;
}

inline boost::shared_ptr<IrLgm1fParametrization> Lgm::parametrization() const {
    return XAssetModel::irlgm1f(0);
}

inline Real Lgm::numeraire(const Time t, const Real x) const {
    return XAssetModel::numeraire(0, t, x);
}

inline Real Lgm::discountBond(const Time t, const Time T, const Real x) const {
    return XAssetModel::discountBond(0, t, T, x);
}

inline Real Lgm::reducedDiscountBond(const Time t, const Time T,
                                     const Real x) const {
    return XAssetModel::reducedDiscountBond(0, t, T, x);
}

inline Real Lgm::discountBondOption(Option::Type type, const Real K,
                                    const Time t, const Time S,
                                    const Time T) const {
    return XAssetModel::discountBondOption(0, type, K, t, S, T);
}

} // namespace QuantExt

#endif
