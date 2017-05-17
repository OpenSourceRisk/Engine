/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file gaussian1dcrossassetadaptor.hpp
    \brief adaptor class that extracts one irlgm1f component
    \ingroup models
*/

#ifndef quantext_gaussian1d_crossasset_adaptor_hpp
#define quantext_gaussian1d_crossasset_adaptor_hpp

#include <ql/models/shortrate/onefactormodels/gaussian1dmodel.hpp>
#include <qle/models/crossassetmodel.hpp>
#include <qle/models/gaussian1dcrossassetadaptor.hpp>

namespace QuantExt {

//! Gaussian 1d Cross Asset adaptor
/*! \ingroup models
 */
class Gaussian1dCrossAssetAdaptor : public Gaussian1dModel {
public:
    Gaussian1dCrossAssetAdaptor(const boost::shared_ptr<LinearGaussMarkovModel>& model);
    Gaussian1dCrossAssetAdaptor(const Size ccy, const boost::shared_ptr<CrossAssetModel>& model);

private:
    /*! Gaussian1dModel interface */
    Real numeraireImpl(const Time t, const Real y, const Handle<YieldTermStructure>& yts) const;
    Real zerobondImpl(const Time T, const Time t, const Real y, const Handle<YieldTermStructure>& yts) const;

    // add this when the preferdDeflatedZerobond is available in QuantLib

    // const Real
    // deflatedZerobondImpl(const Time T, const Time t, const Real y,
    //                      const Handle<YieldTermStructure> &yts,
    //                      const Handle<YieldTermStructure> &ytsNumeraire,
    //                      const bool adjusted) const;

    // add this when available in QuantLib

    // bool preferDeflatedZerobond() const { return true; }

    /* helper functions */
    void initialize();

    /* members */
    const boost::shared_ptr<LinearGaussMarkovModel> x_;
};

// inline

inline Real Gaussian1dCrossAssetAdaptor::numeraireImpl(const Time t, const Real y,
                                                       const Handle<YieldTermStructure>& yts) const {
    Real d = yts.empty() ? 1.0 : x_->parametrization()->termStructure()->discount(t) / yts->discount(t);
    Real x = y * std::sqrt(x_->parametrization()->zeta(t));
    return d * x_->numeraire(t, x);
}

inline Real Gaussian1dCrossAssetAdaptor::zerobondImpl(const Time T, const Time t, const Real y,
                                                      const Handle<YieldTermStructure>& yts) const {
    Real d = yts.empty()
                 ? 1.0
                 : x_->parametrization()->termStructure()->discount(t) /
                       x_->parametrization()->termStructure()->discount(T) * yts->discount(T) / yts->discount(t);
    Real x = y * std::sqrt(x_->parametrization()->zeta(t));
    return d * x_->discountBond(t, T, x);
}

// see above in the declaration

// inline const Real
// Gaussian1dCrossAssetAdaptor::deflatedZerobondImpl(const Time T, const Time t,
// const Real y,
//                      const Handle<YieldTermStructure> &yts,
//                      const Handle<YieldTermStructure> &,
//                      const bool) const {
//     Real d = yts.empty()
//                  ? 1.0
//                  : x_->parametrization()->termStructure()->discount(t) /
//                        x_->parametrization()->termStructure()->discount(T) *
//                        yts->discount(T) / yts->discount(t);
//     Real x = y * std::sqrt(x_->parametrization()->zeta(t));
//     return d * x_->reducedDiscountBond(t, T, x);
// }

} // namespace QuantExt

#endif
