/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file engine/valuationengine.hpp
    \brief The cube valuation core
    \ingroup simulation
*/

#pragma once

#include <orea/cube/npvcube.hpp>
#include <orea/engine/cptycalculator.hpp>
#include <orea/engine/valuationcalculator.hpp>
#include <orea/simulation/simmarket.hpp>
#include <ored/model/modelbuilder.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/utilities/dategrid.hpp>
#include <ored/utilities/progressbar.hpp>

#include <map>
#include <set>

namespace ore {
namespace analytics {
using std::set;

//! Valuation Engine
/*!
  The valuation engine's purpose is to generate an NPV cube.
  Its buildCube loops over samples->dates->trades and prices the portfolio
  under all samples and dates.

  The number of dates is defined by the DateGrid passed to the constructor.
  The number of trades is defined by the size of the portfolio passed to buildCube().
  The number of samples is defined by the NPVCube that is passed to buildCube(), this
  can be dynamic.

  In addition to storing the resulting NPVs it can be given any number of calculators
  that can store additional values in the cube.

  \ingroup simulation
*/
class ValuationEngine : public ore::data::ProgressReporter {
public:
    //! Constructor
    ValuationEngine(
        //! Valuation date
        const QuantLib::Date& today,
        //! Simulation date grid
        const boost::shared_ptr<DateGrid>& dg,
        //! Simulated market object
        const boost::shared_ptr<analytics::SimMarket>& simMarket,
        //! model builders to be updated
        const set<std::pair<string, boost::shared_ptr<data::ModelBuilder>>>& modelBuilders =
            set<std::pair<string, boost::shared_ptr<data::ModelBuilder>>>());

    //! Build NPV cube
    void buildCube(
        //! Portfolio to be priced
        const boost::shared_ptr<data::Portfolio>& portfolio,
        //! Object for storing the results at trade level (e.g. NPVs, close-out NPVs, flows)
        boost::shared_ptr<analytics::NPVCube> outputCube,
        //! Calculators to use
        std::vector<boost::shared_ptr<ValuationCalculator>> calculators,
        //! Use sticky date in MPOR evaluation?
        bool mporStickyDate = true,
        //! Output cube for netting set-level results
        boost::shared_ptr<analytics::NPVCube> outputCubeNettingSet = nullptr,
        //! Output cube for storing counterparty-level survival probabilities
        boost::shared_ptr<analytics::NPVCube> outputCptyCube = nullptr,
        //! Calculators for filling counterparty-level results
        std::vector<boost::shared_ptr<CounterpartyCalculator>> cptyCalculators = {},
        //! Limit samples to one and fill the rest of the cube with random values
        bool dryRun = false);

private:
    void recalibrateModels();
    void runCalculators(bool isCloseOutDate, const std::map<std::string, boost::shared_ptr<Trade>>& trades,
                        std::vector<bool>& tradeHasError,
                        const std::vector<boost::shared_ptr<ValuationCalculator>>& calculators,
                        boost::shared_ptr<analytics::NPVCube>& outputCube,
                        boost::shared_ptr<analytics::NPVCube>& outputCubeSensis, const Date& d,
                        const Size cubeDateIndex, const Size sample, const std::string& label = "");
    void runCalculators(bool isCloseOutDate, const std::map<string, Size>& counterparties,
                        const std::vector<boost::shared_ptr<CounterpartyCalculator>>& calculators,
                        boost::shared_ptr<analytics::NPVCube>& cptyCube, const Date& d,
                        const Size cubeDateIndex, const Size sample);
    void tradeExercisable(bool enable, const std::map<std::string, boost::shared_ptr<Trade>>& trades);
    QuantLib::Date today_;
    boost::shared_ptr<DateGrid> dg_;
    boost::shared_ptr<analytics::SimMarket> simMarket_;
    set<std::pair<string, boost::shared_ptr<data::ModelBuilder>>> modelBuilders_;
};
} // namespace analytics
} // namespace ore
