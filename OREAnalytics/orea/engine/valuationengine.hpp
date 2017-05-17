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
#include <orea/engine/valuationcalculator.hpp>
#include <orea/simulation/dategrid.hpp>
#include <orea/simulation/simmarket.hpp>
#include <ored/model/modelbuilder.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/utilities/progressbar.hpp>

#include <map>
#include <set>

using std::set;

namespace ore {
namespace analytics {

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
        const boost::shared_ptr<analytics::DateGrid>& dg,
        //! Simulated market object
        const boost::shared_ptr<analytics::SimMarket>& simMarket,
        //! model builders to be updated
        const set<boost::shared_ptr<data::ModelBuilder>>& modelBuilders = set<boost::shared_ptr<data::ModelBuilder>>());

    //! Build NPV cube
    void buildCube(
        //! Portfolio to be priced
        const boost::shared_ptr<data::Portfolio>& portfolio,
        //! Object for storing the resulting NPV cube
        boost::shared_ptr<analytics::NPVCube>& outputCube,
        //! Calculators to use
        std::vector<boost::shared_ptr<ValuationCalculator>> calculators);

private:
    QuantLib::Date today_;
    boost::shared_ptr<analytics::DateGrid> dg_;
    boost::shared_ptr<analytics::SimMarket> simMarket_;
    set<boost::shared_ptr<data::ModelBuilder>> modelBuilders_;
};
}
}
