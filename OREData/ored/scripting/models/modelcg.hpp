/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

/*! \file ored/scripting/models/modelcg.hpp
    \brief interface for model against which a script can be run
    \ingroup utilities
*/

#pragma once

#include <qle/math/randomvariable.hpp>
#include <qle/math/randomvariable_ops.hpp>

#include <ql/patterns/lazyobject.hpp>
#include <ql/settings.hpp>
#include <ql/time/daycounters/actualactual.hpp>

#include <boost/any.hpp>

namespace QuantExt {
class ComputationGraph;
}

namespace ore {
namespace data {

using QuantLib::Date;
using QuantLib::Real;
using QuantLib::Integer;
using QuantLib::Natural;
using QuantLib::Size;

class ModelCG : public QuantLib::LazyObject {
public:
    enum class Type { MC, FD };

    explicit ModelCG(const QuantLib::Size n);
    virtual ~ModelCG() {}

    // computation graph
    QuantLib::ext::shared_ptr<QuantExt::ComputationGraph> computationGraph() { return g_; }

    // model type
    virtual Type type() const = 0;

    // number of paths
    virtual QuantLib::Size size() const { return n_; }

    // if not null, this model uses a separate mc training phase for NPV() calcs
    virtual Size trainingSamples() const { return QuantLib::Null<Size>(); }

    /* enable / disable the usage of the training paths (if trainingPaths() is not null)
       the model should be using training paths only temporarily and reset to normal model via RAII */
    virtual void toggleTrainingPaths() const {}

    // the eval date
    virtual const Date& referenceDate() const = 0;

    // the base ccy of the model
    virtual const std::string& baseCcy() const = 0;

    // time between two dates d1 <= d2, default actact should be overriden in derived claases if appropriate
    virtual std::size_t dt(const Date& d1, const Date& d2) const;

    // result must be as of max(refdate, obsdate); refdate < paydate and obsdate <= paydate required
    virtual std::size_t pay(const std::size_t amount, const Date& obsdate, const Date& paydate,
                            const std::string& currency) const = 0;

    // refdate <= obsdate <= paydate required
    virtual std::size_t discount(const Date& obsdate, const Date& paydate, const std::string& currency) const = 0;

    // refdate <= obsdate required
    virtual std::size_t npv(const std::size_t amount, const Date& obsdate, const std::size_t filter,
                            const boost::optional<long>& memSlot, const std::size_t addRegressor1,
                            const std::size_t addRegressor2) const = 0;

    /* eval index at (past or future) obsdate:
       - if fwddate != null, fwddate > obsdate is required. A check must be implemented that the obsdate allows for
         the index projection. For non-inflation indices this check is simply obsdate >= refdate. For zero inflation
         indices the is is obsdate >= basedate where the basedate is the one from the zero inflation ts asociated
         to the index.
       - if a historical fixing is required and missing, the behaviour depends on returnMissingFixingAsNull, if this
         flag is true, an exception is thrown, if false, the node "nan" is returned
       - for non-inflation indices, if ignoreTodaysFixing is true, always return the market spot for obsdate =
         referencedate, even if  a historical fixing is available; for inflation indices, ignore this flag
    */
    virtual std::size_t eval(const std::string& index, const Date& obsdate, const Date& fwddate,
                             const bool returnMissingFixingAsNull = false,
                             const bool ignoreTodaysFixing = false) const = 0;

    // forward looking daily comp/avg, obsdate <= start < end required, result must be as of max(refdate, obsdate)
    virtual std::size_t fwdCompAvg(const bool isAvg, const std::string& index, const Date& obsdate, const Date& start,
                                   const Date& end, const Real spread, const Real gearing, const Integer lookback,
                                   const Natural rateCutoff, const Natural fixingDays, const bool includeSpread,
                                   const Real cap, const Real floor, const bool nakedOption,
                                   const bool localCapFloor) const = 0;

    // barrier hit probability, obsdate1 <= obsdate2 required, but refdate can lie anywhere w.r.t. obsdate1, 2
    virtual std::size_t barrierProbability(const std::string& index, const Date& obsdate1, const Date& obsdate2,
                                           const std::size_t barrier, const bool above) const = 0;

    // get T0 fx spot
    virtual std::size_t fxSpotT0(const std::string& forCcy, const std::string& domCcy) const = 0;

    // extract T0 result from random variable
    virtual Real extractT0Result(const QuantExt::RandomVariable& value) const = 0;

    // reset stored NPV() regression coefficients (if applicable)
    virtual void resetNPVMem() {}

    // additional results provided by the model
    const std::map<std::string, boost::any>& additionalResults() const { return additionalResults_; }

    // CG / AD part of the interface
    virtual std::size_t cgVersion() const = 0;
    virtual const std::vector<std::vector<std::size_t>>& randomVariates() const = 0; // dim / steps
    virtual std::vector<std::pair<std::size_t, double>> modelParameters() const = 0;
    virtual std::vector<std::pair<std::size_t, std::function<double(void)>>>& modelParameterFunctors() const = 0;

    // get fx spot as of today directly, i.e. bypassing the cg
    virtual Real getDirectFxSpotT0(const std::string& forCcy, const std::string& domCcy) const = 0;

    // get discount as of today directly, i.e. bypassing the cg
    virtual Real getDirectDiscountT0(const Date& paydate, const std::string& currency) const = 0;

    // calculate the model
    void calculate() const override { LazyObject::calculate(); }

protected:
    // map with additional results provided by this model instance
    mutable std::map<std::string, boost::any> additionalResults_;

    // the underlying computation graph
    QuantLib::ext::shared_ptr<QuantExt::ComputationGraph> g_;

private:
    void performCalculations() const override {}

    // size of random variables within model
    const QuantLib::Size n_;
};

} // namespace data
} // namespace ore
