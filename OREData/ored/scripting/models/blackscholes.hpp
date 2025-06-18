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

/*! \file ored/scripting/models/blackscholes.hpp
    \brief black scholes / local vol model class for n underlyings (fx, equity or commodity)
    \ingroup utilities
*/

#pragma once

#include <ored/model/utilities.hpp>
#include <ored/scripting/models/modelimpl.hpp>
#include <ored/utilities/to_string.hpp>

#include <qle/methods/multipathvariategenerator.hpp>
#include <qle/models/blackscholesmodelwrapper.hpp>
#include <qle/termstructures/correlationtermstructure.hpp>

#include <ql/indexes/interestrateindex.hpp>
#include <ql/math/comparison.hpp>
#include <ql/math/matrixutilities/choleskydecomposition.hpp>
#include <ql/math/matrixutilities/pseudosqrt.hpp>
#include <ql/math/matrixutilities/symmetricschurdecomposition.hpp>
#include <ql/methods/finitedifferences/meshers/fdmmesher.hpp>
#include <ql/methods/finitedifferences/solvers/fdmbackwardsolver.hpp>
#include <ql/processes/blackscholesprocess.hpp>
#include <ql/timegrid.hpp>

namespace ore {
namespace data {

/* This class is the basis for the BlackScholes and LocalVol model implementations */
class BlackScholes : public ModelImpl {
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
       - processes: hold spot, rate and div ts and vol for each given index
       - we assume that the given correlations are constant and read the value only at t = 0
       - calibration = "ATM", "Deal", "LocalVol"
       - calibration strikes are given as a map indexName => strike, if an index is missing in this map, the calibration
         strike will be atmf
    */
    BlackScholes(
        const Type type, const Size size, const std::vector<std::string>& currencies,
        const std::vector<Handle<YieldTermStructure>>& curves, const std::vector<Handle<Quote>>& fxSpots,
        const std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<InterestRateIndex>>>& irIndices,
        const std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<ZeroInflationIndex>>>& infIndices,
        const std::vector<std::string>& indices, const std::vector<std::string>& indexCurrencies,
        const std::set<std::string>& payCcys, const Handle<BlackScholesModelWrapper>& model,
        const std::map<std::pair<std::string, std::string>, Handle<QuantExt::CorrelationTermStructure>>& correlations,
        const std::set<Date>& simulationDates,
        const IborFallbackConfig& iborFallbackConfig = IborFallbackConfig::defaultConfig(),
        const std::string& calibration = "ATM", const std::map<std::string, std::vector<Real>>& calibrationStrikes = {},
        const Params& params = {});

    // ctor for single underlying
    BlackScholes(const Type Type, const Size size, const std::string& currency, const Handle<YieldTermStructure>& curve,
                 const std::string& index, const std::string& indexCurrency,
                 const Handle<BlackScholesModelWrapper>& model, const std::set<Date>& simulationDates,
                 const IborFallbackConfig& iborFallbackConfig = IborFallbackConfig::defaultConfig(),
                 const std::string& calibration = "ATM", const std::vector<Real>& calibrationStrikes = {},
                 const Params& params = {});

    // Model interface implementation
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

    // override for FD
    Real extractT0Result(const RandomVariable& result) const override;

    // override to handle cases where we use a quanto-adjusted pde for FD
    const std::string& baseCcy() const override;
    RandomVariable pay(const RandomVariable& amount, const Date& obsdate, const Date& paydate,
                       const std::string& currency) const override;

protected:
    // ModelImpl interface implementation
    void performCalculations() const override;
    RandomVariable getIndexValue(const Size indexNo, const Date& d, const Date& fwd = Null<Date>()) const override;
    RandomVariable getIrIndexValue(const Size indexNo, const Date& d, const Date& fwd = Null<Date>()) const override;
    RandomVariable getInfIndexValue(const Size indexNo, const Date& d, const Date& fwd = Null<Date>()) const override;
    RandomVariable getDiscount(const Size idx, const Date& s, const Date& t) const override;
    RandomVariable getNumeraire(const Date& s) const override;
    Real getFxSpot(const Size idx) const override;
    RandomVariable getFutureBarrierProb(const std::string& index, const Date& obsdate1, const Date& obsdate2,
                                        const RandomVariable& barrier, const bool above) const override;

