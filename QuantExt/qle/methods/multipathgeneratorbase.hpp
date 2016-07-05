/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of OpenRiskEngine, a free-software/open-source library
 for transparent pricing and risk analysis - http://openriskengine.org

 OpenRiskEngine is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program; if not, please email
 <users@openriskengine.org>. The license is also available online at
 <http://openriskengine.org/license.shtml>.

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/


/*! \file multipathgeneratorbase.hpp
    \brief base class for multi path generators
    \ingroup 
*/

#ifndef quantext_multi_path_generator_base_hpp
#define quantext_multi_path_generator_base_hpp

#include <qle/methods/multipathgenerator.hpp>

#include <ql/math/randomnumbers/rngtraits.hpp>
#include <ql/methods/montecarlo/brownianbridge.hpp>
#include <ql/methods/montecarlo/multipath.hpp>
#include <ql/methods/montecarlo/sample.hpp>
#include <ql/stochasticprocess.hpp>

using namespace QuantLib;

namespace QuantExt {

    class MultiPathGeneratorBase {
    public:
        virtual const Sample<MultiPath> &next() const = 0;
        virtual void reset() = 0;
    };

    /*! Instantiation of MultiPathGenerator with standard PseudoRandom traits */
    class MultiPathGeneratorPseudoRandom : public MultiPathGeneratorBase {
    public:
        MultiPathGeneratorPseudoRandom(const boost::shared_ptr<StochasticProcess> &,
                                       const TimeGrid &, Size dimension,
                                       BigNatural seed = 0,
                                       bool antitheticSampling = false);
        const Sample<MultiPath> &next() const;
        void reset();

    private:
        const boost::shared_ptr<StochasticProcess> process_;
        TimeGrid grid_;
        Size dimension_;
        BigNatural seed_;

        boost::shared_ptr<MultiPathGenerator<PseudoRandom::rsg_type> > pg_;
        bool antitheticSampling_;
        mutable bool antitheticVariate_;
    };

    /*! Instantiatino of MultiPathGenerator with standard LowDiscrepancy traits,
    for the use of the seed, see ql/math/randomnumbers/sobolrsg.cpp */
    class MultiPathGeneratorLowDiscrepancy : public MultiPathGeneratorBase {
    public:
        MultiPathGeneratorLowDiscrepancy(
                      const boost::shared_ptr<StochasticProcess> &, const TimeGrid &,
                      Size dimension, BigNatural seed = 0, bool brownianBridge = false);
        const Sample<MultiPath> &next() const;
        void reset();

    private:
        const boost::shared_ptr<StochasticProcess> process_;
        TimeGrid grid_;
        Size dimension_;
        BigNatural seed_;
        bool brownianBridge_;

        boost::shared_ptr<MultiPathGenerator<LowDiscrepancy::rsg_type> > pg_;
    };

    /* add more instantiations here, when needed .... */

    // inline

    inline const Sample<MultiPath> &MultiPathGeneratorPseudoRandom::next() const {
        if (antitheticSampling_) {
            antitheticVariate_ = !antitheticVariate_;
            return antitheticVariate_ ? pg_->antithetic() : pg_->next();
        } else {
            return pg_->next();
        }
    }

    inline const Sample<MultiPath> &MultiPathGeneratorLowDiscrepancy::next() const {
        return pg_->next();
    }

} // namesapce QuantExt

#endif
