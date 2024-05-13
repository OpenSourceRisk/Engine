/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

/*! \file orea/engine/historicalsensipnlcalculator.hpp
    \brief Class for generating sensi pnl
*/

#pragma once

#include <orea/cube/npvcube.hpp>
#include <orea/scenario/historicalscenariogenerator.hpp>
#include <orea/scenario/scenarioshiftcalculator.hpp>
#include <orea/engine/sensitivityrecord.hpp>
#include <orea/engine/sensitivitystream.hpp>
#include <orea/scenario/scenario.hpp>

#include <ql/math/matrix.hpp>
#include <ql/shared_ptr.hpp>

#include <boost/accumulators/accumulators.hpp>
// Including <boost/accumulators/statistics.hpp> causes the following swig wrapper compiler errors
//   explicit specialization of 'boost::accumulators::feature_of<boost::accumulators::tag::weighted_skewness>'
//   explicit specialization of 'boost::accumulators::feature_of<boost::accumulators::tag::weighted_kurtosis>'
// see https://github.com/boostorg/accumulators/issues/20
// The following subset of includes is sufficient here and circumvents the swig errors
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/variance.hpp>
#include <boost/accumulators/statistics/covariance.hpp>
#include <boost/accumulators/statistics/variates/covariate.hpp>

namespace ore {
namespace analytics {

class PNLCalculator {
public:
    PNLCalculator(ore::data::TimePeriod pnlPeriod) : pnlPeriod_(pnlPeriod) {}
    virtual ~PNLCalculator() {}
    virtual void writePNL(QuantLib::Size scenarioIdx, bool isCall,
                          const RiskFactorKey& key_1, QuantLib::Real shift_1, QuantLib::Real delta,
                          QuantLib::Real gamma, QuantLib::Real deltaPnl, Real gammaPnl,
                          const RiskFactorKey& key_2 = RiskFactorKey(),
                          QuantLib::Real shift_2 = 0.0, const std::string& tradeId = "") {}
    const bool isInTimePeriod(QuantLib::Date startDate, QuantLib::Date endDate);

    void populatePNLs(const std::vector<QuantLib::Real>& allPnls, const std::vector<QuantLib::Real>& foPnls,
                      const std::vector<QuantLib::Date>& startDates, const std::vector<QuantLib::Date>& endDates);
    
    using TradePnLStore = std::vector<std::vector<QuantLib::Real>>;
    void populateTradePNLs(const TradePnLStore& allPnls, const TradePnLStore& foPnls);

    const std::vector<QuantLib::Real>& pnls() { return pnls_; };
    const std::vector<QuantLib::Real>& foPnls() { return foPnls_; };

    const TradePnLStore& tradePnls() { return tradePnls_; }
    const TradePnLStore& foTradePnls() { return foTradePnls_; }

    void clear() { 
        pnls_.clear();
        foPnls_.clear();
        tradePnls_.clear();
        foTradePnls_.clear();
    }

protected:
    std::vector<QuantLib::Real> pnls_;
    std::vector<QuantLib::Real> foPnls_;
    ore::data::TimePeriod pnlPeriod_;
    TradePnLStore tradePnls_, foTradePnls_;
};

class CovarianceCalculator {
public:
    CovarianceCalculator(ore::data::TimePeriod covariancePeriod) : covariancePeriod_(covariancePeriod) {}
    void initialise(const std::set<std::pair<RiskFactorKey, QuantLib::Size>>& keys);
    void updateAccumulators(const QuantLib::ext::shared_ptr<NPVCube>& shiftCube, QuantLib::Date startDate, QuantLib::Date endDate, QuantLib::Size index);
    void populateCovariance(const std::set<std::pair<RiskFactorKey, QuantLib::Size>>& keys);
    const Matrix& covariance() const { return covariance_; }

private:
    typedef boost::accumulators::accumulator_set<
        QuantLib::Real,
        boost::accumulators::stats<boost::accumulators::tag::covariance<QuantLib::Real, boost::accumulators::tag::covariate1>>>
        accumulator;
    std::map<std::pair<QuantLib::Size, QuantLib::Size>, accumulator> accCov_;
    ore::data::TimePeriod covariancePeriod_;
    QuantLib::Matrix covariance_;
};

class HistoricalSensiPnlCalculator {
public:
    HistoricalSensiPnlCalculator(const QuantLib::ext::shared_ptr<HistoricalScenarioGenerator>& hisScenGen,
                                 const QuantLib::ext::shared_ptr<SensitivityStream>& ss)
        : hisScenGen_(hisScenGen), sensitivityStream_(ss) {}
    
    void populateSensiShifts(QuantLib::ext::shared_ptr<NPVCube>& cube, const vector<RiskFactorKey>& keys,
                             QuantLib::ext::shared_ptr<ScenarioShiftCalculator> shiftCalculator);

    void calculateSensiPnl(const std::set<SensitivityRecord>& srs,
        const std::vector<RiskFactorKey>& rfKeys,
        QuantLib::ext::shared_ptr<NPVCube>& shiftCube,
        const std::vector<QuantLib::ext::shared_ptr<PNLCalculator>>& pnlCalculators,
        const QuantLib::ext::shared_ptr<CovarianceCalculator>& covarianceCalculator,
        const std::vector<std::string>& tradeIds = {},
        const bool includeGammaMargin = true, const bool includeDeltaMargin = true, 
        const bool tradeLevel = false);

private:
    QuantLib::ext::shared_ptr<HistoricalScenarioGenerator> hisScenGen_;
    //! Stream of sensitivity records used for the sensitivity based backtest
    QuantLib::ext::shared_ptr<SensitivityStream> sensitivityStream_;
};

} // namespace analytics
} // namespace ore
