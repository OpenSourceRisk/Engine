/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

/*! \file crossassetstateprocess.hpp
    \brief crossasset model state process
*/

#ifndef quantext_crossasset_stateprocess_hpp
#define quantext_crossasset_stateprocess_hpp

#include <ql/stochasticprocess.hpp>
#include <ql/math/matrixutilities/pseudosqrt.hpp>

#include <boost/unordered_map.hpp>

using namespace QuantLib;

namespace QuantExt {

class CrossAssetModel;

class CrossAssetStateProcess : public StochasticProcess {
  public:
    enum discretization { exact, euler };

    CrossAssetStateProcess(
        const CrossAssetModel *const model, discretization disc,
        SalvagingAlgorithm::Type salvaging = SalvagingAlgorithm::Spectral);

    /*! StochasticProcess interface */
    Size size() const;
    Disposable<Array> initialValues() const;
    Disposable<Array> drift(Time t, const Array &x) const;
    Disposable<Matrix> diffusion(Time t, const Array &x) const;

    /*! specific members */
    void flushCache() const;

  protected:
    virtual Disposable<Matrix> diffusionImpl(Time t, const Array &x) const;

    const CrossAssetModel *const model_;
    SalvagingAlgorithm::Type salvaging_;

    class ExactDiscretization : public StochasticProcess::discretization {
      public:
        ExactDiscretization(
            const CrossAssetModel *const model,
            SalvagingAlgorithm::Type salvaging = SalvagingAlgorithm::Spectral);
        virtual Disposable<Array> drift(const StochasticProcess &, Time t0,
                                        const Array &x0, Time dt) const;
        virtual Disposable<Matrix> diffusion(const StochasticProcess &, Time t0,
                                             const Array &x0, Time dt) const;
        virtual Disposable<Matrix> covariance(const StochasticProcess &,
                                              Time t0, const Array &x0,
                                              Time dt) const;
        void flushCache() const;

      protected:
        virtual Disposable<Array> driftImpl1(const StochasticProcess &, Time t0,
                                             const Array &x0, Time dt) const;
        virtual Disposable<Array> driftImpl2(const StochasticProcess &, Time t0,
                                             const Array &x0, Time dt) const;
        virtual Disposable<Matrix> covarianceImpl(const StochasticProcess &, Time t0,
                                                  const Array &x0, Time dt) const;

        const CrossAssetModel *const model_;
        SalvagingAlgorithm::Type salvaging_;

        // cache for exact discretization
        struct cache_key {
            double t0, dt;
            bool operator==(const cache_key &o) const {
                return (t0 == o.t0) && (dt == o.dt);
            }
        };

        struct cache_hasher : std::unary_function<cache_key, std::size_t> {
            std::size_t operator()(cache_key const &x) const {
                std::size_t seed = 0;
                boost::hash_combine(seed, x.t0);
                boost::hash_combine(seed, x.dt);
                return seed;
            }
        };
        mutable boost::unordered_map<cache_key, Array, cache_hasher> cache_m_;
        mutable boost::unordered_map<cache_key, Matrix, cache_hasher> cache_v_,
            cache_d_;
    }; // ExactDiscretization

    // cache for process drift and diffusion (e.g. used in Euler discretization)
    struct cache_hasher : std::unary_function<double, std::size_t> {
        std::size_t operator()(double const &x) const {
            std::size_t seed = 0;
            boost::hash_combine(seed, x);
            return seed;
        }
    };

    mutable boost::unordered_map<double, Array, cache_hasher> cache_m_;
    mutable boost::unordered_map<double, Matrix, cache_hasher> cache_v_,
        cache_d_;
}; // CrossAssetStateProcess

} // namesapce QuantExt

#endif
