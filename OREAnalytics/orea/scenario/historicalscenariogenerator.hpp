/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

/*! \file orea/scenario/historicalscenariogenerator.hpp
    \brief scenario generator that builds from historical shifts
    \ingroup scenario
 */

#pragma once

#include <boost/make_shared.hpp>
#include <ored/utilities/timeperiod.hpp>
#include <orea/scenario/scenariofactory.hpp>
#include <orea/scenario/scenariogenerator.hpp>
#include <orea/scenario/scenariosimmarket.hpp>
#include <orea/scenario/historicalscenarioloader.hpp>
#include <ored/marketdata/adjustmentfactors.hpp>

#include <ql/math/randomnumbers/rngtraits.hpp>

namespace ore {
namespace analytics {

//! Return type for historical scenario generation (absolute, relative, log)
class ReturnConfiguration {

public:
    enum class ReturnType { Absolute, Relative, Log };

    //! default return types per risk factor
    ReturnConfiguration();

    //! customised return types per risk factor
    ReturnConfiguration(const std::map<RiskFactorKey::KeyType, ReturnType>& returnType);

    /*! Compute return from v1, v2.
        The date parameters are are used to improve the log messages
    */
    QuantLib::Real returnValue(const RiskFactorKey& key, const QuantLib::Real v1, const QuantLib::Real v2,
        const QuantLib::Date& d1, const QuantLib::Date& d2) const;

    //! apply return from v1, v2 to base value
    QuantLib::Real applyReturn(const RiskFactorKey& key, const QuantLib::Real baseValue,
        const QuantLib::Real returnValue) const;

private:
    const std::map<RiskFactorKey::KeyType, ReturnType> returnType_;

    //! Perform checks on key
    void check(const RiskFactorKey& key) const;
};

//! Historical Scenario Generator
/*! A Scenario Generator that takes historical scenarios and builds new scenarios by applying historical shifts to the
 * base scenario (which typically comes from todays market).
 *
 *  This generator can be used for historical VAR and backtesting calculations, unlike a PRNG based generator it is
 * limited in the number of scenarios it can generate.
 *
 *  The scenarios generated are based on the scenario differences between t and t+mpor, these differences are typically
 * a relative change and this change is then applied to the baseScenario to give a new scenario which is asof Today or
 * Today+mpor.
 */
class HistoricalScenarioGenerator : public ScenarioGenerator {
public:
    //! Default constructor
    HistoricalScenarioGenerator(
        //! Historical Scenario Loader containing all scenarios
        const boost::shared_ptr<HistoricalScenarioLoader>& historicalScenarioLoader,
        //! Scenario factory to use
        const boost::shared_ptr<ScenarioFactory>& scenarioFactory,
        //! Calendar to use
        const QuantLib::Calendar& cal,
        //! Mpor days or step size
        const Size mporDays = 10,
        //! overlapping scenarios
        const bool overlapping = true,
        //! return configuration
        const ReturnConfiguration& returnConfiguration = ReturnConfiguration(),
        //! optional adjustment factors for stock splits etc
        const boost::shared_ptr<ore::data::AdjustmentFactors>& adjFactors = nullptr,
        //! string prepended to label of all scenarios generated
        const std::string& labelPrefix = "");

    //! Set base scenario, this also defines the asof date
    boost::shared_ptr<Scenario>& baseScenario() { return baseScenario_; }
    //! Get base scenario
    const boost::shared_ptr<Scenario>& baseScenario() const { return baseScenario_; }

    //! Get calendar
    const QuantLib::Calendar& cal() const { return cal_; }
    //! Get mpor days
    QuantLib::Size mporDays() const { return mporDays_; }
    //! Are scenarios overlapping
    bool overlapping() const { return overlapping_; }

    //! Return configuration
    const ReturnConfiguration& returnConfiguration() const { return returnConfiguration_; }

    //! Scaling
    virtual QuantLib::Real scaling(const RiskFactorKey& key, const QuantLib::Real& keyReturn) { return 1; };

    //! Return the next scenario for the given date.
    /*! Date should be asof or asof+mporDays, this class only checks that date is >= the asof date.
        Whatever you put in here will be in the returned scenario.

        Generator returns scenarios in order of shifts and throws if we have run out of historicals

        If Mpor > 1 than the scenarios will overlap.
    */
    boost::shared_ptr<Scenario> next(const QuantLib::Date& d) override;

    //! Reset the generator so calls to next() return the first scenario.
    /*! This allows re-generation of scenarios if required. */
    void reset() override { i_ = 0; }

    //! Number of scenarios this generator can generate
    virtual QuantLib::Size numScenarios() const;

    //! set the start and end dates, can be overwritten in derived class
    virtual void setDates();

    //! start dates from which the scenarios were generated from
    const std::vector<QuantLib::Date>& startDates() const { return startDates_; }
    //! end dates from which the scenarios were generated from
    const std::vector<QuantLib::Date>& endDates() const { return endDates_; }

    //! Get the HistoricalScenarioLoader
    const boost::shared_ptr<HistoricalScenarioLoader>& scenarioLoader() const { return historicalScenarioLoader_; }

