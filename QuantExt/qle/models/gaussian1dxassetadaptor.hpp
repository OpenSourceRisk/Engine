/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

/*! \file gaussian1dxassetadaptor.hpp
    \brief adaptor class that extracts one irlgm1f component
           from an xasset model as a gaussian 1d model
*/

#ifndef quantext_gaussian1d_xasset_adaptor_hpp
#define quantext_gaussian1d_xasset_adaptor_hpp

#include <qle/models/gaussian1dxassetadaptor.hpp>
#include <qle/models/xassetmodel.hpp>
#include <qle/models/gaussian1dmodel.hpp>

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

inline const Real
Gaussian1dXAssetAdaptor::zerobondImpl(const Time T, const Time t, const Real y,
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
Gaussian1dXAssetAdaptor::deflatedZerobondImpl(const Time T, const Time t, const Real y,
                     const Handle<YieldTermStructure> &yts,
                     const Handle<YieldTermStructure> &ytsNumeraire,
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
