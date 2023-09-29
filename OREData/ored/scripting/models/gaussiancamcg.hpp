/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
 All rights reserved.
*/

/*! \file models/gaussiancamcg.hpp
    \brief Gaussian CAM model
    \ingroup utilities
*/

#pragma once

#include <orepscriptedtrade/ored/models/amcmodelcg.hpp>
#include <orepscriptedtrade/ored/models/modelcgimpl.hpp>

#include <ored/model/crossassetmodelbuilder.hpp>

#include <qle/processes/crossassetstateprocess.hpp>

#include <ql/termstructures/volatility/swaption/swaptionvolstructure.hpp>

namespace oreplus {
namespace data {

class GaussianCamCG : public ModelCGImpl, public AmcModelCG {
public:
    /* For the constructor arguments see ModelCGImpl, plus:
       - eq, com processes are given with arbitrary riskFreeRate() and dividendYield(), these two curves only define
         the forward curve drift for each asset
       - the base ccy is the first ccy in the currency vector, the fx spots are given as for-base, the ccy curves define
         the fx forwards
       - fx processes must be given w.r.t. the base ccy and consistent with the given fx spots and curves, but we do not
         require fx processes for all currencies (but they are required, if an fx index is evaluated in eval())
       - correlations are for index pair names and must be constant; if not given for a pair, we assume zero correlation
       - regressionOrder is the regression order used to compute conditional expectations in npv()
     */
    GaussianCamCG(const Handle<CrossAssetModel>& cam, const Size paths, const std::vector<std::string>& currencies,
                  const std::vector<Handle<YieldTermStructure>>& curves, const std::vector<Handle<Quote>>& fxSpots,
                  const std::vector<std::pair<std::string, boost::shared_ptr<InterestRateIndex>>>& irIndices,
                  const std::vector<std::pair<std::string, boost::shared_ptr<ZeroInflationIndex>>>& infIndices,
                  const std::vector<std::string>& indices, const std::vector<std::string>& indexCurrencies,
                  const std::set<Date>& simulationDates, const Size timeStepsPerYear = 1,
                  const IborFallbackConfig& iborFallbackConfig = IborFallbackConfig::defaultConfig(),
                  const std::vector<Size>& projectedStateProcessIndices = {},
                  const std::vector<std::string>& conditionalExpectationModelStates = {});

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

    // TODO ....
    void resetNPVMem() override {}
    void injectPaths(const std::vector<QuantLib::Real>* pathTimes,
                     const std::vector<std::vector<std::size_t>>* variates, const std::vector<bool>* isRelevantTime,
                     const bool stickyCloseOutRun) override;

protected:
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
    const std::vector<std::string>& currencies_;
    const std::vector<Handle<YieldTermStructure>> curves_;
    const std::vector<Handle<Quote>> fxSpots_;
    const std::vector<std::pair<std::string, boost::shared_ptr<InterestRateIndex>>> irIndices_;
    const std::vector<std::pair<std::string, boost::shared_ptr<ZeroInflationIndex>>> infIndices_;
    const std::vector<std::string>& indices_;
    const std::vector<std::string> indexCurrencies_;
    const std::set<Date> simulationDates_;
    const Size timeStepsPerYear_;
    const IborFallbackConfig iborFallbackConfig_;
    const std::vector<Size> projectedStateProcessIndices_;
    const std::vector<std::string> conditionalExpectationModelStates_;

    // updated in performCalculations()
    mutable Date referenceDate_;                      // the model reference date
    mutable std::set<Date> effectiveSimulationDates_; // the dates effectively simulated (including today)
    mutable TimeGrid timeGrid_;                       // the (possibly refined) time grid for the simulation
    mutable std::vector<Size> positionInTimeGrid_;    // for each effective simulation date the index in the time grid
    mutable std::map<Date, std::vector<std::size_t>> underlyingPaths_; // per simulation date index states
    mutable std::size_t underlyingPathsCgVersion_ = 0;

    // data when paths are injected via the AMCModelCG interface
    const std::vector<QuantLib::Real>* injectedPathTimes_ = nullptr;
    const std::vector<std::vector<std::size_t>>* injectedPaths_ = nullptr;
    const std::vector<bool>* injectedPathIsRelevantTime_;
    bool injectedPathStickyCloseOutRun_;
    Size overwriteModelSize_ = Null<Size>();
};

} // namespace data
} // namespace oreplus
