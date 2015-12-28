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

/*! \file xassetmodel.hpp
    \brief cross asset model
*/

#ifndef quantext_xasset_model_hpp
#define quantext_xasset_model_hpp

#include <qle/models/parametrization.hpp>
#include <ql/math/matrix.hpp>
#include <ql/models/model.hpp>
#include <ql/stochasticprocess.hpp>

using namespace QuantLib;

namespace QuantExt {

/*! The model is operated under the domestic LGM measure

    There are two ways of calibrating the model:

    - provide a calibrated parametrization for a component 
      extracted from some external model
    - do the calibration within the XAssetModel using one
      of the calibration procedures

    The inter-parametrization correlation matrix specified
    here can not be calibrated currently, but is a fixed
    external input. */

class XAssetModel : public CalibratedModel {
  public:
    /*! Parametrizations must be given in the following order
        - IR (first parametrization defines the domestic currency)
        - FX (for all pairs domestic-ccy defined by the IR models)
        - INF (optionally, ccy must be a subset of the IR ccys)
        - CRD (optionally, ccy must be a subset of the IR ccys)
        - COM (optionally) ccy must be a subset of the IR ccys) */
    XAssetModel(const boost::shared_ptr<Parametrization> parametrizations,
                const Matrix &correlation);
    
    /*! allow for time dependent correlation (second constructor) ? */

    /*! inspectors */
    boost::shared_ptr<StochasticProcess> stateProcess() const;

    /*! LGM1F components */
    Real numeraire(const Time t, const Size ccy = 0);
    Real discountBond(const Size ccy, const Real x, const Time t, const Time T);
    Real reducedDiscountBond(const Size ccy, const Real x, const Time t,
                             const Time T);
    Real zeroBondOption(const Size ccy, const Time t, const Time S,
                        const Time T, const Real K, Option::Type type);

    /*! other components */

    /*! calibration procedures */
    void calibrateIrAlphasIterative();
    /* ... */
    
};

} // namespace QuantExt

#endif
