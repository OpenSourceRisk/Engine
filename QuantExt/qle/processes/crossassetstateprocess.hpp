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
    CrossAssetStateProcess(QuantLib::ext::shared_ptr<const CrossAssetModel> model);

    /*! StochasticProcess interface */
    Size size() const override;
    Size factors() const override;
    Array initialValues() const override;
    Array drift(Time t, const Array& x) const override;
    Matrix diffusion(Time t, const Array& x) const override;
    Array evolve(Time t0, const Array& x0, Time dt, const Array& dw) const override;

    // enables and resets the cache, once enabled the simulated times must stay the stame
    void resetCache(const Size timeSteps) const;

protected:
    virtual Matrix diffusionOnCorrelatedBrownians(Time t, const Array& x) const;
    virtual Matrix diffusionOnCorrelatedBrowniansImpl(Time t, const Array& x) const;
    void updateSqrtCorrelation() const;

    QuantLib::ext::shared_ptr<const CrossAssetModel> model_;

    std::vector<QuantLib::ext::shared_ptr<StochasticProcess>> crCirpp_;
    Size cirppCount_;

    mutable Matrix sqrtCorrelation_;

    class ExactDiscretization : public StochasticProcess::discretization {
    public:
        ExactDiscretization(QuantLib::ext::shared_ptr<const CrossAssetModel> model,
                            SalvagingAlgorithm::Type salvaging = SalvagingAlgorithm::Spectral);
        virtual Array drift(const StochasticProcess&, Time t0, const Array& x0, Time dt) const override;
        virtual Matrix diffusion(const StochasticProcess&, Time t0, const Array& x0, Time dt) const override;
        virtual Matrix covariance(const StochasticProcess&, Time t0, const Array& x0, Time dt) const override;
        void resetCache(const Size timeSteps) const;

    protected:
        virtual Array driftImpl1(const StochasticProcess&, Time t0, const Array& x0, Time dt) const;
        virtual Array driftImpl2(const StochasticProcess&, Time t0, const Array& x0, Time dt) const;
        virtual Matrix covarianceImpl(const StochasticProcess&, Time t0, const Array& x0, Time dt) const;

        QuantLib::ext::shared_ptr<const CrossAssetModel> model_;
        SalvagingAlgorithm::Type salvaging_;

        mutable bool cacheNotReady_m_ = true;
        mutable bool cacheNotReady_d_ = true;
        mutable bool cacheNotReady_v_ = true;
        mutable Size timeStepsToCache_m_ = 0;
        mutable Size timeStepsToCache_d_ = 0;
        mutable Size timeStepsToCache_v_ = 0;
        mutable Size timeStepCache_m_ = 0;
        mutable Size timeStepCache_d_ = 0;
        mutable Size timeStepCache_v_ = 0;
        mutable std::vector<Array> cache_m_;
        mutable std::vector<Matrix> cache_v_, cache_d_;
    }; // ExactDiscretization

    mutable bool cacheNotReady_m_ = true;
    mutable bool cacheNotReady_d_ = true;
    mutable Size timeStepsToCache_m_ = 0;
    mutable Size timeStepCache_m_ = 0;
    mutable Size timeStepsToCache_d_ = 0;
    mutable Size timeStepCache_d_ = 0;
    mutable std::vector<Array> cache_m_;
    mutable std::vector<Matrix> cache_d_;
}; // CrossAssetStateProcess

} // namespace QuantExt

#endif