    //! Get the ScenarioFactory
    const boost::shared_ptr<ScenarioFactory>& scenarioFactory() const { return scenarioFactory_; }

    //! Get the adj factors
    const boost::shared_ptr<ore::data::AdjustmentFactors>& adjFactors() const { return adjFactors_; }

    //! Get (start, end) scenario date pairs filtered on the given period
    std::vector<std::pair<QuantLib::Date, QuantLib::Date>> filteredScenarioDates(const ore::data::TimePeriod& period) const;

    //! Get the scenario label prefix
    const std::string& labelPrefix() const { return labelPrefix_; }

protected:
    // to be managed in derived classes, if next is overwritten
    Size i_;

    boost::shared_ptr<HistoricalScenarioLoader> historicalScenarioLoader_;
    std::vector<QuantLib::Date> startDates_, endDates_;

    boost::shared_ptr<ScenarioFactory> scenarioFactory_;
    boost::shared_ptr<Scenario> baseScenario_;

    //! The Scenario Pairs for a given index
    std::pair<boost::shared_ptr<Scenario>, boost::shared_ptr<Scenario>> scenarioPair();

    //! Returns the adjusted price
    /*! Scenarios may contian unadjusted market prices e.g equity spot prices,
        apply adjustment factors to ensure no jumps between 2 scenarios
        Only handles equity spot adjustments at the moment */
    QuantLib::Real adjustedPrice(RiskFactorKey key, QuantLib::Date d, QuantLib::Real price);

private:
    QuantLib::Calendar cal_;
    QuantLib::Size mporDays_;
    bool overlapping_;
    ReturnConfiguration returnConfiguration_;
    boost::shared_ptr<ore::data::AdjustmentFactors> adjFactors_;
    std::string labelPrefix_;
};

//! Historical scenario generator generating random scenarios, for testing purposes
class HistoricalScenarioGeneratorRandom : public HistoricalScenarioGenerator {
public:
    HistoricalScenarioGeneratorRandom(
        //! Historical Scenario Loader containing all scenarios
        const boost::shared_ptr<HistoricalScenarioLoader>& historicalScenarioLoader,
        /*! Scenario factory to use */
        const boost::shared_ptr<ScenarioFactory>& scenarioFactory,
        //! Calendar to use
        const QuantLib::Calendar& cal,
        //! MPOR days
        const QuantLib::Size mporDays = 10,
        //! overlapping scenarios
        const bool overlapping = true,
        //! return configuration
        const ReturnConfiguration& returnConfiguration = ReturnConfiguration())
        : HistoricalScenarioGenerator(historicalScenarioLoader, scenarioFactory, cal, mporDays, overlapping,
                                      returnConfiguration) {
        normalrng_ = boost::make_shared<QuantLib::PseudoRandom::rng_type>(MersenneTwisterUniformRng(42));
    };

    boost::shared_ptr<Scenario> next(const QuantLib::Date& d) override;
    void reset() override;

private:
    boost::shared_ptr<QuantLib::PseudoRandom::rng_type> normalrng_;
};

//! Historical scenario generator transform
/*! This class allows scenarios within a historical scenario generator to be transformed from discount rates to
 *  zero rates and survival probabilities to hazard rates
 *  WARNING: This transform function should only be used for backtesting statistics reports requiring the transforms
 *  listed above.
 */
class HistoricalScenarioGeneratorTransform : public ScenarioGenerator {
public:
    HistoricalScenarioGeneratorTransform(boost::shared_ptr<HistoricalScenarioGenerator>& hsg,
                                         const boost::shared_ptr<ScenarioSimMarket>& simMarket,
                                         const boost::shared_ptr<ScenarioSimMarketParameters>& simMarketConfig)
        : hsg_(hsg), simMarket_(simMarket), simMarketConfig_(simMarketConfig) {}

    boost::shared_ptr<Scenario> next(const QuantLib::Date& d) override;

    void reset() override;

private:
    boost::shared_ptr<HistoricalScenarioGenerator> hsg_;
    boost::shared_ptr<ScenarioSimMarket> simMarket_;
    boost::shared_ptr<ScenarioSimMarketParameters> simMarketConfig_;
};

// Historical scenario generator with filtered scenario dates
/*! This class will only generate the subset of scenarios with start / end dates contained in one of the given time periods.
    Warning: the base scenario in the passed historical scenario generator must be set on construction of this class, later
    changes in the base scenario will not be reflected in an instance of this class. */
class HistoricalScenarioGeneratorWithFilteredDates : public HistoricalScenarioGenerator {
public:
    HistoricalScenarioGeneratorWithFilteredDates(const std::vector<ore::data::TimePeriod>& filter,
                                                 const boost::shared_ptr<HistoricalScenarioGenerator>& gen);
    void reset() override;
    boost::shared_ptr<Scenario> next(const QuantLib::Date& d) override;

private:
    boost::shared_ptr<HistoricalScenarioGenerator> gen_;
    std::vector<bool> isRelevantScenario_;
    QuantLib::Size i_orig_;
};

} // namespace analytics
} // namespace ore
