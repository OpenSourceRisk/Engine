/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file ored/scripting/models/fdblackscholesbase.hpp
    \brief black scholes fd model base class for n underlyings (fx, equity or commodity)
    \ingroup models
*/

#pragma once

#include <ored/scripting/models/modelimpl.hpp>

#include <qle/termstructures/correlationtermstructure.hpp>
#include <qle/models/blackscholesmodelwrapper.hpp>

#include <ql/indexes/interestrateindex.hpp>
#include <ql/methods/finitedifferences/meshers/fdmmesher.hpp>
#include <ql/methods/finitedifferences/solvers/fdmbackwardsolver.hpp>
#include <ql/processes/blackscholesprocess.hpp>
#include <ql/timegrid.hpp>

namespace ore {
namespace data {

/* At the moment this is the FD Black Scholes model class, restricted to one underlying. TODOs:
   - extend to several underlyings,
   - cover both black scholes and local vol models
   - refactor with BlackScholesBase, there is quite a bit of code duplication */
class FdBlackScholesBase : public ModelImpl {
public:
    /* For the constructor arguments see BlackScholesBase (the corresponding MC model class), except
       - we don't have a regressionOrder and paths parameter here.
       - instead we have a stateGridPoints parameter and additional fd specific parameters
       - if staticMesher is true, the mesh will be held constant after its initial construction, this
         is important to get stable sensitivities
    */
    FdBlackScholesBase(
        const Size stateGridPoints, const std::vector<std::string>& currencies,
        const std::vector<Handle<YieldTermStructure>>& curves, const std::vector<Handle<Quote>>& fxSpots,
        const std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<InterestRateIndex>>>& irIndices,
        const std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<ZeroInflationIndex>>>& infIndices,
        const std::vector<std::string>& indices, const std::vector<std::string>& indexCurrencies,
        const std::set<std::string>& payCcys_, const Handle<BlackScholesModelWrapper>& model,
        const std::map<std::pair<std::string, std::string>, Handle<QuantExt::CorrelationTermStructure>>& correlations,
        const std::set<Date>& simulationDates, const IborFallbackConfig& iborFallbackConfig,
        const std::string& calibration, const std::map<std::string, std::vector<Real>>& calibrationStrikes = {},
        const Real mesherEpsilon = 1E-4, const Real mesherScaling = 1.5, const Real mesherConcentration = 0.1,
        const Size mesherMaxConcentratingPoints = 9999, const bool staticMesher = false);

    // ctor for single underlying
    FdBlackScholesBase(const Size stateGridPoints, const std::string& currency, const Handle<YieldTermStructure>& curve,
                       const std::string& index, const std::string& indexCurrency,
                       const Handle<BlackScholesModelWrapper>& model, const std::set<Date>& simulationDates,
                       const IborFallbackConfig& iborFallbackConfig, const std::string& calibration,
                       const std::vector<Real>& calibrationStrikes = {}, const Real mesherEpsilon = 1E-4,
                       const Real mesherScaling = 1.5, const Real mesherConcentration = 0.1,
                       const Size mesherMaxConcentratingPoints = 9999, const bool staticMesher = false);

    // Model interface implementation
    Type type() const override { return Type::FD; }
    const Date& referenceDate() const override;
    RandomVariable npv(const RandomVariable& amount, const Date& obsdate, const Filter& filter,
                       const boost::optional<long>& memSlot, const RandomVariable& addRegressor1,
                       const RandomVariable& addRegressor2) const override;
    RandomVariable fwdCompAvg(const bool isAvg, const std::string& index, const Date& obsdate, const Date& start,
                              const Date& end, const Real spread, const Real gearing, const Integer lookback,
                              const Natural rateCutoff, const Natural fixingDays, const bool includeSpread,
                              const Real cap, const Real floor, const bool nakedOption,
                              const bool localCapFloor) const override;
    void releaseMemory() override;
    Real extractT0Result(const RandomVariable& result) const override;

    // override to handle cases where we use a quanto-adjusted pde
    const std::string& baseCcy() const override;
    RandomVariable pay(const RandomVariable& amount, const Date& obsdate, const Date& paydate,
                       const std::string& currency) const override;

protected:
    // ModelImpl interface implementation (except initiModelState, this is done in the derived classes)
    void performCalculations() const override;
    RandomVariable getIndexValue(const Size indexNo, const Date& d, const Date& fwd = Null<Date>()) const override;
    RandomVariable getIrIndexValue(const Size indexNo, const Date& d, const Date& fwd = Null<Date>()) const override;
    RandomVariable getInfIndexValue(const Size indexNo, const Date& d, const Date& fwd = Null<Date>()) const override;
    RandomVariable getDiscount(const Size idx, const Date& s, const Date& t) const override;
    RandomVariable getNumeraire(const Date& s) const override;
    Real getFxSpot(const Size idx) const override;
    RandomVariable getFutureBarrierProb(const std::string& index, const Date& obsdate1, const Date& obsdate2,
                                        const RandomVariable& barrier, const bool above) const override;

    // helper function that constructs the correlation matrix
    Matrix getCorrelation() const;

    // input parameters
    const std::vector<Handle<YieldTermStructure>> curves_;
    const std::vector<Handle<Quote>> fxSpots_;
    const std::set<std::string> payCcys_;
    const Handle<BlackScholesModelWrapper> model_;
    const std::map<std::pair<std::string, std::string>, Handle<QuantExt::CorrelationTermStructure>> correlations_;
    const std::vector<Date> simulationDates_;
    const std::string calibration_;
    const std::map<std::string, std::vector<Real>> calibrationStrikes_;
    const Real mesherEpsilon_, mesherScaling_, mesherConcentration_;
    const Size mesherMaxConcentratingPoints_;
    const bool staticMesher_;

    // quanto adjustment parameters
    bool applyQuantoAdjustment_ = false;
    Size quantoSourceCcyIndex_, quantoTargetCcyIndex_;
    Real quantoCorrelationMultiplier_;

    // these are all initialised when the interface functions above are called
    mutable std::vector<std::function<RandomVariable(const std::vector<const RandomVariable*>&)>> basisFns_;
    mutable Date referenceDate_;                      // the model reference date
    mutable std::set<Date> effectiveSimulationDates_; // the dates effectively simulated (including today)
    mutable TimeGrid timeGrid_;                       // the (possibly refined) time grid for the FD solver
    mutable std::vector<Size> positionInTimeGrid_;    // for each effective simulation date the index in the time grid
    mutable QuantLib::ext::shared_ptr<FdmMesher> mesher_;     // the mesher for the FD solver
    mutable QuantLib::ext::shared_ptr<FdmLinearOpComposite> operator_; // the operator
    mutable QuantLib::ext::shared_ptr<FdmBackwardSolver> solver_;      // the sovler
    mutable RandomVariable underlyingValues_;                  // the discretised underlying
};

} // namespace data
} // namespace ore
