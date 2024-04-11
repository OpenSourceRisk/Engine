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

/*! \file ored/scripting/models/blackscholesbase.hpp
    \brief black scholes model base class for n underlyings (fx, equity or commodity)
    \ingroup utilities
*/

#pragma once

#include <ored/scripting/models/modelimpl.hpp>

#include <qle/models/blackscholesmodelwrapper.hpp>
#include <qle/termstructures/correlationtermstructure.hpp>

#include <ql/indexes/interestrateindex.hpp>
#include <ql/processes/blackscholesprocess.hpp>
#include <ql/timegrid.hpp>

namespace ore {
namespace data {

/* This class is the basis for the BlackScholes and LocalVol model implementations */
class BlackScholesBase : public ModelImpl {
public:
    /* For the constructor arguments see ModelImpl, plus:
       - eq, com processes are given with arbitrary riskFreeRate() and dividendYield(), these two curves only define
         the forward curve drift for each asset
       - the base ccy is the first ccy in the currency vector, the fx spots are given as for-base, the ccy curves define
         the fx forwards
       - fx processes must be given w.r.t. the base ccy and consistent with the given fx spots and curves, but we do not
         require fx processes for all currencies (but they are required, if an fx index is evaluated in eval())
       - correlations are for index pair names and must be constant; if not given for a pair, we assume zero correlation
       - regressionOrder is the regression order used to compute conditional expectations in npv()
     */
    BlackScholesBase(
        const Size paths, const std::vector<std::string>& currencies,
        const std::vector<Handle<YieldTermStructure>>& curves, const std::vector<Handle<Quote>>& fxSpots,
        const std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<InterestRateIndex>>>& irIndices,
        const std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<ZeroInflationIndex>>>& infIndices,
        const std::vector<std::string>& indices, const std::vector<std::string>& indexCurrencies,
        const Handle<BlackScholesModelWrapper>& model,
        const std::map<std::pair<std::string, std::string>, Handle<QuantExt::CorrelationTermStructure>>& correlations,
        const McParams& mcParams, const std::set<Date>& simulationDates, const IborFallbackConfig& iborFallbackConfig);

    // ctor for single underlying
    BlackScholesBase(const Size paths, const std::string& currency, const Handle<YieldTermStructure>& curve,
                     const std::string& index, const std::string& indexCurrency,
                     const Handle<BlackScholesModelWrapper>& model, const Model::McParams& mcParams,
                     const std::set<Date>& simulationDates, const IborFallbackConfig& iborFallbackConfig);

    // Model interface implementation
    Type type() const override { return Type::MC; }
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
    void resetNPVMem() override;
    void toggleTrainingPaths() const override;
    Size trainingSamples() const override;
    Size size() const override;

protected:
    // ModelImpl interface implementation (except initiModelState, this is done in the derived classes)
    void performCalculations() const override;
    RandomVariable getIndexValue(const Size indexNo, const Date& d, const Date& fwd = Null<Date>()) const override;
    RandomVariable getIrIndexValue(const Size indexNo, const Date& d, const Date& fwd = Null<Date>()) const override;
    RandomVariable getInfIndexValue(const Size indexNo, const Date& d, const Date& fwd = Null<Date>()) const override;
    RandomVariable getDiscount(const Size idx, const Date& s, const Date& t) const override;
    RandomVariable getNumeraire(const Date& s) const override;
    Real getFxSpot(const Size idx) const override;

    // helper function that constructs the correlation matrix
    Matrix getCorrelation() const;

    // input parameters
    const std::vector<Handle<YieldTermStructure>> curves_;
    const std::vector<Handle<Quote>> fxSpots_;
    const Handle<BlackScholesModelWrapper> model_;
    const std::map<std::pair<std::string, std::string>, Handle<QuantExt::CorrelationTermStructure>> correlations_;
    const McParams mcParams_;
    const std::vector<Date> simulationDates_;

    // these all except underlyingPaths_ are initialised when the interface functions above are called
    mutable Date referenceDate_;                      // the model reference date
    mutable std::set<Date> effectiveSimulationDates_; // the dates effectively simulated (including today)
    mutable TimeGrid timeGrid_;                       // the (possibly refined) time grid for the simulation
    mutable std::vector<Size> positionInTimeGrid_;    // for each effective simulation date the index in the time grid
    mutable std::map<Date, std::vector<RandomVariable>> underlyingPaths_;         // per simulation date index states
    mutable std::map<Date, std::vector<RandomVariable>> underlyingPathsTraining_; // ditto (training phase)
    mutable bool inTrainingPhase_ = false; // are we currently using training paths?

    // stored regression coefficients
    mutable std::map<long, std::tuple<Array, Size, Matrix>> storedRegressionModel_;
};

} // namespace data
} // namespace ore
