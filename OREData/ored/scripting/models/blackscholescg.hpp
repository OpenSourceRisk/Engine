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

/*! \file ored/scripting/models/blackscholescgbase.hpp
    \brief black scholes model base class for n underlyings (fx, equity or commodity)
    \ingroup utilities
*/

#pragma once

#include <ored/scripting/models/model.hpp>
#include <ored/scripting/models/modelcgimpl.hpp>

#include <qle/models/blackscholesmodelwrapper.hpp>
#include <qle/termstructures/correlationtermstructure.hpp>

#include <ql/indexes/interestrateindex.hpp>
#include <ql/processes/blackscholesprocess.hpp>
#include <ql/timegrid.hpp>

namespace ore {
namespace data {

/* This class is the basis for the BlackScholes and LocalVol model implementations */
class BlackScholesCG : public ModelCGImpl {
public:
    /* For the constructor arguments see ModelCGImpl, plus:
       - processes: hold spot, rate and div ts and vol for each given index
       - we assume that the given correlations are constant and read the value only at t = 0
       - calibration strikes are given as a map indexName => strike, if an index is missing in this map, the calibration
         strike will be atmf
       - eq, com processes are given with arbitrary riskFreeRate() and dividendYield(), these two curves only define
         the forward curve drift for each asset
       - the base ccy is the first ccy in the currency vector, the fx spots are given as for-base, the ccy curves define
         the fx forwards
       - fx processes must be given w.r.t. the base ccy and consistent with the given fx spots and curves, but we do not
         require fx processes for all currencies (but they are required, if an fx index is evaluated in eval())
       - correlations are for index pair names and must be constant; if not given for a pair, we assume zero correlation
       - regressionOrder is the regression order used to compute conditional expectations in npv()
       - processes: hold spot, rate and div ts and vol for each given index
       - we assume that the given correlations are constant and read the value only at t = 0
       - calibration strikes are given as a map indexName => strike, if an index is missing in this map, the calibration
         strike will be atmf
    */
    BlackScholesCG(
        const ModelCG::Type type, const Size paths, const std::vector<std::string>& currencies,
        const std::vector<Handle<YieldTermStructure>>& curves, const std::vector<Handle<Quote>>& fxSpots,
        const std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<InterestRateIndex>>>& irIndices,
        const std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<ZeroInflationIndex>>>& infIndices,
        const std::vector<std::string>& indices, const std::vector<std::string>& indexCurrencies,
        const Handle<BlackScholesModelWrapper>& model,
        const std::map<std::pair<std::string, std::string>, Handle<QuantExt::CorrelationTermStructure>>& correlations,
        const std::set<Date>& simulationDates,
        const IborFallbackConfig& iborFallbackConfig = IborFallbackConfig::defaultConfig(),
        const std::string& calibration = "ATM",
        const std::map<std::string, std::vector<Real>>& calibrationStrikes = {});

    // ctor for single underlying
    BlackScholesCG(const ModelCG::Type type, const Size paths, const std::string& currency,
                   const Handle<YieldTermStructure>& curve, const std::string& index, const std::string& indexCurrency,
                   const Handle<BlackScholesModelWrapper>& model, const std::set<Date>& simulationDates,
                   const IborFallbackConfig& iborFallbackConfig = IborFallbackConfig::defaultConfig(),
                   const std::string& calibration = "ATM", const std::vector<Real>& calibrationStrikes = {});

    // Model interface implementation
    const Date& referenceDate() const override;
    std::size_t npv(const std::size_t amount, const Date& obsdate, const std::size_t filter,
                    const std::optional<long>& memSlot, const std::set<std::size_t> addRegressors,
                    const std::optional<std::set<std::size_t>>& overwriteRegressors) const override;
    std::set<std::size_t> npvRegressors(const Date& obsdate,
                                        const std::optional<std::set<std::string>>& relevantCurrencies) const override;
    std::size_t numeraire(const Date& s) const override;
    std::size_t fwdCompAvg(const bool isAvg, const std::string& indexInput, const Date& obsdate, const Date& start,
                           const Date& end, const Real spread, const Real gearing, const Integer lookback,
                           const Natural rateCutoff, const Natural fixingDays, const bool includeSpread, const Real cap,
                           const Real floor, const bool nakedOption, const bool localCapFloor) const override;

    // t0 market data functions from the ModelCG interface
    Real getDirectFxSpotT0(const std::string& forCcy, const std::string& domCcy) const override;
    Real getDirectDiscountT0(const Date& paydate, const std::string& currency) const override;

protected:
    // ModelImpl interface implementation
    void performCalculations() const override;
    std::size_t getIndexValue(const Size indexNo, const Date& d, const Date& fwd = Null<Date>()) const override;
    std::size_t getIrIndexValue(const Size indexNo, const Date& d, const Date& fwd = Null<Date>()) const override;
    std::size_t getInfIndexValue(const Size indexNo, const Date& d, const Date& fwd = Null<Date>()) const override;
    std::size_t getDiscount(const Size idx, const Date& s, const Date& t) const override;
    std::size_t getFxSpot(const Size idx) const override;
    std::size_t getFutureBarrierProb(const std::string& index, const Date& obsdate1, const Date& obsdate2,
                                     const std::size_t barrier, const bool above) const override;

    // input parameters
    std::vector<Handle<YieldTermStructure>> curves_;
    std::vector<Handle<Quote>> fxSpots_;
    Handle<BlackScholesModelWrapper> model_;
    std::map<std::pair<std::string, std::string>, Handle<QuantExt::CorrelationTermStructure>> correlations_;
    std::vector<Date> simulationDates_;

    // The calibration to use, ATM or Deal
    std::string calibration_;

    // map indexName => calibration strike (for missing indices we'll assume atmf)
    std::map<std::string, std::vector<Real>> calibrationStrikes_;

    // updated in performCalculations()
    mutable Date referenceDate_;                      // the model reference date
    mutable std::set<Date> effectiveSimulationDates_; // the dates effectively simulated (including today)
    mutable TimeGrid timeGrid_;                       // the (possibly refined) time grid for the simulation
    mutable std::vector<Size> positionInTimeGrid_;    // for each effective simulation date the index in the time grid

    // updated in derived classes' performCalculations() whenever cg version changes
    mutable std::map<Date, std::vector<std::size_t>> underlyingPaths_; // per simulation date index states
    mutable std::size_t underlyingPathsCgVersion_ = 0;
};

} // namespace data
} // namespace ore
