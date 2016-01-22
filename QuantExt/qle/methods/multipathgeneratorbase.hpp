/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

/*! \file multipathgeneratorbase.hpp
    \brief base class for multi path generators
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
    virtual const Sample<MultiPath> &antithetic() const = 0;
};

class MultiPathGeneratorPseudoRandom : public MultiPathGeneratorBase {
  public:
    MultiPathGeneratorPseudoRandom(const boost::shared_ptr<StochasticProcess> &,
                                   const TimeGrid &, Size dimension,
                                   BigNatural seed,
                                   bool brownianBridge = false);
    const Sample<MultiPath> &next() const;
    const Sample<MultiPath> &antithetic() const;

  private:
    boost::shared_ptr<MultiPathGenerator<PseudoRandom::rsg_type> > pg_;
};

class MultiPathGeneratorLowDiscrepancy : public MultiPathGeneratorBase {
  public:
    MultiPathGeneratorLowDiscrepancy(
        const boost::shared_ptr<StochasticProcess> &, const TimeGrid &,
        Size dimension, BigNatural seed, bool brownianBridge = false);
    const Sample<MultiPath> &next() const;
    const Sample<MultiPath> &antithetic() const;

  private:
    boost::shared_ptr<MultiPathGenerator<LowDiscrepancy::rsg_type> > pg_;
};

// inline

const Sample<MultiPath> &MultiPathGeneratorPseudoRandom::next() const {
    return pg_->next();
}

const Sample<MultiPath> &MultiPathGeneratorPseudoRandom::antithetic() const {
    return pg_->antithetic();
}

const Sample<MultiPath> &MultiPathGeneratorLowDiscrepancy::next() const {
    return pg_->next();
}

const Sample<MultiPath> &MultiPathGeneratorLowDiscrepancy::antithetic() const {
    return pg_->antithetic();
}


} // namesapce QuantExt

#endif
