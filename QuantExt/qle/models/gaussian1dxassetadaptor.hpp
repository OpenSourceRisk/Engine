/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2015 Quaternion Risk Management

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

/*! \file gaussian1dxassetadaptor.hpp
    \brief adaptor class that extracts one irlgm1f component
           from an xasset model as a gaussian 1d model
*/

#ifndef quantext_gaussian1d_xasset_adaptor_hpp
#define quantext_gaussian1d_xasset_adaptor_hpp

#include <qle/models/gaussian1dxassetadaptor.hpp>
#include <qle/models/lgm.hpp>
#include <ql/models/shortrate/onefactormodels/gaussian1dmodel.hpp>

namespace QuantExt {

class Gaussian1dXAssetAdaptor : public Gaussian1dModel {
  public:
    Gaussian1dXAssetAdaptor(const boost::shared_ptr<Lgm> &model);
    Gaussian1dXAssetAdaptor(const Size ccy,
                            const boost::shared_ptr<XAssetModel> &model);

  private:
    /*! Gaussian1dModel interface */
    const Real numeraireImpl(const Time t, const Real y,
                             const Handle<YieldTermStructure> &yts) const;
    const Real zerobondImpl(const Time T, const Time t, const Real y,
                            const Handle<YieldTermStructure> &yts) const;
    /* helper functions */
    void initialize();
    /*! members */
    const boost::shared_ptr<Lgm> x_;
};

// inline

inline const Real Gaussian1dXAssetAdaptor::numeraireImpl(
    const Time t, const Real y, const Handle<YieldTermStructure> &yts) const {
    Real d = yts.empty() ? 1.0
                         : x_->parametrization()->termStructure()->discount(t) /
                               yts->discount(t);
    Real x = y * std::sqrt(x_->parametrization()->zeta(t));
    return d * x_->numeraire(t, x);
}

inline const Real Gaussian1dXAssetAdaptor::zerobondImpl(
    const Time T, const Time t, const Real y,
    const Handle<YieldTermStructure> &yts) const {
    Real d = yts.empty()
                 ? 1.0
                 : x_->parametrization()->termStructure()->discount(t) /
                       x_->parametrization()->termStructure()->discount(T) *
                       yts->discount(T) / yts->discount(t);
    Real x = y * std::sqrt(x_->parametrization()->zeta(t));
    return d * x_->discountBond(t, T, x);
}

} // QuantExt

#endif
