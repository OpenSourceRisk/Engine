/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
 All rights reserved.
*/

/*! \file models/gaussiancamcg.hpp
    \brief Gaussian CAM model
    \ingroup utilities
*/

#pragma once

#include <ored/model/crossassetmodelbuilder.hpp>
#include <ored/scripting/models/modelcgimpl.hpp>

#include <qle/processes/crossassetstateprocess.hpp>

#include <ql/termstructures/volatility/swaption/swaptionvolstructure.hpp>

namespace ore {
namespace data {

class GaussianCamCG : public ModelCGImpl {
public:
    /* For the constructor arguments see ModelCGImpl, plus the notes in GaussianCam */
    GaussianCamCG(const Handle<CrossAssetModel>& cam, const Size paths, const std::vector<std::string>& currencies,
                  const std::vector<Handle<YieldTermStructure>>& curves, const std::vector<Handle<Quote>>& fxSpots,
                  const std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<InterestRateIndex>>>& irIndices,
                  const std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<ZeroInflationIndex>>>& infIndices,
                  const std::vector<std::string>& indices, const std::vector<std::string>& indexCurrencies,
                  const std::set<Date>& simulationDates, const Size timeStepsPerYear = 1,
                  const IborFallbackConfig& iborFallbackConfig = IborFallbackConfig::defaultConfig(),
                  const std::vector<Size>& projectedStateProcessIndices = {},
                  const std::vector<std::string>& conditionalExpectationModelStates = {},
                  const bool sloppySimDates = false);

    // Model interface implementation
    Type type() const override { return Type::MC; }
    const Date& referenceDate() const override;
    std::size_t npv(const std::size_t amount, const Date& obsdate, const std::size_t filter,
                    const boost::optional<long>& memSlot, const std::size_t addRegressor1,
                    const std::size_t addRegressor2) const override;
    std::size_t fwdCompAvg(const bool isAvg, const std::string& indexInput, const Date& obsdate, const Date& start,
                           const Date& end, const Real spread, const Real gearing, const Integer lookback,
                           const Natural rateCutoff, const Natural fixingDays, const bool includeSpread, const Real cap,
                           const Real floor, const bool nakedOption, const bool localCapFloor) const override;
    QuantLib::Size size() const override;

    // t0 market data functions from the ModelCG interface
    Real getDirectFxSpotT0(const std::string& forCcy, const std::string& domCcy) const override;
    Real getDirectDiscountT0(const Date& paydate, const std::string& currency) const override;

protected:
    // ModelCGImpl interface implementation
    virtual std::size_t getFutureBarrierProb(const std::string& index, const Date& obsdate1, const Date& obsdate2,
                                             const std::size_t barrier, const bool above) const override {
        QL_FAIL("getFutureBarrierProb not implemented by GaussianCamCG");
    }
    // ModelCGImpl interface implementation
    void performCalculations() const override;
    std::size_t getIndexValue(const Size indexNo, const Date& d, const Date& fwd = Null<Date>()) const override;
    std::size_t getIrIndexValue(const Size indexNo, const Date& d, const Date& fwd = Null<Date>()) const override;
    std::size_t getInfIndexValue(const Size indexNo, const Date& d, const Date& fwd = Null<Date>()) const override;
    std::size_t getDiscount(const Size idx, const Date& s, const Date& t) const override;
    std::size_t getNumeraire(const Date& s) const override;
    std::size_t getFxSpot(const Size idx) const override;

    // input parameters
    const Handle<CrossAssetModel> cam_;
    const std::vector<Handle<YieldTermStructure>> curves_;
    const std::vector<Handle<Quote>> fxSpots_;
    const Size timeStepsPerYear_;
    const std::vector<Size> projectedStateProcessIndices_;
    const bool sloppySimDates_;

    // updated in performCalculations()
    mutable Date referenceDate_;                      // the model reference date
    mutable std::set<Date> effectiveSimulationDates_; // the dates effectively simulated (including today)
    mutable TimeGrid timeGrid_;                       // the (possibly refined) time grid for the simulation
    mutable std::vector<Size> positionInTimeGrid_;    // for each effective simulation date the index in the time grid
    mutable std::map<Date, std::vector<std::size_t>> underlyingPaths_; // per simulation date index states
    mutable std::map<Date, std::vector<std::size_t>> irStates_;        // per simulation date ir states for currencies_
    mutable std::map<Date, std::vector<std::pair<std::size_t, std::size_t>>>
        infStates_;                                       // per simulation date dk (x,y) or jy (x,y)
    mutable std::vector<Size> indexPositionInProcess_;    // maps index no to position in state process
    mutable std::vector<Size> infIndexPositionInProcess_; // maps inf index no to position in state process
    mutable std::vector<Size> currencyPositionInProcess_; // maps currency no to position in state process
    mutable std::vector<Size> irIndexPositionInCam_;      // maps ir index no to currency idx in cam
    mutable std::vector<Size> infIndexPositionInCam_;     // maps inf index no to inf idx in cam
    mutable std::vector<Size> currencyPositionInCam_;     // maps currency no to position in cam parametrizations
    mutable std::vector<Size> eqIndexInCam_;      // maps index no to eq position in cam (or null, if not an eq index)
    mutable bool conditionalExpectationUseIr_;    // derived from input conditionalExpectationModelState
    mutable bool conditionalExpectationUseInf_;   // derived from input conditionalExpectationModelState
    mutable bool conditionalExpectationUseAsset_; // derived from input conditionalExpectationModelState

    mutable std::size_t underlyingPathsCgVersion_ = 0;

    // data when paths are injected via the AMCModelCG interface
    const std::vector<QuantLib::Real>* injectedPathTimes_ = nullptr;
    const std::vector<std::vector<std::size_t>>* injectedPaths_ = nullptr;
    const std::vector<bool>* injectedPathIsRelevantTime_;
    bool injectedPathStickyCloseOutRun_;
    Size overwriteModelSize_ = Null<Size>();
};

} // namespace data
} // namespace ore
