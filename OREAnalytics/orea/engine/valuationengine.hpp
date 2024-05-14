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

#include <ored/utilities/progressbar.hpp>

#include <ql/time/date.hpp>

#include <map>
#include <set>

namespace ore::data {
class DateGrid;
class Portfolio;
class Trade;
} // namespace ore::data

namespace QuantExt {
class ModelBuilder;
}

namespace ore {
namespace analytics {

class NPVCube;
class CounterpartyCalculator;
class ValuationCalculator;
class SimMarket;

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
        const QuantLib::ext::shared_ptr<ore::data::DateGrid>& dg,
        //! Simulated market object
        const QuantLib::ext::shared_ptr<analytics::SimMarket>& simMarket,
        //! model builders to be updated
        const set<std::pair<std::string, QuantLib::ext::shared_ptr<QuantExt::ModelBuilder>>>& modelBuilders =
            set<std::pair<std::string, QuantLib::ext::shared_ptr<QuantExt::ModelBuilder>>>());

    //! Build NPV cube
    void buildCube(
        //! Portfolio to be priced
        const QuantLib::ext::shared_ptr<data::Portfolio>& portfolio,
        //! Object for storing the results at trade level (e.g. NPVs, close-out NPVs, flows)
        QuantLib::ext::shared_ptr<analytics::NPVCube> outputCube,
        //! Calculators to use
        std::vector<QuantLib::ext::shared_ptr<ValuationCalculator>> calculators,
        //! Use sticky date in MPOR evaluation?
        bool mporStickyDate = true,
        //! Output cube for netting set-level results
        QuantLib::ext::shared_ptr<analytics::NPVCube> outputCubeNettingSet = nullptr,
        //! Output cube for storing counterparty-level survival probabilities
        QuantLib::ext::shared_ptr<analytics::NPVCube> outputCptyCube = nullptr,
        //! Calculators for filling counterparty-level results
        std::vector<QuantLib::ext::shared_ptr<CounterpartyCalculator>> cptyCalculators = {},
        //! Limit samples to one and fill the rest of the cube with random values
        bool dryRun = false);

private:
    void recalibrateModels();
    std::pair<double, double> populateCube(const QuantLib::Date& d, size_t cubeDateIndex, size_t sample,
                                           bool isValueDate, bool isStickyDate, bool scenarioUpdated,
                                           const std::map<std::string, QuantLib::ext::shared_ptr<ore::data::Trade>>& trades,
                                           std::vector<bool>& tradeHasError,
                                           const std::vector<QuantLib::ext::shared_ptr<ValuationCalculator>>& calculators,
                                           QuantLib::ext::shared_ptr<analytics::NPVCube>& outputCube,
                                           QuantLib::ext::shared_ptr<analytics::NPVCube>& outputCubeNettingSet,
                                           const std::map<std::string, size_t>& counterparties,
                                           const std::vector<QuantLib::ext::shared_ptr<CounterpartyCalculator>>& cptyCalculators,
                                           QuantLib::ext::shared_ptr<analytics::NPVCube>& outputCptyCube);
    void runCalculators(bool isCloseOutDate, const std::map<std::string, QuantLib::ext::shared_ptr<ore::data::Trade>>& trades,
                        std::vector<bool>& tradeHasError,
                        const std::vector<QuantLib::ext::shared_ptr<ValuationCalculator>>& calculators,
                        QuantLib::ext::shared_ptr<analytics::NPVCube>& outputCube,
                        QuantLib::ext::shared_ptr<analytics::NPVCube>& outputCubeSensis, const QuantLib::Date& d,
                        const QuantLib::Size cubeDateIndex, const QuantLib::Size sample, const std::string& label = "");
    void runCalculators(bool isCloseOutDate, const std::map<std::string, QuantLib::Size>& counterparties,
                        const std::vector<QuantLib::ext::shared_ptr<CounterpartyCalculator>>& calculators,
                        QuantLib::ext::shared_ptr<analytics::NPVCube>& cptyCube, const QuantLib::Date& d,
                        const QuantLib::Size cubeDateIndex, const QuantLib::Size sample);
    void tradeExercisable(bool enable, const std::map<std::string, QuantLib::ext::shared_ptr<ore::data::Trade>>& trades);
    QuantLib::Date today_;
    QuantLib::ext::shared_ptr<ore::data::DateGrid> dg_;
    QuantLib::ext::shared_ptr<ore::analytics::SimMarket> simMarket_;
    set<std::pair<std::string, QuantLib::ext::shared_ptr<QuantExt::ModelBuilder>>> modelBuilders_;
};
} // namespace analytics
} // namespace ore
