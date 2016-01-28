/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

/*! \file gaussian1dcrossassetadaptor.hpp
    \brief adaptor class that extracts one irlgm1f component
           from an crossasset model as a gaussian 1d model
*/

#ifndef quantext_gaussian1d_crossasset_adaptor_hpp
#define quantext_gaussian1d_crossasset_adaptor_hpp

#include <qle/models/gaussian1dcrossassetadaptor.hpp>
#include <qle/models/crossassetmodel.hpp>
#include <qle/models/gaussian1dmodel.hpp>

namespace QuantExt {

class Gaussian1dCrossAssetAdaptor : public Gaussian1dModel {
  public:
    Gaussian1dCrossAssetAdaptor(const boost::shared_ptr<LinearGaussMarkovModel> &model);
    Gaussian1dCrossAssetAdaptor(const Size ccy,
                            const boost::shared_ptr<CrossAssetModel> &model);

  private:
    /*! Gaussian1dModel interface */
    const Real numeraireImpl(const Time t, const Real y,
                             const Handle<YieldTermStructure> &yts) const;
    const Real zerobondImpl(const Time T, const Time t, const Real y,
                            const Handle<YieldTermStructure> &yts,
                            const bool adjusted) const;
    const Real
    deflatedZerobondImpl(const Time T, const Time t, const Real y,
                         const Handle<YieldTermStructure> &yts,
                         const Handle<YieldTermStructure> &ytsNumeraire,
                         const bool adjusted) const;

    bool preferDeflatedZerobond() const { return true; }

    /* helper functions */
    void initialize();

    /* members */
    const boost::shared_ptr<LinearGaussMarkovModel> x_;
};

// inline

inline const Real Gaussian1dCrossAssetAdaptor::numeraireImpl(
    const Time t, const Real y, const Handle<YieldTermStructure> &yts) const {
    Real d = yts.empty() ? 1.0
                         : x_->parametrization()->termStructure()->discount(t) /
                               yts->discount(t);
    Real x = y * std::sqrt(x_->parametrization()->zeta(t));
    return d * x_->numeraire(t, x);
}

inline const Real
Gaussian1dCrossAssetAdaptor::zerobondImpl(const Time T, const Time t, const Real y,
                                      const Handle<YieldTermStructure> &yts,
                                      const bool) const {
    Real d = yts.empty()
                 ? 1.0
                 : x_->parametrization()->termStructure()->discount(t) /
                       x_->parametrization()->termStructure()->discount(T) *
                       yts->discount(T) / yts->discount(t);
    Real x = y * std::sqrt(x_->parametrization()->zeta(t));
    return d * x_->discountBond(t, T, x);
}

inline const Real
Gaussian1dCrossAssetAdaptor::deflatedZerobondImpl(const Time T, const Time t, const Real y,
                     const Handle<YieldTermStructure> &yts,
                     const Handle<YieldTermStructure> &,
                     const bool) const {
    Real d = yts.empty()
                 ? 1.0
                 : x_->parametrization()->termStructure()->discount(t) /
                       x_->parametrization()->termStructure()->discount(T) *
                       yts->discount(T) / yts->discount(t);
    Real x = y * std::sqrt(x_->parametrization()->zeta(t));
    return d * x_->reducedDiscountBond(t, T, x);
}

} // QuantExt

#endif
