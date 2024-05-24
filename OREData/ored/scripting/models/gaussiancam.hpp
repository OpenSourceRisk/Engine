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

/*! \file ored/scripting/models/gaussiancam.hpp
    \brief gaussian cross asset model for ir, fx, eq, com
    \ingroup utilities
*/

#pragma once

#include <ored/scripting/models/amcmodel.hpp>
#include <ored/scripting/models/modelimpl.hpp>

#include <ored/model/crossassetmodelbuilder.hpp>

#include <qle/processes/crossassetstateprocess.hpp>

#include <ql/termstructures/volatility/swaption/swaptionvolstructure.hpp>

namespace ore {
namespace data {

class GaussianCam : public ModelImpl, public AmcModel {
public:
    /* analogue to ModelImpl, plus:
       - cam: a handle to the underlying cross asset model
       - the currencies, curves, fxSpots need to match those in the cam (FIXME, remove them from the ctor?)
       - simulationDates are the dates on which indices can be observed
       - regressionOrder is the regression order used to compute conditional expectations in npv()
       - timeStepsPerYear time steps used for discretisation (overwritten by 1 if exact discretisation is used
       - disc: choose exact or Euler discretisation of state process
     */
    GaussianCam(const Handle<CrossAssetModel>& cam, const Size paths, const std::vector<std::string>& currencies,
                const std::vector<Handle<YieldTermStructure>>& curves, const std::vector<Handle<Quote>>& fxSpots,
                const std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<InterestRateIndex>>>& irIndices,
                const std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<ZeroInflationIndex>>>& infIndices,
                const std::vector<std::string>& indices, const std::vector<std::string>& indexCurrencies,
                const std::set<Date>& simulationDates, const McParams& mcParams, const Size timeStepsPerYear = 1,
                const IborFallbackConfig& iborFallbackConfig = IborFallbackConfig::defaultConfig(),
                const std::vector<Size>& projectedStateProcessIndices = {},
                const std::vector<std::string>& conditionalExpectationModelStates = {});

    // Model interface implementation
    Type type() const override { return Type::MC; }
    const Date& referenceDate() const override;
    Size size() const override;
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

    // AMCModel interface implementation
    void injectPaths(const std::vector<QuantLib::Real>* pathTimes,
                     const std::vector<std::vector<QuantExt::RandomVariable>>* paths,
                     const std::vector<size_t>* pathIndexes, const std::vector<size_t>* timeIndexes) override;

private:
    // ModelImpl interface implementation
    RandomVariable getFutureBarrierProb(const std::string& index, const Date& obsdate1, const Date& obsdate2,
                                        const RandomVariable& barrier, const bool above) const override {
        QL_FAIL("getFutureBarrierProb not implemented by GaussianCam");
    }
    // ModelImpl interface implementation
    void performCalculations() const override;
    RandomVariable getIndexValue(const Size indexNo, const Date& d, const Date& fwd = Null<Date>()) const override;
    RandomVariable getIrIndexValue(const Size indexNo, const Date& d, const Date& fwd = Null<Date>()) const override;
    RandomVariable getInfIndexValue(const Size indexNo, const Date& d, const Date& fwd = Null<Date>()) const override;
    RandomVariable getDiscount(const Size idx, const Date& s, const Date& t) const override;
    RandomVariable getNumeraire(const Date& s) const override;
    Real getFxSpot(const Size idx) const override;

    // same as getDiscount() above, but takes an arbitrary correction curve (for compounding on equity curves)
    RandomVariable getDiscount(const Size idx, const Date& s, const Date& t,
                               const Handle<YieldTermStructure>& targetCurve) const;

    void populatePathValues(const Size nSamples, std::map<Date, std::vector<RandomVariable>>& paths,
                            std::map<Date, std::vector<RandomVariable>>& irStates,
                            std::map<Date, std::vector<std::pair<RandomVariable, RandomVariable>>>& infStates,
                            const std::vector<Real>& times, const bool isTraining) const;
    // input parameters
    const Handle<CrossAssetModel> cam_;
    const std::vector<Handle<YieldTermStructure>> curves_;
    const std::vector<Handle<Quote>> fxSpots_;
    const McParams mcParams_;
    const Size timeStepsPerYear_;
    const std::vector<Size> projectedStateProcessIndices_; // if data is injected via the AMCModel interface
    const Real regressionVarianceCutoff_ = Null<Real>();

    // computed values
    mutable Date referenceDate_;                      // the model reference date
    mutable std::set<Date> effectiveSimulationDates_; // the dates effectively simulated (including today)
    mutable TimeGrid timeGrid_;                       // the (possibly refined) time grid for the simulation
    mutable std::vector<Size> positionInTimeGrid_;    // for each effective simulation date the index in the time grid
    mutable std::map<Date, std::vector<RandomVariable>> underlyingPaths_; // per simulation date index states
    mutable std::map<Date, std::vector<RandomVariable>> irStates_;        // per sim date ir states for currencies_
    mutable std::map<Date, std::vector<std::pair<RandomVariable, RandomVariable>>>
        infStates_; // per sim date inf states dk (x,y) or jy (x,y)
    mutable std::map<Date, std::vector<RandomVariable>> underlyingPathsTraining_; // ditto (training)
    mutable std::map<Date, std::vector<RandomVariable>> irStatesTraining_;        // ditto (training)
    mutable std::map<Date, std::vector<std::pair<RandomVariable, RandomVariable>>>
        infStatesTraining_;                               // ditto (training)
    mutable bool inTrainingPhase_ = false;                // are we currently using training paths?
    mutable std::vector<Size> indexPositionInProcess_;    // maps index no to position in state process
    mutable std::vector<Size> infIndexPositionInProcess_; // maps inf index no to position in state process
    mutable std::vector<Size> currencyPositionInProcess_; // maps currency no to position in state process
    mutable std::vector<Size> irIndexPositionInCam_;      // maps ir index no to currency idx in cam
    mutable std::vector<Size> infIndexPositionInCam_;     // maps inf index no to inf idx in cam
    mutable std::vector<Size> currencyPositionInCam_;     // maps currency no to position in cam parametrizations
    mutable std::vector<Size> eqIndexInCam_;      // maps index no to eq position in cam (or null, if not an eq index)
    mutable std::vector<Size> comIndexInCam_;     // maps index no to com position in cam (or null, if not a com index)
    mutable bool conditionalExpectationUseIr_;    // derived from input conditionalExpectationModelState
    mutable bool conditionalExpectationUseInf_;   // derived from input conditionalExpectationModelState
    mutable bool conditionalExpectationUseAsset_; // derived from input conditionalExpectationModelState

    // internal cache for ir index fixings
    mutable std::map<std::tuple<Size, Date, Date>, RandomVariable> irIndexValueCache_;

    // data when paths are injected via the AMCModel interface
    const std::vector<QuantLib::Real>* injectedPathTimes_ = nullptr;
    const std::vector<std::vector<QuantExt::RandomVariable>>* injectedPaths_ = nullptr;
    const std::vector<size_t>* injectedPathRelevantPathIndexes_;
    const std::vector<size_t>* injectedPathRelevantTimeIndexes_;
    Size overwriteModelSize_ = Null<Size>();

    // stored regression coefficients, state size (before possible transform) and (optional) coordinate transform
    mutable std::map<long, std::tuple<Array, Size, Matrix>> storedRegressionModel_;
};

} // namespace data
} // namespace ore
