/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*! \file ored/scripting/models/model.hpp
    \brief interface for model against which a script can be run
    \ingroup utilities
*/

#pragma once

#include <map>
#include <ored/scripting/value.hpp>

#include <qle/models/modelbuilder.hpp>
#include <qle/methods/multipathgeneratorbase.hpp>

#include <ql/patterns/lazyobject.hpp>
#include <ql/settings.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <ql/methods/montecarlo/lsmbasissystem.hpp>

#include <boost/any.hpp>
#include <boost/enable_shared_from_this.hpp>

namespace ore {
namespace data {

using QuantLib::Date;

class Model : public LazyObject {
public:
    enum class Type { MC, FD };

    struct McParams {
        McParams() = default;
        Size seed = 42;
        Size trainingSeed = 43;
        Size trainingSamples = Null<Size>();
        QuantExt::SequenceType sequenceType = QuantExt::SequenceType::SobolBrownianBridge;
        QuantExt::SequenceType trainingSequenceType = QuantExt::SequenceType::MersenneTwister;
        bool externalDeviceCompatibilityMode = false;
        Size regressionOrder = 2;
        QuantLib::LsmBasisSystem::PolynomialType polynomType = QuantLib::LsmBasisSystem::PolynomialType::Monomial;
        QuantLib::SobolBrownianGenerator::Ordering sobolOrdering = QuantLib::SobolBrownianGenerator::Steps;
        QuantLib::SobolRsg::DirectionIntegers sobolDirectionIntegers = QuantLib::SobolRsg::DirectionIntegers::JoeKuoD7;
        QuantLib::Real regressionVarianceCutoff = Null<QuantLib::Real>();
    };

    explicit Model(const Size n) : n_(n) {}
    virtual ~Model() {}

    // model type
    virtual Type type() const = 0;

    // number of paths
    virtual Size size() const { return n_; }

    // if not null, this model uses a separate mc training phase for NPV() calcs
    virtual Size trainingSamples() const { return Null<Size>(); }

    /* enable / disable the usage of the training paths (if trainingPaths() is not null)
       the model should be using training paths only temporarily and reset to normal model via RAII */
    virtual void toggleTrainingPaths() const {}

    // the eval date
    virtual const Date& referenceDate() const = 0;

    // the base ccy of the model
    virtual const std::string& baseCcy() const = 0;

    // time between two dates d1 <= d2, default actact should be overriden in derived claases if appropriate
    virtual Real dt(const Date& d1, const Date& d2) const {
        return ActualActual(ActualActual::ISDA).yearFraction(d1, d2);
    }

    // time from reference date in this model
    Real timeFromReference(const Date& d) const { return dt(referenceDate(), d); }

    // result must be as of max(refdate, obsdate); refdate < paydate and obsdate <= paydate required
    virtual RandomVariable pay(const RandomVariable& amount, const Date& obsdate, const Date& paydate,
                               const std::string& currency) const = 0;

    // refdate <= obsdate <= paydate required
    virtual RandomVariable discount(const Date& obsdate, const Date& paydate, const std::string& currency) const = 0;

    // refdate <= obsdate required
    virtual RandomVariable npv(const RandomVariable& amount, const Date& obsdate, const Filter& filter,
                               const boost::optional<long>& memSlot, const RandomVariable& addRegressor1,
                               const RandomVariable& addRegressor2) const = 0;

    /* eval index at (past or future) obsdate:
       - if fwddate != null, fwddate > obsdate is required. A check must be implemented that the obsdate allows for
         the index projection. For non-inflation indices this check is simply obsdate >= refdate. For zero inflation
         indices the is is obsdate >= basedate where the basedate is the one from the zero inflation ts asociated
         to the index.
       - if a historical fixing is required and missing, the behaviour depends on returnMissingFixingAsNull, if this
         flag is true, an exception is thrown, if false, an RandomVariable rv with rv.initialised() = false is returned
       - for non-inflation indices, if ignoreTodaysFixing is true, always return the market spot for obsdate =
         referencedate, even if  a historical fixing is available; for inflation indices, ignore this flag
    */
    virtual RandomVariable eval(const std::string& index, const Date& obsdate, const Date& fwddate,
                                const bool returnMissingFixingAsNull = false,
                                const bool ignoreTodaysFixing = false) const = 0;

    // forward looking daily comp, obsdate <= start < end required, result must be as of max(refdate, obsdate)
    virtual RandomVariable fwdCompAvg(const bool isAvg, const std::string& index, const Date& obsdate,
                                      const Date& start, const Date& end, const Real spread, const Real gearing,
                                      const Integer lookback, const Natural rateCutoff, const Natural fixingDays,
                                      const bool includeSpread, const Real cap, const Real floor,
                                      const bool nakedOption, const bool localCapFloor) const = 0;

    // barrier hit probability, obsdate1 <= obsdate2 required, but refdate can lie anywhere w.r.t. obsdate1, 2
    virtual RandomVariable barrierProbability(const std::string& index, const Date& obsdate1, const Date& obsdate2,
                                              const RandomVariable& barrier, const bool above) const = 0;

    // get T0 fx spot
    virtual Real fxSpotT0(const std::string& forCcy, const std::string& domCcy) const = 0;

    // extract T0 result from random variable
    virtual Real extractT0Result(const RandomVariable& value) const = 0;

    /* Release memory allocated for caches (if applicable). This should _not_ notify observers of the model, since
       this would in particular trigger a recalculation of the scripted instrument pricing engine after each pricing
       when the memory is released, although the model's observables may not have changed. */
    virtual void releaseMemory() {}

    // reset stored NPV() regression coefficients (if applicable)
    virtual void resetNPVMem() {}

    // additional results provided by the model
    const std::map<std::string, boost::any>& additionalResults() const { return additionalResults_; }

protected:
    // default implementation lazy object interface
    void performCalculations() const override {}

    // map with additional results provided by this model instance
    mutable std::map<std::string, boost::any> additionalResults_;

private:
    // size of random variables within model
    const Size n_;
};

} // namespace data
} // namespace ore
