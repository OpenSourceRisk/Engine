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
                     const QuantLib::ext::shared_ptr<InputParameters>& inputs,
                     //! A market data loader object that can retrieve required data from a large repository
                     const QuantLib::ext::shared_ptr<MarketDataLoader>& marketDataLoader);
    virtual ~AnalyticsManager() {};

    //! Valid analytics in the analytics manager are the union of analytics types provided by analytics_ map
    bool hasAnalytic(const std::string& type);
    const std::set<std::string>& validAnalytics();
    const std::set<std::string>& requestedAnalytics();
    const QuantLib::ext::shared_ptr<Analytic>& getAnalytic(const std::string& type) const;
    Size numberOfAnalytics() { return analytics_.size(); }
    const QuantLib::ext::shared_ptr<InputParameters>& inputs() { return inputs_; }
    std::vector<QuantLib::ext::shared_ptr<ore::data::TodaysMarketParameters>> todaysMarketParams();
    void runAnalytics(const QuantLib::ext::shared_ptr<MarketCalibrationReportBase>& marketCalibrationReport = nullptr);
    void addAnalytic(const std::string& label, const QuantLib::ext::shared_ptr<Analytic>& analytic);

    // returns a vector of all analytics, including dependent analytics
    std::map<std::string, QuantLib::ext::shared_ptr<Analytic>> analytics() { return analytics_; }
    void clear();
    
    Analytic::analytic_reports const reports();
    Analytic::analytic_npvcubes const npvCubes();
    Analytic::analytic_mktcubes const mktCubes();
    Analytic::analytic_stresstests const stressTests();
    
    // Write all reports to files, reportNames map can be used to replace standard report names
    // with custom names
    void toFile(const Analytic::analytic_reports& reports, const std::string& outputPath,
                const std::map<std::string, std::string>& reportNames = {}, const char sep = ',',
                const bool commentCharacter = false, char quoteChar = '\0', const string& nullString = "#N/A",
                const std::set<std::string>& lowerHeaderReportNames = {});

private:
    std::map<std::string, QuantLib::ext::shared_ptr<Analytic>> analytics_;
    QuantLib::ext::shared_ptr<InputParameters> inputs_;
    QuantLib::ext::shared_ptr<MarketDataLoader> marketDataLoader_;
    Analytic::analytic_reports reports_;
    std::set<std::string> validAnalytics_;
};

QuantLib::ext::shared_ptr<AnalyticsManager> parseAnalytics(const std::string& s,
    const QuantLib::ext::shared_ptr<InputParameters>& inputs,
    const QuantLib::ext::shared_ptr<MarketDataLoader>& marketDataLoader);

} // analytics
} // ore
