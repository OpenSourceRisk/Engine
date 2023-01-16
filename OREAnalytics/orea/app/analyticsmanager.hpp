/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

/*! \file orea/app/analyticsmanager.hpp
    \brief ORE Analytics Manager
*/

#pragma once

#include <orea/app/analytic.hpp>
#include <orea/app/marketcalibrationreport.hpp>
#include <orea/app/marketdataloader.hpp>
#include <orea/app/inputparameters.hpp>

#include <iostream>

namespace ore {
namespace analytics {

class AnalyticsManager {
public:
    AnalyticsManager(//! Container for the inputs required by the standard analytics
                     const boost::shared_ptr<InputParameters>& inputs,
                     //! A market data loader object that can retrieve required data from a large repository
                     const boost::shared_ptr<MarketDataLoader>& marketDataLoader,
                     //! Stream for optional output
                     std::ostream& out = std::cout);
    virtual ~AnalyticsManager() {};

    //! Valid analytics in the base analytics manager are the union of analytics types provided by analytics_ map
    bool isValidAnalytic(const std::string& type);
    const std::vector<std::string>& validAnalytics();

    void runAnalytics(const std::vector<std::string>& runTypes,
                      const boost::shared_ptr<MarketCalibrationReport>& marketCalibrationReport = nullptr);

    void addAnalytic(const std::string& label, const boost::shared_ptr<Analytic>& analytic);

    Analytic::analytic_reports const reports();
    Analytic::analytic_npvcubes const npvCubes();
    Analytic::analytic_mktcubes const mktCubes();

private:
    std::map<std::string, boost::shared_ptr<Analytic>> analytics_;
    boost::shared_ptr<InputParameters> inputs_;
    boost::shared_ptr<MarketDataLoader> marketDataLoader_;
    Analytic::analytic_reports marketDataReports_;
    std::vector<std::string> validAnalytics_;
    std::ostream& out_;
};

boost::shared_ptr<AnalyticsManager> parseAnalytics(const std::string& s,
    const boost::shared_ptr<InputParameters>& inputs,
    const boost::shared_ptr<MarketDataLoader>& marketDataLoader);

} // analytics
} // ore
