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

/*! \file crossassetstateprocess.hpp
    \brief crossasset model state process
    \ingroup crossassetmodel
    \ingroup processes
*/

#ifndef quantext_crossasset_stateprocess_hpp
#define quantext_crossasset_stateprocess_hpp

#include <ql/math/matrixutilities/pseudosqrt.hpp>
#include <ql/stochasticprocess.hpp>

#include <boost/unordered_map.hpp>

namespace QuantExt {
using namespace QuantLib;

class CrossAssetModel;

//! Cross Asset Model State Process
/*! \ingroup crossassetmodel
 \ingroup processes
 */
class CrossAssetStateProcess : public StochasticProcess {
public:
    CrossAssetStateProcess(const CrossAssetModel* const model);

    /*! StochasticProcess interface */
    Size size() const override;
    Size factors() const override;
    Array initialValues() const override;
    Array drift(Time t, const Array& x) const override;
    Matrix diffusion(Time t, const Array& x) const override;
    Array evolve(Time t0, const Array& x0, Time dt, const Array& dw) const override;

    /*! specific members */
    virtual void flushCache() const;

protected:
    virtual Matrix diffusionOnCorrelatedBrownians(Time t, const Array& x) const;
    virtual Matrix diffusionOnCorrelatedBrowniansImpl(Time t, const Array& x) const;
    void updateSqrtCorrelation() const;

    const CrossAssetModel* const model_;

    std::vector<boost::shared_ptr<StochasticProcess>> crCirpp_;
    Size cirppCount_;

    mutable Matrix sqrtCorrelation_;

    class ExactDiscretization : public StochasticProcess::discretization {
    public:
        ExactDiscretization(const CrossAssetModel* const model,
                            SalvagingAlgorithm::Type salvaging = SalvagingAlgorithm::Spectral);
        virtual Array drift(const StochasticProcess&, Time t0, const Array& x0, Time dt) const override;
        virtual Matrix diffusion(const StochasticProcess&, Time t0, const Array& x0, Time dt) const override;
        virtual Matrix covariance(const StochasticProcess&, Time t0, const Array& x0, Time dt) const override;
        void flushCache() const;

    protected:
        virtual Array driftImpl1(const StochasticProcess&, Time t0, const Array& x0, Time dt) const;
        virtual Array driftImpl2(const StochasticProcess&, Time t0, const Array& x0, Time dt) const;
        virtual Matrix covarianceImpl(const StochasticProcess&, Time t0, const Array& x0, Time dt) const;

        const CrossAssetModel* const model_;
        SalvagingAlgorithm::Type salvaging_;

        // cache for exact discretization
        struct cache_key {
            double t0, dt;
            bool operator==(const cache_key& o) const { return (t0 == o.t0) && (dt == o.dt); }
        };

        struct cache_hasher {
            std::size_t operator()(cache_key const& x) const {
                std::size_t seed = 0;
                boost::hash_combine(seed, x.t0);
                boost::hash_combine(seed, x.dt);
                return seed;
            }
        };
        mutable boost::unordered_map<cache_key, Array, cache_hasher> cache_m_;
        mutable boost::unordered_map<cache_key, Matrix, cache_hasher> cache_v_, cache_d_;
    }; // ExactDiscretization

    // cache for process drift and diffusion (e.g. used in Euler discretization)
    struct cache_hasher {
        std::size_t operator()(double const& x) const {
            std::size_t seed = 0;
            boost::hash_combine(seed, x);
            return seed;
        }
    };

    mutable boost::unordered_map<double, Array, cache_hasher> cache_m_;
    mutable boost::unordered_map<double, Matrix, cache_hasher> cache_d_;
}; // CrossAssetStateProcess

} // namespace QuantExt

#endif
