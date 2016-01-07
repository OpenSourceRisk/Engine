/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
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

    /*! specific members */
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
