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

/*! LGM 1f interest rate model
    Basically the same remarks as for XAssetModel hold */

class Lgm : public Observer, public Observable {

  public:
    Lgm(const boost::shared_ptr<IrLgm1fParametrization> &parametrization);

    const boost::shared_ptr<StochasticProcess1D> stateProcess() const;
    const boost::shared_ptr<IrLgm1fParametrization> parametrization() const;

    Real numeraire(const Time t, const Real x) const;
    Real discountBond(const Time t, const Time T, const Real x) const;
    Real reducedDiscountBond(const Time t, const Time T, const Real x) const;
    Real discountBondOption(Option::Type type, const Real K, const Time t,
                            const Time S, const Time T) const;

    void calibrateIrVolatilitiesIterative();

    /*! observer interface */
    void update();

  private:
    boost::shared_ptr<XAssetModel> x_;
    boost::shared_ptr<StochasticProcess1D> stateProcess_;
};

// inline

inline void Lgm::update() {
    x_->update();
    notifyObservers();
}

inline const boost::shared_ptr<StochasticProcess1D> Lgm::stateProcess() const {
    return stateProcess_;
}

inline const boost::shared_ptr<IrLgm1fParametrization>
Lgm::parametrization() const {
    return x_->irlgm1f(0);
}

inline Real Lgm::numeraire(const Time t, const Real x) const {
    return x_->numeraire(0, t, x);
}

inline Real Lgm::discountBond(const Time t, const Time T, const Real x) const {
    return x_->discountBond(0, t, T, x);
}

inline Real Lgm::reducedDiscountBond(const Time t, const Time T,
                                     const Real x) const {
    return x_->reducedDiscountBond(0, t, T, x);
}

inline Real Lgm::discountBondOption(Option::Type type, const Real K,
                                    const Time t, const Time S,
                                    const Time T) const {
    return x_->discountBondOption(0, type, K, t, S, T);
}

} // namespace QuantExt

#endif
