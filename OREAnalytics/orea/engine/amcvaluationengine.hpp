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

/*! \file engine/amcvaluationengine.hpp
    \brief valuation engine for amc
    \ingroup engine
*/

#pragma once

#include <orea/cube/npvcube.hpp>
#include <orea/scenario/scenariogeneratordata.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/utilities/progressbar.hpp>

#include <qle/models/crossassetmodel.hpp>

namespace ore {
namespace analytics {

using std::string;

//! AMC Valuation Engine
class AMCValuationEngine : public ore::data::ProgressReporter {
public:
    //! Constructor
    AMCValuationEngine(const boost::shared_ptr<QuantExt::CrossAssetModel>& model,
                       const boost::shared_ptr<ore::analytics::ScenarioGeneratorData>& sgd,
                       const boost::shared_ptr<ore::data::Market>& market, const std::vector<string>& aggDataIndices,
                       const std::vector<string>& aggDataCurrencies);
    //! Build NPV cube
    void buildCube(
        //! Portfolio to be priced
        const boost::shared_ptr<ore::data::Portfolio>& portfolio,
        //! Object for storing the resulting NPV cube
        boost::shared_ptr<ore::analytics::NPVCube>& outputCube);

    //! Set aggregation data
    boost::shared_ptr<ore::analytics::AggregationScenarioData>& aggregationScenarioData() { return asd_; }
    //! Get aggregation data
    const boost::shared_ptr<ore::analytics::AggregationScenarioData>& aggregationScenarioData() const { return asd_; }

private:
    // Only for the case of grids with close-out lag and mpor mode sticky date:
    // If processCloseOutDates is true, filter the path on the close out dates and move the close-out times to the
    // valuation times. If processCloseOutDates is false, filter the path on the valuation dates.
    QuantExt::MultiPath effectiveSimulationPath(const QuantExt::MultiPath& p, const bool processCloseOutDates) const;

    const boost::shared_ptr<QuantExt::CrossAssetModel> model_;
    const boost::shared_ptr<ore::analytics::ScenarioGeneratorData> sgd_;
    const boost::shared_ptr<ore::data::Market> market_;
    const std::vector<string> aggDataIndices_, aggDataCurrencies_;

    boost::shared_ptr<ore::analytics::AggregationScenarioData> asd_;
};
} // namespace analytics
} // namespace ore
