/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of OpenRiskEngine, a free-software/open-source library
 for transparent pricing and risk analysis - http://openriskengine.org

 OpenRiskEngine is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program; if not, please email
 <users@openriskengine.org>. The license is also available online at
 <http://openriskengine.org/license.shtml>.

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

#include <orea/simulation/dategrid.hpp>
#include <orea/simulation/simmarket.hpp>
#include <orea/cube/npvcube.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/utilities/progressbar.hpp>
#include <orea/engine/valuationcalculator.hpp>

#include <map>

namespace openriskengine {
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
class ValuationEngine : public openriskengine::data::ProgressReporter {
public:
    //! Constructor
    ValuationEngine(
        //! Valuation date
        const QuantLib::Date& today,
        //! Simulation date grid
        const boost::shared_ptr<analytics::DateGrid>& dg,
        //! Simulated market object
        const boost::shared_ptr<analytics::SimMarket>& simMarket);

    //! Build NPV cube
    void buildCube(
        //! Portfolio to be priced
        const boost::shared_ptr<data::Portfolio>& portfolio,
        //! Object for storing the resulting NPV cube
        boost::shared_ptr<analytics::NPVCube>& outputCube,
        //! Calculators to use
        std::vector<boost::shared_ptr<ValuationCalculator>> calculators);

    using FixingsMap = std::map<std::string, std::vector<QuantLib::Date>>;

private:
    typedef std::vector<std::vector<std::string>> FlowList;
    static FixingsMap getFixingDates(const std::vector<FlowList>& flowList);

    QuantLib::Date today_;
    boost::shared_ptr<analytics::DateGrid> dg_;
    boost::shared_ptr<analytics::SimMarket> simMarket_;
};
}
}
