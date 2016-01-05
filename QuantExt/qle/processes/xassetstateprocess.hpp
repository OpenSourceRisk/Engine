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

/*! \file xassetstateprocess.hpp
    \brief xasset model state process
*/

#ifndef quantext_xasset_stateprocess_hpp
#define quantext_xasset_stateprocess_hpp

#include <ql/stochasticprocess.hpp>

using namespace QuantLib;

namespace QuantExt {

class XAssetModel;

class XAssetStateProcess : public StochasticProcess {
  public:
    enum discretization { exact, euler };

    XAssetStateProcess(const XAssetModel *const model, discretization disc);

    /*! StochasticProcess interface */
    Size size() const;
    Disposable<Array> initialValues() const;
    Disposable<Array> drift(Time t, const Array &x) const;
    Disposable<Matrix> diffusion(Time t, const Array &x) const;

    /* !specific members */
    void flushCache();

  private:
    const XAssetModel *const model_;

    class ExactDiscretization : public StochasticProcess::discretization {
      public:
        ExactDiscretization(const XAssetModel *const model);
        virtual Disposable<Array> drift(const StochasticProcess &, Time t0,
                                        const Array &x0, Time dt) const;
        virtual Disposable<Matrix> diffusion(const StochasticProcess &, Time t0,
                                             const Array &x0, Time dt) const;
        virtual Disposable<Matrix> covariance(const StochasticProcess &,
                                              Time t0, const Array &x0,
                                              Time dt) const;

      private:
        const XAssetModel *const model_;
    };

};

} // namesapce QuantExt

#endif