    // helper functions
    Matrix getCorrelation() const;
    std::vector<Real> getCalibrationStrikes() const;
    void setAdditionalResults() const;

    // BS / LV and type specific code
    void performCalculationsMcBs() const;
    void performCalculationsMcLv() const;
    void performCalculationsFd() const;
    void initUnderlyingPathsMc() const;
    void setReferenceDateValuesMc() const;
    void generatePathsBs() const;
    void generatePathsLv() const;
    void populatePathValuesBs(const Size nSamples, std::map<Date, std::vector<RandomVariable>>& paths,
                              const QuantLib::ext::shared_ptr<MultiPathVariateGeneratorBase>& gen,
                              const std::vector<Array>& drift, const std::vector<Matrix>& sqrtCov) const;
    void populatePathValuesLv(const Size nSamples, std::map<Date, std::vector<RandomVariable>>& paths,
                              const QuantLib::ext::shared_ptr<MultiPathVariateGeneratorBase>& gen,
                              const Matrix& correlation, const Matrix& sqrtCorr,
                              const std::vector<Array>& deterministicDrift, const std::vector<Size>& eqComIdx,
                              const std::vector<Real>& t, const std::vector<Real>& dt,
                              const std::vector<Real>& sqrtdt) const;

    // input parameters
    std::vector<Handle<YieldTermStructure>> curves_;
    std::vector<Handle<Quote>> fxSpots_;
    std::set<std::string> payCcys_;
    Handle<BlackScholesModelWrapper> model_;
    std::map<std::pair<std::string, std::string>, Handle<QuantExt::CorrelationTermStructure>> correlations_;
    std::vector<Date> simulationDates_;
    std::string calibration_;
    std::map<std::string, std::vector<Real>> calibrationStrikes_;

    // quanto adjustment parameters (used for model type FD only)
    bool applyQuantoAdjustment_ = false;
    Size quantoSourceCcyIndex_, quantoTargetCcyIndex_;
    Real quantoCorrelationMultiplier_;

    // these all except underlyingPaths_ are initialised when the interface functions above are called
    mutable Date referenceDate_;                      // the model reference date
    mutable std::set<Date> effectiveSimulationDates_; // the dates effectively simulated (including today)
    mutable TimeGrid timeGrid_;                       // the (possibly refined) time grid for the simulation
    mutable std::vector<Size> positionInTimeGrid_;    // for each effective simulation date the index in the time grid
    mutable Matrix correlation_;                      // the correlation matrix (constant in time)

    // used for MC only:
    mutable std::map<Date, std::vector<RandomVariable>> underlyingPaths_;         // per simulation date index states
    mutable std::map<Date, std::vector<RandomVariable>> underlyingPathsTraining_; // ditto (training phase)
    mutable bool inTrainingPhase_ = false;   // are we currently using training paths?
    mutable std::vector<Matrix> covariance_; // covariance per effective simulation date
    mutable std::map<long, std::tuple<Array, Size, Matrix>> storedRegressionModel_; // stored regression coefficients

    // used for FD only:
    mutable QuantLib::ext::shared_ptr<FdmMesher> mesher_;              // the mesher for the FD solver
    mutable QuantLib::ext::shared_ptr<FdmLinearOpComposite> operator_; // the operator
    mutable QuantLib::ext::shared_ptr<FdmBackwardSolver> solver_;      // the sovler
    mutable RandomVariable underlyingValues_;                          // the discretised underlying
};

} // namespace data
} // namespace ore
